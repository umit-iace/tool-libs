/** @file Min.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <stdint.h>
#include "utils/Queue.h"
#include "x/FrameRegistry.h"

/** simple CRC32 implementation */
struct CRC32 {
    /** state */
    uint32_t checksum{0xffffffff};
    /** reinitialize state */
    void init() {
        *this = CRC32{};
    }
    /** calculate step with given byte */
    void step(uint8_t byte) {
        checksum ^= byte;
        for (uint32_t j = 0; j < 8; j++) {
            uint32_t mask = (uint32_t) -(checksum & 1U);
            checksum = (checksum >> 1) ^ (0xedb88320U & mask);
        }
    }
    /** get final CRC32 value */
    uint32_t finalize() {
        return ~checksum;
    }
};

namespace MIN {
    // Special protocol bytes
    enum {
        HEADER_BYTE = 0xaaU,
        STUFF_BYTE = 0x55U,
        EOF_BYTE = 0x55U,
    };
}


/** full min-based connection wrapper */
struct Min {
    /** incoming Min stream
     * 
     * check data availability from underlying Buffer stream with
     * `empty()` then `pop()` and use data
     * ```
     * while (!in.empty()) {
     *  Frame f = in.pop();
     *  ...
     * }
     * ```
     *
     * \dot
     * digraph {
     *  rankdir=RL;
     *  Frame [style=dashed, URL="\ref Frame"]
     *  Buffer [style=dashed, URL="\ref Buffer"]
     *  MinIn [URL="\ref MinIn"]
     *  Buffer -> MinIn [label=empty, URL="\ref empty"]
     *  MinIn -> Frame [label=pop, URL="\ref pop"]
     * }
     * \enddot
     **/
    class MinIn : public Source<Frame> {
        CRC32 crc{};
        Frame frame{};
        Queue<Frame> queue{20};
        Source<Buffer<uint8_t>> &source;
        uint8_t header_seen{0}, frame_length{0};
        uint32_t frame_crc{0};
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
        } state {};
        void byte(uint8_t b) {
            // three header bytes always mean "start of frame" and will
            // reset the frame buffer and be ready to receive frame data
            //
            // two in a row during the frame means to expect a stuff byte.

            if (header_seen == 2) {
                header_seen = 0;
                if (b == MIN::HEADER_BYTE) {
                    state = RECEIVING_ID_CONTROL;
                    return;
                }
                if (b == MIN::STUFF_BYTE) {
                    // discard this byte
                    return;
                } else {
                    // something has gone wrong, give up
                    state = SEARCHING_FOR_SOF;
                    return;
                }
            }

            if (b == MIN::HEADER_BYTE) {
                header_seen++;
            } else {
                header_seen = 0;
            }

            switch (state) {
                case SEARCHING_FOR_SOF:
                    // handled at the header byte site
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
                        state = RECEIVING_PAYLOAD;
                    } else {
                        state = RECEIVING_CHECKSUM_3;
                    }
                    break;
                case RECEIVING_PAYLOAD:
                    frame.b.append(b);
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
                    if (frame_crc == crc.finalize()) {
                        // Frame received OK, pass up data to handler
                        queue.push(std::move(frame));
                        frame = Frame{};
                    }
                    // Either the frame failed, or we already handled it
                    // anyway we can start looking for the next frame,
                    // we don't have to explicitly wait for the EOF
                    state = SEARCHING_FOR_SOF;
                    break;
                case RECEIVING_EOF:
                    // fallthrough
                default:
                    state = SEARCHING_FOR_SOF;
                    break;
            }
        }
    public:
        /** unwrap given Buffer stream into Frame */
        MinIn(Source<Buffer<uint8_t>> &from) : source{from} { }
        /** check if Frame available */
        bool empty() override {
            while (!source.empty()) {
                for (auto b: source.pop()) {
                    byte(b);
                }
            }
            return queue.empty();
        }

        /** get available Frame
         *
         * always guard by if(! empty())
         */
        Frame pop() override {
            return queue.pop();
        }
    };
    /** outgoing Min stream
     * 
     * pushes data out directly to underlying Buffer stream
     *
     * \dot
     * digraph {
     *  rankdir=LR;
     *  Frame [style=dashed, URL="\ref Frame"]
     *  Buffer [style=dashed, URL="\ref Buffer"]
     *  MinOut [URL="\ref MinOut"]
     *  Frame -> MinOut [label=push, URL="\ref push"]
     *  MinOut -> Buffer [label=push, URL="\ref push"]
     * }
     * \enddot
     **/
    class MinOut : public Sink<Frame> {
        CRC32 crc{};
        Buffer<uint8_t> req{128};
        Sink<Buffer<uint8_t>> &out;
        uint8_t header_countdown = 2;
        void stuff(uint8_t b) {
            req.append(b);
            crc.step(b);

            // See if an additional stuff byte is needed
            if (b == MIN::HEADER_BYTE) {
                if (--header_countdown == 0) {
                    req.append(MIN::STUFF_BYTE);
                    header_countdown = 2U;
                }
            } else {
                header_countdown = 2U;
            }
        }
        void nostuff(uint8_t b) {
            req.append(b);
        }
    public:
        /** create Buffer stream wrapper */
        MinOut(Sink<Buffer<uint8_t>> &to) : out{to} { }
        using Sink<Frame>::push;
        /** push Frame through to underlying Buffer stream */
        void push(Frame &&f) override {
            req = Buffer<uint8_t>{128};
            crc.init();
            nostuff(MIN::HEADER_BYTE);
            nostuff(MIN::HEADER_BYTE);
            nostuff(MIN::HEADER_BYTE);
            stuff(f.id);
            stuff(f.b.len);
            for (size_t i = 0; i < f.b.len; ++i) {
                stuff(f.b.at(i));
            }
            uint32_t sum = crc.finalize();
            stuff((uint8_t) ((sum >> 24) & 0xff));
            stuff((uint8_t) ((sum >> 16) & 0xff));
            stuff((uint8_t) ((sum >> 8) & 0xff));
            stuff((uint8_t) ((sum >> 0) & 0xff));
            nostuff(MIN::EOF_BYTE);
            out.push(req);
        }
    };
    /** incoming Frame stream */
    MinIn in;
    /** outgoing Frame stream */
    MinOut out;
    /** Frame registry for this connection */
    FrameRegistry reg;
    /** dispatch incoming Frames through registry */
    void poll(uint32_t, uint32_t) {
        while (!in.empty()) {
            reg.handle(in.pop());
        }
    };
};
