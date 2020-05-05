// MIN Protocol v3.0.
//
// MIN is a lightweight reliable protocol for exchanging information from a microcontroller (MCU) to a host.
// It is designed to run on an 8-bit MCU but also scale up to more powerful devices. A typical use case is to
// send data from a UART on a small MCU over a UART-USB converter plugged into a PC host. A Python implementation
// of host code is provided (or this code could be compiled for a PC).
//
// MIN supports frames of 0-255 bytes (with a lower limit selectable at compile time to reduce RAM). MIN frames
// have identifier values between 0 and 63.
//
// A transport layer is compiled in. This provides sliding window reliable transmission of frames.
#ifndef MIN_H
#define MIN_H

#include <stdlib.h>

#include "utils/Transport.h"

class Min;

/**
 * Implementation of Min transmissions
 */
class MinImpl {
public:
#ifdef TRANSPORT_PROTOCOL
    /**
     * return current time in milliseconds
     */
    virtual uint32_t time_ms() = 0;
#endif

    /**
     * transmit one byte to wire
     */
    virtual void tx_byte(uint8_t byte) = 0;

    /**
     * current empty space in output buffer
     */
    virtual uint16_t tx_space() = 0;

    /**
     * connect implementation to MIN
     */
    virtual void connect(Min*) = 0;
};

#ifdef TRANSPORT_PROTOCOL
class TransportFrame {
public:
    uint32_t last_sent_time_ms = 0;     // used for re-send timeouts
    uint16_t payload_offset = 0;        // in the ring buffer
    uint8_t payload_len = 0;            // How big the payload is
    uint8_t min_id = 0;                 // ID of frame
    uint8_t seq = 0;                    // Sequence number of frame
};

class TransportFIFO {
public:
    TransportFIFO(uint8_t size) {
        frames = new TransportFrame[size]();
        now = 0;
    }

    void reset();

    struct TransportFrame *frames = nullptr;
    uint32_t last_sent_ack_time_ms;
    uint32_t last_received_anything_ms;
    uint32_t last_received_frame_ms;
    uint32_t dropped_frames;                // Diagnostic counters
    uint32_t spurious_acks;
    uint32_t sequence_mismatch_drop;
    uint32_t resets_received;
    uint16_t n_ring_buffer_bytes;           // Number of bytes used in the payload ring buffer
    uint16_t ring_buffer_tail_offset;       // Tail of the payload ring buffer
    uint8_t n_frames;                       // Number of frames in the FIFO
    uint8_t head_idx;                       // Where frames are taken from in the FIFO
    uint8_t tail_idx;                       // Where new frames are added
    uint8_t sn_min;                         // Sequence numbers for transport protocol
    uint8_t sn_max;
    uint8_t rn;
    uint32_t now;
};

#endif

// TODO: should the connection be moved to Implementation?
class Min : public Connection {
public:
    int send(uint8_t id, const uint8_t *payload, uint8_t len) override {
#ifdef TRANSPORT_PROTOCOL
        this->queue_frame(id, payload, len);
#else
        this->send_frame(id, payload, len);
#endif
        return 0;
    }

    void registerFrameHandler(void (*handler)(uint8_t id, const uint8_t *payload)) override {
        add_application_function(handler);
    }

    Min(MinImpl *imp, uint16_t maxPayload,
            uint16_t maxFrames = 1 << 4,
            uint16_t maxData = 1 << 10,
            uint16_t windowSize = 16) : impl(imp), iMaxPayload(maxPayload)
#ifdef TRANSPORT_PROTOCOL
                       , transportFifo(maxFrames)
    {
        // Counters for diagnosis purposes
        transportFifo.reset();

        this->payloads_ring_buffer = new uint8_t[maxData]();
        this->transportFifo_max_frames = maxFrames;
        this->transportFifo_max_frames_mask = maxFrames - 1;
        this->transportFifo_max_frame_data = maxData;
        this->transportFifo_max_frame_data_mask = maxData - 1;
        this->max_window_size = windowSize;
#else
        {
#endif
        imp->connect(this);
        // Initialize context
        rx_header_bytes_seen = 0;
        rx_frame_state = SEARCHING_FOR_SOF;

        this->rx_frame_payload_buf = new uint8_t[maxPayload]();      // Payload received so far

    }

    const uint8_t iMaxPayload;

private:
#ifdef TRANSPORT_PROTOCOL
    TransportFIFO transportFifo;           // T-MIN queue of outgoing frames
    uint32_t ack_retransmit_timeout_ms = 100;
    uint32_t frame_retransmit_timeout_ms = 20;
    uint32_t max_window_size = 16;
    uint32_t idle_timeout_ms = 3000;
    uint32_t transportFifo_max_frames_mask;
    uint32_t transportFifo_max_frame_data_mask;
    uint32_t transportFifo_max_frames = 16;
    uint32_t transportFifo_max_frame_data;

    uint8_t *payloads_ring_buffer;
#endif

    uint8_t *rx_frame_payload_buf;                  // Payload received so far
    uint32_t rx_frame_checksum;                     // Checksum received over the wire
    uint32_t rx_checksum;                           // Calculated checksum for receiving frame
    uint32_t tx_checksum;                           // Calculated checksum for sending frame
    uint8_t rx_header_bytes_seen;                   // Countdown of header bytes to reset state
    uint8_t rx_frame_state;                         // State of receiver
    uint8_t rx_frame_payload_bytes;                 // Length of payload received so far
    uint8_t rx_frame_id_control;                    // ID and control bit of frame being received
    uint8_t rx_frame_seq;                           // Sequence number of frame being received
    uint8_t rx_frame_length;                        // Length of frame
    uint8_t rx_control;                             // Control byte
    uint8_t tx_header_byte_countdown;               // Count out the header bytes

    // Special protocol bytes
    enum {
        HEADER_BYTE = 0xaaU,
        STUFF_BYTE = 0x55U,
        EOF_BYTE = 0x55U,
    };

    // Receiving state machine
    enum {
        SEARCHING_FOR_SOF,
        RECEIVING_ID_CONTROL,
        RECEIVING_SEQ,
        RECEIVING_LENGTH,
        RECEIVING_PAYLOAD,
        RECEIVING_CHECKSUM_3,
        RECEIVING_CHECKSUM_2,
        RECEIVING_CHECKSUM_1,
        RECEIVING_CHECKSUM_0,
        RECEIVING_EOF,
    };

    enum {
        // Top bit must be set: these are for the transport protocol to use
        // 0x7f and 0x7e are reserved MIN identifiers.
        ACK = 0xffU,
        RESET = 0xfeU,
        TRANSPORT_FRAME = 0x80U,
    };


    void crc32_init_context(uint32_t &checksum);

    void crc32_step(uint32_t &checksum, uint8_t byte);

    uint32_t crc32_finalize(uint32_t &checksum);

    void stuffed_tx_byte(uint8_t byte);

    void on_wire_bytes(uint8_t id_control, uint8_t seq, const uint8_t *payload_base, uint16_t payload_offset,
                       uint16_t payload_mask, uint8_t payload_len);

#ifdef TRANSPORT_PROTOCOL

    void transport_fifo_pop();

    TransportFrame *transport_fifo_push(uint8_t data_size);

    TransportFrame *transport_fifo_get(uint8_t n);

    void transport_fifo_send(TransportFrame *frame);

    void send_ack();

    void send_reset();

    void transport_fifo_reset();

    void transport_reset(bool inform_other_side);

#endif

public:

#ifdef TRANSPORT_PROTOCOL
    bool queue_frame(uint8_t min_id, const uint8_t *iPayload, uint8_t payload_len);

    void poll();

    TransportFrame *find_retransmit_frame();

    uint32_t time_ms() {
        return impl->time_ms();
    }

#endif
    void rx_byte(uint8_t byte);

    void tx_byte(uint8_t byte) {
        impl->tx_byte(byte);
    }

    uint16_t tx_space() {
        return impl->tx_space();
    }

    void application_handler(uint8_t min_id, uint8_t *min_payload);

    void add_application_function(
            void(*application_function)(uint8_t min_id, const uint8_t *min_payload)) {
        this->application_function = application_function;
    }

    void send_frame(uint8_t min_id, const uint8_t *payload, uint8_t payload_len) {
        if ((on_wire_size(payload_len) <= tx_space())) {
            this->on_wire_bytes(min_id & (uint8_t) 0x3fU, 0, payload, 0, 0xffffU, payload_len);
        }
    }

private:
    MinImpl *impl = nullptr;
    void (*application_function)(uint8_t min_id, const uint8_t *min_payload);

    void valid_frame_received();

    uint32_t on_wire_size(uint32_t p) {
        return p + 11U;
    }
};

#endif //MIN_H
