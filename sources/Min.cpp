#include "utils/Min.h"

void Min::crc32_init_context(uint32_t &checksum) {
    checksum = 0xffffffffU;
}

void Min::crc32_step(uint32_t &checksum, uint8_t byte) {
    checksum ^= byte;
    for (uint32_t j = 0; j < 8; j++) {
        uint32_t mask = (uint32_t) -(checksum & 1U);
        checksum = (checksum >> 1) ^ (0xedb88320U & mask);
    }
}

uint32_t Min::crc32_finalize(uint32_t &checksum) {
    return ~checksum;
}

void Min::stuffed_tx_byte(uint8_t byte) {
    // Transmit the byte
    tx_byte(byte);
    crc32_step(this->tx_checksum, byte);

    // See if an additional stuff byte is needed
    if (byte == HEADER_BYTE) {
        if (--this->tx_header_byte_countdown == 0) {
            tx_byte(STUFF_BYTE);        // Stuff byte
            this->tx_header_byte_countdown = 2U;
        }
    } else {
        this->tx_header_byte_countdown = 2U;
    }
}

void Min::on_wire_bytes(uint8_t id_control, uint8_t seq, const uint8_t *payload_base, uint16_t payload_offset,
                        uint16_t payload_mask, uint8_t payload_len) {
    uint8_t n;
    uint32_t checksum;

    this->tx_header_byte_countdown = 2U;
    crc32_init_context(this->tx_checksum);

    // Header is 3 bytes; because unstuffed will reset receiver immediately
    tx_byte(HEADER_BYTE);
    tx_byte(HEADER_BYTE);
    tx_byte(HEADER_BYTE);

    stuffed_tx_byte(id_control);
    if (id_control & TRANSPORT_FRAME) {
        // Send the sequence number if it is a transport frame
        stuffed_tx_byte(seq);
    }

    stuffed_tx_byte(payload_len);

    for (n = payload_len; n > 0; n--) {
        stuffed_tx_byte(payload_base[payload_offset]);
        payload_offset++;
        payload_offset &= payload_mask;
    }

    checksum = crc32_finalize(this->tx_checksum);

    // Network order is big-endian. A decent C compiler will spot that this
    // is extracting bytes and will use efficient instructions.
    stuffed_tx_byte((uint8_t) ((checksum >> 24) & 0xffU));
    stuffed_tx_byte((uint8_t) ((checksum >> 16) & 0xffU));
    stuffed_tx_byte((uint8_t) ((checksum >> 8) & 0xffU));
    stuffed_tx_byte((uint8_t) ((checksum >> 0) & 0xffU));

    // Ensure end-of-frame doesn't contain 0xaa and confuse search for start-of-frame
    tx_byte(EOF_BYTE);
}

#ifdef TRANSPORT_PROTOCOL
// Return the nth frame in the FIFO
struct TransportFrame *Min::transport_fifo_get(uint8_t n) {
    uint8_t idx = this->transportFifo.head_idx;
    return &this->transportFifo.frames[(idx + n) & transportFifo_max_frames_mask];
}

// Sends the given frame to the serial line
void Min::transport_fifo_send(struct TransportFrame *frame) {
    on_wire_bytes(frame->min_id | (uint8_t) TRANSPORT_FRAME, frame->seq, payloads_ring_buffer, frame->payload_offset,
                  transportFifo_max_frame_data_mask, frame->payload_len);
    frame->last_sent_time_ms = transportFifo.now;
}

// We don't queue an ACK frame - we send it straight away (if there's space to do so)
void Min::send_ack() {
    // In the embedded end we don't reassemble out-of-order frames and so never ask for retransmits. Payload is
    // always the same as the sequence number.
    if (on_wire_size(0) <= tx_space()) {
        on_wire_bytes(ACK, this->transportFifo.rn, &this->transportFifo.rn, 0, 0, 1U);
        this->transportFifo.last_sent_ack_time_ms = transportFifo.now;
    }
}

// We don't queue an RESET frame - we send it straight away (if there's space to do so)
void Min::send_reset() {
    if (on_wire_size(0) <= tx_space()) {
        on_wire_bytes(RESET, 0, 0, 0, 0, 0);
    }
}

void TransportFIFO::reset() {
    // Clear down the transmission FIFO queue
    this->n_frames = 0;
    this->head_idx = 0;
    this->tail_idx = 0;
    this->n_ring_buffer_bytes = 0;
    this->ring_buffer_tail_offset = 0;
    this->sn_max = 0;
    this->sn_min = 0;
    this->rn = 0;

    // Reset the timers
    this->last_received_anything_ms = now;
    this->last_sent_ack_time_ms = now;
    this->last_received_frame_ms = 0;
}

void Min::transport_reset(bool inform_other_side) {
    if (inform_other_side) {
        // Tell the other end we have gone away
        send_reset();
    }

    // Throw our frames away
    transportFifo.reset();
}

// Queues a MIN ID / iPayload frame into the outgoing FIFO
// Returns true if the frame was queued OK.
bool Min::queue_frame(uint8_t min_id, const uint8_t *iPayload, uint8_t payload_len) {
    struct TransportFrame *frame = transport_fifo_push(payload_len); // Claim a FIFO slot, reserve space for iPayload

    // We are just queueing here: the poll() function puts the frame into the window and on to the wire
    if (frame != 0) {
        // Copy frame details into frame slot, copy payload into ring buffer
        frame->min_id = min_id & (uint8_t) 0x3fU;
        frame->payload_len = payload_len;

        uint16_t payload_offset = frame->payload_offset;
        for (uint32_t i = 0; i < payload_len; i++) {
            payloads_ring_buffer[payload_offset] = iPayload[i];
            payload_offset++;
            payload_offset &= transportFifo_max_frame_data_mask;
        }
        return true;
    } else {
        this->transportFifo.dropped_frames++;
        return false;
    }
}

// Finds the frame in the window that was sent least recently
struct TransportFrame *Min::find_retransmit_frame() {
    uint8_t window_size = this->transportFifo.sn_max - this->transportFifo.sn_min;

    // Start with the head of the queue and call this the oldest
    struct TransportFrame *oldest_frame = &this->transportFifo.frames[this->transportFifo.head_idx];
    uint32_t oldest_elapsed_time = transportFifo.now - oldest_frame->last_sent_time_ms;

    uint8_t idx = this->transportFifo.head_idx;
    for (uint8_t i = 0; i < window_size; i++) {
        uint32_t elapsed = transportFifo.now - this->transportFifo.frames[idx].last_sent_time_ms;
        if (elapsed > oldest_elapsed_time) { // Strictly older only; otherwise the earlier frame is deemed the older
            oldest_elapsed_time = elapsed;
            oldest_frame = &this->transportFifo.frames[idx];
        }
        idx++;
        idx &= transportFifo_max_frames_mask;
    }

    return oldest_frame;
}
#endif

// This runs the receiving half of the transport protocol, acknowledging frames received, discarding
// duplicates received, and handling RESET requests.
void Min::valid_frame_received() {
    uint8_t id_control = this->rx_frame_id_control;
    uint8_t *payload = this->rx_frame_payload_buf;
    uint8_t payload_len = this->rx_control;

#ifdef TRANSPORT_PROTOCOL
    uint8_t seq = this->rx_frame_seq;

    uint8_t num_acked;
    uint8_t num_nacked;
    uint8_t num_in_window;

    // When we receive anything we know the other end is still active and won't shut down
    this->transportFifo.last_received_anything_ms = transportFifo.now;

    switch (id_control) {
        case ACK:
            // If we get an ACK then we remove all the acknowledged frames with seq < rn
            // The payload byte specifies the number of NACKed frames: how many we want retransmitted because
            // they have gone missing.
            // But we need to make sure we don't accidentally ACK too many because of a stale ACK from an old session
            num_acked = seq - this->transportFifo.sn_min;
            num_nacked = payload[0] - seq;
            num_in_window = this->transportFifo.sn_max - this->transportFifo.sn_min;

            if (num_acked <= num_in_window && this->transportFifo.n_frames) {
                this->transportFifo.sn_min = seq;
                // Now pop off all the frames up to (but not including) rn
                // The ACK contains Rn; all frames before Rn are ACKed and can be removed from the window
                for (uint8_t i = 0; i < num_acked; i++) {
                    transport_fifo_pop();
                }
                uint8_t idx = this->transportFifo.head_idx;
                // Now retransmit the number of frames that were requested
                for (uint8_t i = 0; i < num_nacked; i++) {
                    struct TransportFrame *retransmit_frame = &this->transportFifo.frames[idx];
                    if (on_wire_size(retransmit_frame->payload_len) <= tx_space()) {
                        transport_fifo_send(retransmit_frame);
                    }
                    idx++;
                    idx &= transportFifo_max_frames_mask;
                }
            } else {
                this->transportFifo.spurious_acks++;
            }
            break;
        case RESET:
            // If we get a RESET demand then we reset the transport protocol (empty the FIFO, reset the
            // sequence numbers, etc.)
            // We don't send anything, we just do it. The other end can send frames to see if this end is
            // alive (pings, etc.) or just wait to get application frames.
            this->transportFifo.resets_received++;
            transport_reset(true);
            break;
        default:
            if (id_control & TRANSPORT_FRAME) {
                // Incoming application frames

                // Reset the activity time (an idle connection will be stalled)
                this->transportFifo.last_received_frame_ms = transportFifo.now;

                if (seq == this->transportFifo.rn) {
                    // Accept this frame as matching the sequence number we were looking for

                    // Now looking for the next one in the sequence
                    this->transportFifo.rn++;

                    // Always send an ACK back for the frame we received
                    // ACKs are short (should be about 9 microseconds to send on the wire) and
                    // this will cut the latency down.
                    // We also periodically send an ACK in case the ACK was lost, and in any case
                    // frames are re-sent.
                    send_ack();

                    // Now ready to pass this up to the application handlers

                    // Pass frame up to application handler to deal with
                    application_handler(id_control & (uint8_t) 0x3fU, payload);
                } else {
                    // Discard this frame because we aren't looking for it: it's either a dupe because it was
                    // retransmitted when our ACK didn't get through in time, or else it's further on in the
                    // sequence and others got dropped.
                    this->transportFifo.sequence_mismatch_drop++;
                }
            } else {
                // Not a transport frame
                application_handler(id_control & (uint8_t) 0x3fU, payload);
            }
            break;
    }
#else
    application_handler(id_control & (uint8_t) 0x3fU, payload);
#endif
}

void Min::rx_byte(uint8_t byte) {
    // Regardless of state, three header bytes means "start of frame" and
    // should reset the frame buffer and be ready to receive frame data
    //
    // Two in a row in over the frame means to expect a stuff byte.
    uint32_t crc;

    if (this->rx_header_bytes_seen == 2) {
        this->rx_header_bytes_seen = 0;
        if (byte == HEADER_BYTE) {
            this->rx_frame_state = RECEIVING_ID_CONTROL;
            return;
        }
        if (byte == STUFF_BYTE) {
            /* Discard this byte; carry on receiving on the next character */
            return;
        } else {
            /* Something has gone wrong, give up on this frame and look for header again */
            this->rx_frame_state = SEARCHING_FOR_SOF;
            return;
        }
    }

    if (byte == HEADER_BYTE) {
        this->rx_header_bytes_seen++;
    } else {
        this->rx_header_bytes_seen = 0;
    }

    switch (this->rx_frame_state) {
        case SEARCHING_FOR_SOF:
            break;
        case RECEIVING_ID_CONTROL:
            this->rx_frame_id_control = byte;
            this->rx_frame_payload_bytes = 0;
            crc32_init_context(this->rx_checksum);
            crc32_step(this->rx_checksum, byte);
            if (byte & TRANSPORT_FRAME) {
                this->rx_frame_state = RECEIVING_SEQ;
            } else {
                this->rx_frame_seq = 0;
                this->rx_frame_state = RECEIVING_LENGTH;
            }
            break;
        case RECEIVING_SEQ:
            this->rx_frame_seq = byte;
            crc32_step(this->rx_checksum, byte);
            this->rx_frame_state = RECEIVING_LENGTH;
            break;
        case RECEIVING_LENGTH:
            this->rx_frame_length = byte;
            this->rx_control = byte;
            crc32_step(this->rx_checksum, byte);
            if (this->rx_frame_length > 0) {
                // Can reduce the RAM size by compiling limits to frame sizes
                if (this->rx_frame_length <= this->iMaxPayload) {
                    this->rx_frame_state = RECEIVING_PAYLOAD;
                } else {
                    // Frame dropped because it's longer than any frame we can buffer
                    this->rx_frame_state = SEARCHING_FOR_SOF;
                }
            } else {
                this->rx_frame_state = RECEIVING_CHECKSUM_3;
            }
            break;
        case RECEIVING_PAYLOAD:
            this->rx_frame_payload_buf[this->rx_frame_payload_bytes++] = byte;
            crc32_step(this->rx_checksum, byte);
            if (--this->rx_frame_length == 0) {
                this->rx_frame_state = RECEIVING_CHECKSUM_3;
            }
            break;
        case RECEIVING_CHECKSUM_3:
            this->rx_frame_checksum = ((uint32_t) byte) << 24;
            this->rx_frame_state = RECEIVING_CHECKSUM_2;
            break;
        case RECEIVING_CHECKSUM_2:
            this->rx_frame_checksum |= ((uint32_t) byte) << 16;
            this->rx_frame_state = RECEIVING_CHECKSUM_1;
            break;
        case RECEIVING_CHECKSUM_1:
            this->rx_frame_checksum |= ((uint32_t) byte) << 8;
            this->rx_frame_state = RECEIVING_CHECKSUM_0;
            break;
        case RECEIVING_CHECKSUM_0:
            this->rx_frame_checksum |= byte;
            crc = crc32_finalize(this->rx_checksum);
            if (this->rx_frame_checksum != crc) {
                // Frame fails the checksum and so is dropped
                this->rx_frame_state = SEARCHING_FOR_SOF;
            } else {
                // Checksum passes, go on to check for the end-of-frame marker
                this->rx_frame_state = RECEIVING_EOF;
            }
            break;
        case RECEIVING_EOF:
            if (byte == 0x55u) {
                // Frame received OK, pass up data to handler
                valid_frame_received();
            }
            // else discard
            // Look for next frame */
            this->rx_frame_state = SEARCHING_FOR_SOF;
            break;
        default:
            // Should never get here but in case we do then reset to a safe state
            this->rx_frame_state = SEARCHING_FOR_SOF;
            break;
    }
}

#ifdef TRANSPORT_PROTOCOL
// Sends received bytes into a MIN context and runs the transport timeouts
void Min::poll() {
    transportFifo.now = time_ms();

    bool remote_connected = (transportFifo.now - this->transportFifo.last_received_anything_ms < idle_timeout_ms);
    bool remote_active = (transportFifo.now - this->transportFifo.last_received_frame_ms < idle_timeout_ms);

    // This sends one new frame or resends one old frame
    uint8_t window_size = this->transportFifo.sn_max - this->transportFifo.sn_min; // Window size
    if ((window_size < max_window_size) && (this->transportFifo.n_frames > window_size)) {
        // There are new frames we can send; but don't even bother if there's no buffer space for them
        struct TransportFrame *frame = transport_fifo_get(window_size);
        if (on_wire_size(frame->payload_len) < tx_space()) {
            frame->seq = this->transportFifo.sn_max;
            transport_fifo_send(frame);

            // Move window on
            this->transportFifo.sn_max++;
        }
    } else {
        // Sender cannot send new frames so resend old ones (if there's anyone there)
        if ((window_size > 0) && remote_connected && this->transportFifo.n_frames) {
            // There are unacknowledged frames. Can re-send an old frame. Pick the least recently sent one.
            struct TransportFrame *oldest_frame = find_retransmit_frame();
            if (transportFifo.now - oldest_frame->last_sent_time_ms >= frame_retransmit_timeout_ms) {
                // Resending oldest frame if there's a chance there's enough space to send it
                if (on_wire_size(oldest_frame->payload_len) <= tx_space()) {
                    transport_fifo_send(oldest_frame);
                }
            }
        }
    }
    // Periodically transmit the ACK with the rn value, unless the line has gone idle
    if (transportFifo.now - this->transportFifo.last_sent_ack_time_ms > ack_retransmit_timeout_ms) {
        if (remote_active) {
            send_ack();
        }
    }
}

// Pops frame from front of queue, reclaims its ring buffer space
void Min::transport_fifo_pop() {
    struct TransportFrame *frame = &this->transportFifo.frames[this->transportFifo.head_idx];

    this->transportFifo.n_frames--;
    this->transportFifo.head_idx++;
    this->transportFifo.head_idx &= transportFifo_max_frames_mask;
    this->transportFifo.n_ring_buffer_bytes -= frame->payload_len;
}

// Claim a buffer slot from the FIFO. Returns 0 if there is no space.
struct TransportFrame *Min::transport_fifo_push(uint8_t data_size) {
    // A frame is only queued if there aren't too many frames in the FIFO and there is space in the
    // data ring buffer.
    struct TransportFrame *ret = 0;
    if (this->transportFifo.n_frames < transportFifo_max_frames) {
        // Is there space in the ring buffer for the frame payload?
        if (this->transportFifo.n_ring_buffer_bytes + data_size <= transportFifo_max_frame_data) {
            this->transportFifo.n_frames++;
            // Create FIFO entry
            ret = &(this->transportFifo.frames[this->transportFifo.tail_idx]);
            ret->payload_offset = this->transportFifo.ring_buffer_tail_offset;

            // Claim ring buffer space
            this->transportFifo.n_ring_buffer_bytes += data_size;
            this->transportFifo.ring_buffer_tail_offset += data_size;
            this->transportFifo.ring_buffer_tail_offset &= transportFifo_max_frame_data_mask;

            // Claim FIFO space
            this->transportFifo.tail_idx++;
            this->transportFifo.tail_idx &= transportFifo_max_frames_mask;
        } else {
            //debug_print("No FIFO payload space: data_size=%d, n_ring_buffer_bytes=%d\n",
            //           data_size, this->transportFifo.n_ring_buffer_bytes);
        }
    } else {
        //debug_print("No FIFO frame slots\n");
    }
    return ret;
}
#endif

void Min::application_handler(uint8_t min_id, uint8_t *min_payload) {
    application_function(min_id, min_payload);
}
