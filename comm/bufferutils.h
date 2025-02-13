/** @file bufferutils.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

/** stream splitter
 *
 * pushes data out to two different sinks when pushed into
 * for data going the other direction, refer to SplitPull
 * \dot
 *  digraph {
 *  rankdir=LR;
 *  data [label="Data", style=dotted, URL="\ref Buffer"]
 *  left [style=dashed, URL="\ref Sink"]
 *  right [style=dashed, URL="\ref Sink"]
 *  SplitPush [URL="\ref SplitPush"]
 *  data -> SplitPush [label=push]
 *  SplitPush -> left [label=push]
 *  SplitPush -> right [label=push]
 * }
 * \enddot
 */
struct SplitPush : Sink<Buffer<uint8_t>> {
    Sink<Buffer<uint8_t>> &left, &right;
    bool full() override {
        return left.full() || right.full();
    }
    void push(Buffer<uint8_t> &&in) override {
        left.push(in);
        right.push(in);
    }
    /** set sinks for splitter */
    SplitPush(Sink<Buffer<uint8_t>> &left, Sink<Buffer<uint8_t>> &right) : left(left), right(right) {}
};

/** stream splitter
 *
 * provides two data Sources from a single one
 * for data going the other direction, refer to SplitPush
 * \dot
 *  digraph {
 *  rankdir=RL;
 *  data1 [label="Data", style=dotted, URL="\ref Buffer"]
 *  data2 [label="Data", style=dotted, URL="\ref Buffer"]
 *  S [style=dashed, URL="\ref Source"]
 *  a [style=dashed, URL="\ref Source"]
 *  b [style=dashed, URL="\ref Source"]
 *  SplitPull [URL="\ref SplitPull"]
 *  S -> SplitPull [label=empty]
 *  SplitPull -> a [label=empty]
 *  SplitPull -> b [label=empty]
 *  a -> data1 [label=pop]
 *  b -> data2 [label=pop]
 * }
 * \enddot
 */
struct SplitPull {
    struct splitter : Source<Buffer<uint8_t>>, Sink<Buffer<uint8_t>> {
        Source<Buffer<uint8_t>> &from;
        Queue<Buffer<uint8_t>> q{20};
        Sink<Buffer<uint8_t>> *other;
        bool empty() override {
            while (!q.full() && !from.empty()) {
                auto b = from.pop();
                other->push(b);
                q.push(std::move(b));
            }
            return q.empty();
        }
        Buffer<uint8_t> pop() override {
            return q.pop();
        }
        bool full() override {
            return q.full();
        }
        void push(Buffer<uint8_t> &&t) override {
            q.push(std::move(t));
        }
        splitter(Source<Buffer<uint8_t>> &from) : from(from) { }
    };
    /** split data Source */
    splitter a;
    /** split data Source */
    splitter b;
    /** set underlying data Source */
    SplitPull(Source<Buffer<uint8_t>> &f): a(f), b(f) {
        a.other = &b;
        b.other = &a;
    }
};

/** stream splitter
 *
 * provides data Source from data Source, but also splits data out into
 * given Sink
 * \dot
 *  digraph {
 *  rankdir=RL;
 *  U [label="from", style=dashed, URL="\ref Source"]
 *  P [label="Data", style=dotted, URL="\ref Buffer"]
 *  S [label="to", style=dashed, URL="\ref Sink"]
 *  Tee [URL="\ref Tee"]
 *  U -> Tee [label=empty, URL="\ref empty"]
 *  Tee -> P [label=pop, URL="\ref pop"]
 *  Tee -> S [label=empty, URL="\ref empty"]
 * }
 * \enddot
 */
struct Tee : Source<Buffer<uint8_t>> {
    Source<Buffer<uint8_t>> &from;
    Sink<Buffer<uint8_t>> &to;
    Queue<Buffer<uint8_t>> q{20};
    /** check source and push available data to sink */
    bool empty() override {
        while (!q.full() && !from.empty()) {
            auto b = from.pop();
            q.push(b);
            to.trypush(std::move(b));
        }
        return q.empty();
    }
    /** get available data */
    Buffer<uint8_t> pop() override {
        return q.pop();
    }
    /** set underlying source and sink */
    Tee(Source<Buffer<uint8_t>> &from, Sink<Buffer<uint8_t>> &to)
        : from(from), to(to) { }
};

/** convert Sink stream into printable hex
 *
 * useful for debugging low-level stuff
 */
struct Hexify : Sink<Buffer<uint8_t>> {
    static constexpr uint8_t map[]="0123456789abcdef";
    static constexpr size_t blen = 512*3;
    Sink<Buffer<uint8_t>> &down;
    Buffer<uint8_t> work = blen;
    bool full() override {
        return down.full();
    }
    void push(Buffer<uint8_t> &&in) override {
        for (auto b: in) {
            work.append({'\\', map[b >> 4], map[b & 0xf]});
        }
        down.push(std::move(work));
        work = blen;
    }
    Hexify(Sink<Buffer<uint8_t>> &down) : down(down) {}
};
