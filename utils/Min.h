#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <queue>
#include <utils/Buffer.h>
#include <utils/RequestQueue.h>
#include <utils/Queue.h>
#include <cstdio>

struct ByteHandler {
    virtual void recv(uint8_t b) = 0;
};

struct Frame {
    Buffer b{128};
    uint8_t id{};
    Frame(uint8_t id) : id(id) { }
    Frame() : id(0) { }

    struct {
        uint8_t pack, unpack;
    } cursor{};
    template<typename T>
    void pack(T value) {
        auto *origin = (uint8_t *) &value;
        assert(cursor.pack + sizeof(T) < b.size);
        b.len += sizeof(T);
        for (int i = sizeof(T) - 1; i >= 0; --i) {
            /* if (cursor.pack + i >= b.size) { */
            /*     return; */
            /* } */
            b[cursor.pack + i] = *origin++;
            /* b.payload[cursor.pack + i] = *origin++; */
        }
        cursor.pack += sizeof(T);
    }

    template<typename T>
    void unPack(T &value) {
        auto *dest = (uint8_t *) &value;
        assert(cursor.unpack + sizeof(T) < b.size);
        for (int i = sizeof(T) - 1; i >= 0; --i) {
            /* if (cursor.unpack + i >= b.size) { */
            /*     value = T{}; */
            /* } */
            *dest++ = b[cursor.unpack + i];
            /* *dest++ = b.payload[cursor.unpack + i]; */
        }
        cursor.unpack += sizeof(T);
    }
};

struct CRC32 {
    uint32_t checksum{0xffffffff};
    void init() {
        checksum = 0xffffffffU;
    }

    void step(uint8_t byte) {
        checksum ^= byte;
        for (uint32_t j = 0; j < 8; j++) {
            uint32_t mask = (uint32_t) -(checksum & 1U);
            checksum = (checksum >> 1) ^ (0xedb88320U & mask);
        }
    }

    uint32_t finalize() {
        return ~checksum;
    }
};

class Min : public ByteHandler {
public:
    Min(RequestQueue<Buffer> *txrq) : txrq(txrq) { }

    /* int send(uint8_t id, const uint8_t *payload, uint8_t len) { */
    /*     tx.req = new Buffer(128); */
    /*     txrq->request( */
    /*             tx.prepare(id & (uint8_t) 0x3fU, 0, payload, 0, 0xffffU, len) */
    /*             ); */
    /*     return 0; */
    /* } */
    void send(Frame &f) {
        // sending state
        struct {
            CRC32 crc;
            Buffer *req{new Buffer{128}};
            uint8_t header_countdown = 2;
            void stuff(uint8_t b) {
                req->append(b);
                crc.step(b);

                // See if an additional stuff byte is needed
                if (b == HEADER_BYTE) {
                    if (--header_countdown == 0) {
                        req->append(STUFF_BYTE);        // Stuff byte
                        header_countdown = 2U;
                    }
                } else {
                    header_countdown = 2U;
                }
            }
            void nostuff(uint8_t b) {
                req->append(b);
            }
        } tx{};
        tx.nostuff(HEADER_BYTE);
        tx.nostuff(HEADER_BYTE);
        tx.nostuff(HEADER_BYTE);
        tx.stuff(f.id);
        tx.stuff(f.b.len);
        for (size_t i = 0; i < f.b.len; ++i) {
            tx.stuff(f.b.at(i));
        }
        uint32_t sum = tx.crc.finalize();
        tx.stuff((uint8_t) ((sum >> 24) & 0xff));
        tx.stuff((uint8_t) ((sum >> 16) & 0xff));
        tx.stuff((uint8_t) ((sum >> 8) & 0xff));
        tx.stuff((uint8_t) ((sum >> 0) & 0xff));
        tx.nostuff(EOF_BYTE);
        txrq->request(tx.req);
    }

    void recv(uint8_t b) override {
        rx.byte(b);
    }

    Frame getFrame() {
        if (rx.queue.empty()) return Frame{};
        Frame ret = rx.queue.front();
        rx.queue.pop();
        return ret;
    }

private:
    RequestQueue<Buffer> *txrq;
    // Special protocol bytes
    enum {
        HEADER_BYTE = 0xaaU,
        STUFF_BYTE = 0x55U,
        EOF_BYTE = 0x55U,
    };
    // receiving state
    struct {
        CRC32 crc;
        /* Frame frame{}; */
        /* std::queue<Frame> queue; */
        Queue<Frame, 2> queue;
        uint8_t header_seen, frame_length;
        uint32_t frame_crc;
        // Receiving state machine
        enum State {
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
        } state;
        void byte(uint8_t b) {
            // XXX: we're doing stuff directly in the queue's memory.
            // don't!
            Frame &frame = queue.back();
            // Regardless of state, three header bytes means "start of frame" and
            // should reset the frame buffer and be ready to receive frame data
            //
            // Two in a row in over the frame means to expect a stuff byte.

            if (header_seen == 2) {
                header_seen = 0;
                if (b == HEADER_BYTE) {
                    state = RECEIVING_ID_CONTROL;
                    return;
                }
                if (b == STUFF_BYTE) {
                    /* Discard this byte; carry on receiving on the next character */
                    return;
                } else {
                    /* Something has gone wrong, give up on this frame and look for header again */
                    state = SEARCHING_FOR_SOF;
                    return;
                }
            }

            if (b == HEADER_BYTE) {
                header_seen++;
            } else {
                header_seen = 0;
            }

            switch (state) {
                case SEARCHING_FOR_SOF:
                    break;
                case RECEIVING_ID_CONTROL:
                    frame.id = b & (uint8_t) 0x3fU;
                    frame.b.len = 0;
                    crc.init();
                    crc.step(b);
                    state = RECEIVING_LENGTH;
                    break;
                case RECEIVING_LENGTH:
                    frame_length = b;
                    crc.step(b);
                    if (frame_length > 0) {
                        // Can reduce the RAM size by compiling limits to frame sizes
                        if (frame_length <= 80) {
                            state = RECEIVING_PAYLOAD;
                        } else {
                            // Frame dropped because it's longer than any frame we can buffer
                            state = SEARCHING_FOR_SOF;
                        }
                    } else {
                        state = RECEIVING_CHECKSUM_3;
                    }
                    break;
                case RECEIVING_PAYLOAD:
                    queue.back().b.append(b);
                    crc.step(b);
                    if (--frame_length == 0) {
                        state = RECEIVING_CHECKSUM_3;
                    }
                    break;
                case RECEIVING_CHECKSUM_3:
                    frame_crc = ((uint32_t) b) << 24;
                    state = RECEIVING_CHECKSUM_2;
                    break;
                case RECEIVING_CHECKSUM_2:
                    frame_crc |= ((uint32_t) b) << 16;
                    state = RECEIVING_CHECKSUM_1;
                    break;
                case RECEIVING_CHECKSUM_1:
                    frame_crc |= ((uint32_t) b) << 8;
                    state = RECEIVING_CHECKSUM_0;
                    break;
                case RECEIVING_CHECKSUM_0:
                    frame_crc |= b;
                    if (frame_crc != crc.finalize()) {
                        // Frame fails the checksum and so is dropped
                        state = SEARCHING_FOR_SOF;
                    } else {
                        // Checksum passes, go on to check for the end-of-frame marker
                        state = RECEIVING_EOF;
                    }
                    break;
                case RECEIVING_EOF:
                    if (b == EOF_BYTE) {
                        // Frame received OK, pass up data to handler
                        queue.push(frame);
                        /* frame = Frame{}; */
                        // XXX: reset frame?
                    }
                    // else discard
                    // Look for next frame */
                    state = SEARCHING_FOR_SOF;
                    break;
                default:
                    // Should never get here but in case we do then reset to a safe state
                    state = SEARCHING_FOR_SOF;
                    break;
            }
        }
    } rx{};
};
