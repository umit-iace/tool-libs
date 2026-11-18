/** @file fixedframe.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once
#include "frameregistry.h"

#include <stdint.h>
#include <utils/queue.h>

/** connection wrapper for Frames of fixed size, as pyWisp's TcpConnection sends them
 *
 * each Frame is 1 byte id followed by exactly `LEN` bytes payload. Shorter payloads
 * are padded with zeros, longer ones are dropped. There is no checksum and no
 * resynchronisation, meant for reliable byte streams such as TCP. Used like \ref Min.
 */
template<size_t LEN = 80>
struct FixedFrame {
    static_assert(LEN <= 128, "payload has to fit into a Frame");
    /** incoming Frame stream, received Frames carry the full `LEN` bytes payload */
    class In : public Source<Frame> {
        Frame frame{};
        bool id_seen{};
        Queue<Frame> queue{20};
        Source<Buffer<uint8_t>> &source;
        Buffer<uint8_t> pending; // received, not parsed yet
        size_t pos{};
        void byte(uint8_t b) {
            if (!id_seen) {
                frame.id = b;
                id_seen = true;
                return;
            }
            frame.b.append(b);
            if (frame.b.len == LEN) {
                queue.trypush(std::move(frame));
                frame = {};
                id_seen = false;
            }
        }
    public:
        /** unwrap given Buffer stream into Frame */
        In(Source<Buffer<uint8_t>> &from) : source{from} { }
        /** check if Frame available
         *
         * stops reading when the Frame queue is full, the rest of the
         * current Buffer is kept for the next call
         */
        bool empty() override {
            while (!queue.full()) {
                if (pos >= pending.len) {
                    if (source.empty()) break;
                    pending = source.pop();
                    pos = 0;
                    continue;
                }
                byte(pending.buf[pos++]);
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
    /** outgoing Frame stream, pushes padded Frames to the underlying Buffer stream */
    class Out : public Sink<Frame> {
        Sink<Buffer<uint8_t>> &out;
    public:
        /** create Buffer stream wrapper */
        Out(Sink<Buffer<uint8_t>> &to) : out{to} { }
        using Sink<Frame>::push;
        bool full() override {
            return out.full();
        }
        /** push Frame through to underlying Buffer stream, dropped if its payload exceeds `LEN` */
        void push(Frame &&f) override {
            if (f.b.len > LEN) return;
            Buffer<uint8_t> req(1 + LEN);
            req.append(f.id);
            for (size_t i = 0; i < LEN; ++i) {
                req.append(i < f.b.len ? f.b.buf[i] : 0);
            }
            out.trypush(std::move(req));
        }
    };
    /** incoming Frame stream */
    In in;
    /** outgoing Frame stream */
    Out out;
    /** Frame registry for this connection */
    FrameRegistry reg;
    /** dispatch incoming Frames through registry */
    void poll(uint32_t, uint32_t) {
        while (!in.empty()) {
            reg.handle(in.pop());
        }
    };
};
