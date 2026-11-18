/** @file logger.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include "critical.h"
template<typename T>
struct DevNull: Sink<T>, Source<T> {
    bool empty() override { return true; }
    T pop() override { return T{}; }
    bool full() override { return false; }
    void push(T &&) override { }
};
/** black-hole sink & source, just drops all data, provides none */
template<typename T>
inline DevNull<T> devnull;

/** simple printf-style logging infrastructure */
struct Logger : Sink<Buffer<uint8_t>> {
    enum Lvl {NONE, INFO, WARN, ERROR};
    static constexpr const char *color[4] = {"\u001b[0m","\u001b[32m", "\u001b[31m", "\u001b[1;31m"};
    /** bit of `l` in the mask of \ref tap */
    static constexpr uint8_t bit(Lvl l) { return 1u << l; }
    /** additionally hand the raw messages of the levels in `mask` to `snk`
     *
     * as [lvl][text]: without prefix and color, nullptr removes the tap.
     * Only messages formatted by this Logger are tapped, not Buffers pushed
     * through it, so the messages of a Logger logging into another one aren't
     * tapped twice.
     */
    void tap(Sink<Buffer<uint8_t>> *snk, uint8_t mask) {
        tapSink = snk;
        tapMask = mask;
    }
    /** the sink everything is logged into */
    Sink<Buffer<uint8_t>> &out() { return *sink; }
    /** point the logger to another sink */
    void setSink(Sink<Buffer<uint8_t>> &snk) { sink = &snk; }

    virtual Buffer<uint8_t> pre() {
        return Buffer<uint8_t>(256);
    }
    /** space left in b, keeping `reserve` bytes free */
    static size_t room(const Buffer<uint8_t> &b, size_t reserve=0) {
        return b.size > b.len + reserve ? b.size - b.len - reserve : 0;
    }
    /** number of chars actually written by (v)snprintf into `room` bytes
     *
     * (v)snprintf returns the length it _would_ have written
     */
    static size_t written(int n, size_t room) {
        if (n < 0 || room == 0) return 0;
        return (size_t)n < room ? n : room - 1;
    }
    virtual void mklog(Lvl lvl, const char *fmt, va_list args) {
        Buffer<uint8_t> b = pre();
        // keep room for resetting the color, even if the message gets truncated
        size_t reserve = lvl != NONE ? strlen(color[NONE]) + 1 : 0;
        size_t r;
        if (lvl != NONE) {
            r = room(b, reserve);
            b.len += written(snprintf((char*)b.buf+b.len, r, "%s", color[lvl]), r);
        }
        size_t text = b.len;
        r = room(b, reserve);
        b.len += written(vsnprintf((char*)b.buf+b.len, r, fmt, args), r);
        if (tapSink && (tapMask & bit(lvl))) {
            Buffer<uint8_t> t(1 + b.len - text);
            t.append(lvl);
            for (size_t i = text; i < b.len; ++i) t.append(b.buf[i]);
            // never waits, the tap is an extra, the local sink is what counts
            if (tapSink->full()) ++tapDropped;
            else tapSink->push(std::move(t));
        }
        if (lvl != NONE) {
            r = room(b);
            b.len += written(snprintf((char*)b.buf+b.len, r, "%s", color[NONE]), r);
        }
        send(std::move(b));
    }
    /** hand b to the sink, waiting a little if it is full
     *
     * waiting only helps if something else drains the sink meanwhile, i.e.
     * not in an interrupt, not with interrupts disabled, not single threaded
     */
    void send(Buffer<uint8_t> &&b) {
        if (!CriticalSection::blocking()) {
            for (uint32_t spin = 0; spin < SPIN && sink->full(); ++spin) { }
        }
        if (sink->full()) {
            ++dropped;
            return;
        }
        sink->push(std::move(b));
    }
    /** polls of a full sink before giving up, ~tens of ms on a Cortex-M7 */
    static constexpr uint32_t SPIN = 1000000;
public:
    /** Info log level */
    void info(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        mklog(INFO, fmt, args);
        va_end(args);
    }
    /** Warn log level */
    void warn(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        mklog(WARN, fmt, args);
        va_end(args);
    }
    /** Error log level */
    void error(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        mklog(ERROR, fmt, args);
        va_end(args);
    }
    /** None log level */
    void print(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        mklog(NONE, fmt, args);
        va_end(args);
    }
    bool full() override {
        return sink->full();
    }
    /** push otherwisely prepared Buffer through Logger */
    void push(Buffer<uint8_t> &&b) override {
        Buffer<uint8_t> prefix = pre();
        Buffer<uint8_t> tmp(prefix.len + b.len);
        for (auto &c : prefix) { tmp.append(c); }
        for (auto &c : b) { tmp.append(c); }
        send(std::move(tmp));
    }
    /** messages lost because the sink was full */
    uint32_t dropped{};
    /** messages lost because the tap was full, kept apart from \ref dropped */
    uint32_t tapDropped{};
    /** create Logger wrapping a Buffer Sink */
    Logger(Sink<Buffer<uint8_t>> &snk) : sink{&snk} { }
    Logger() : sink{&devnull<Buffer<uint8_t>>} {}
    virtual ~Logger() = default;
private:
    Sink<Buffer<uint8_t>> *sink;
    Sink<Buffer<uint8_t>> *tapSink{};
    uint8_t tapMask{};
};

/** buffered logging facility
 *
 * not tapped (\ref Logger::tap): it collects several messages into one Buffer
 * and only sends them on flush, so they have no level of their own anymore
 */
struct BufferedLogger: public Logger {
    static constexpr int MAXLEN = 256;
    char buf[MAXLEN];
    size_t ix = 0;

    void put(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vput(fmt, args);
        va_end(args);
    }
    /** append formatted string, flush first if it doesn't fit anymore */
    void vput(const char *fmt, va_list args) {
        // args can only be traversed once, probe with a copy
        va_list probe;
        va_copy(probe, args);
        int need = vsnprintf(nullptr, 0, fmt, probe);
        va_end(probe);
        if (need < 0) return;
        if ((size_t)need >= MAXLEN - ix) {
            flush();
        }
        ix += written(vsnprintf(buf+ix, MAXLEN - ix, fmt, args), MAXLEN - ix);
    }
    void mklog(Lvl lvl, const char *fmt, va_list args) override {
        put("%s", color[lvl]);
        vput(fmt, args);
        put("%s", color[NONE]);
    }

public:
    BufferedLogger(Sink<Buffer<uint8_t>> &snk) : Logger{snk} { }
    BufferedLogger() : Logger{devnull<Buffer<uint8_t>>} {}
    void flush() {
        if (!ix) return;
        send(Buffer<uint8_t>((const uint8_t *)buf, ix));
        ix = 0;
    }
    bool full() override {
        // last byte is always taken by the terminating '\0'
        return ix >= MAXLEN - 1;
    }
};


