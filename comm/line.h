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
    Buffer<uint8_t> l = linelen; // stash
    Source<Buffer<uint8_t>> &source;
    void recv(uint8_t b) {
        // handle both \n and \r\n newlines
        if (b == '\n' || b == '\r') {
            if (l.len == 0) return;
            if (!q.full()) {
                q.push(std::move(l));
            } else {
                // fallthrough: Drop Line, queue was full
            }
            l = linelen;
            return;
        }
        if (l.len + 1 > linelen) {
            // XXX: failure: line was longer than our buffer
            // Drop Line
            l = linelen;
        }
        l.append(b);
    }
public:
    /** set underlying Source */
    LineFilter(Source<Buffer<uint8_t>> &p): source(p) { }
    bool empty() override {
        while (!q.full() && !source.empty()) {
            for (auto b: source.pop()) {
                recv(b);
            }
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
