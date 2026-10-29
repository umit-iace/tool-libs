/** @file line.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/queue.h>

/** simple example of a pipe that receives bytes as they come in and splits them
 * into `\n` delimited lines.
 * also filters the newline out (both LF and CRLF style newlines)
 */
class LineFilter : public Source<Buffer<uint8_t>> {
    static constexpr size_t linelen = 128;
    Queue<Buffer<uint8_t>> q;
    Buffer<uint8_t> l = Buffer<uint8_t>(linelen); // stash
    Source<Buffer<uint8_t>> &source;
    Buffer<uint8_t> pending; // received, not split yet
    size_t pos{};
    void recv(uint8_t b) {
        // handle both \n and \r\n newlines
        if (b == '\n' || b == '\r') {
            if (l.len == 0) return;
            q.trypush(std::move(l));
            l = Buffer<uint8_t>(linelen);
            return;
        }
        if (l.len + 1 > linelen) {
            // XXX: failure: line was longer than our buffer
            // Drop Line
            l = Buffer<uint8_t>(linelen);
        }
        l.append(b);
    }
public:
    /** set underlying Source */
    LineFilter(Source<Buffer<uint8_t>> &p): source(p) { }
    /** stops reading when the line queue is full, the rest of the
     * current Buffer is kept for the next call */
    bool empty() override {
        while (!q.full()) {
            if (pos >= pending.len) {
                if (source.empty()) break;
                pending = source.pop();
                pos = 0;
                continue;
            }
            recv(pending.buf[pos++]);
        }
        return q.empty();
    }
    Buffer<uint8_t> pop() override { return q.pop(); }
};

/** simple pipe that will append a newline to every data packet */
class LineDelimiter : public Sink<Buffer<uint8_t>> {
    Sink<Buffer<uint8_t>> &p;
public:
    /** set underlying Sink */
    LineDelimiter(Sink<Buffer<uint8_t>> &p): p(p) { }
    bool full() override {
        return p.full();
    }
    void push(const Buffer<uint8_t> &b) override {
        if (b.len + 1 <= b.size) {
            push(std::move(Buffer<uint8_t>{b}));
        } else {
            push(std::move(Buffer<uint8_t>{b.buf, b.size, b.size+1}));
        }
    }
    void push(Buffer<uint8_t> &&b) override {
        if (b.len + 1 <= b.size) {
            p.push(std::move(b.append('\n')));
        } else {
            p.push(std::move(Buffer<uint8_t>{b.buf, b.size, b.size+1}.append('\n')));
        }
    }
};
