#pragma once

#include "utils/Queue.h"
#include "utils/Buffer.h"

// simple example of a pipe that receives bytes as they come in and splits them
// into '\n' delimited lines.
class LineFilter : public Source<Buffer<uint8_t>> {
    static constexpr size_t linelen = 1024;
    Queue<Buffer<uint8_t>> q;
    Buffer<uint8_t> l{linelen}; // stash
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
            l = Buffer<uint8_t>{linelen};
            return;
        }
        if (l.len + 1 > linelen) {
            // XXX: failure: line was longer than our buffer
            // Drop Line
            l = Buffer<uint8_t>{linelen};
        }
        l.append(b);
    }
public:
    LineFilter(Source<Buffer<uint8_t>> &p): source(p) { }
    bool empty() override {
        while (!source.empty()) {
            for (auto b: source.pop()) {
                recv(b);
            }
        }
        return q.empty();
    }
    Buffer<uint8_t> pop() override { return q.pop(); }
};
class LineDelimiter : public Sink<Buffer<uint8_t>> {
    Sink<Buffer<uint8_t>> &p;
public:
    LineDelimiter(Sink<Buffer<uint8_t>> &p): p(p) { }
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
