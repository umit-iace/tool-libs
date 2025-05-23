/** @file logger.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cstdio>
#include <cstdarg>
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
    enum Lvl {NONE, INFO, WARN};
    const char *color[3] = {"\u001b[0m","\u001b[32m", "\u001b[31m"};
    Sink<Buffer<uint8_t>> &out;

    virtual Buffer<uint8_t> pre() {
        return Buffer<uint8_t>(256);
    }
    virtual void mklog(Lvl lvl, const char *fmt, va_list args) {
        Buffer<uint8_t> b = pre();
        if (lvl != NONE) {
            b.len += snprintf((char*)b.buf+b.len, b.size-b.len, "%s", color[lvl]);
        }
        b.len += vsnprintf((char*)b.buf+b.len, b.size-b.len, fmt, args);
        if (lvl != NONE) {
            b.len += snprintf((char*)b.buf+b.len, b.size-b.len, "%s", color[NONE]);
        }
        while (out.full());
        out.trypush(std::move(b));
    }
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
    /** None log level */
    void print(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        mklog(NONE, fmt, args);
        va_end(args);
    }
    bool full() override {
        return out.full();
    }
    /** push otherwisely prepared Buffer through Logger */
    void push(Buffer<uint8_t> &&b) override {
        Buffer<uint8_t> tmp = pre();
        for (auto &c : b) { tmp.append(c); }
        out.push(std::move(tmp));
    }
    /** create Logger wrapping a Buffer Sink */
    Logger(Sink<Buffer<uint8_t>> &snk) : out{snk} { }
    Logger() : out{devnull<Buffer<uint8_t>>} {}
    void *operator new(size_t, Logger *where) {
        return where;
    }
};

/** buffered logging facility */
struct BufferedLogger: public Logger {
    static constexpr int MAXLEN = 256;
    char buf[MAXLEN];
    size_t ix = 0;

    void mklog(Lvl lvl, const char *fmt, va_list args) override {
        size_t need = snprintf(buf, 0, "%s", color[lvl]);
        if (need >= MAXLEN - ix) {
            flush();
        }
        ix += snprintf(buf+ix, MAXLEN - ix, "%s", color[lvl]);

        need = vsnprintf(buf, 0, fmt, args);
        if (need >= MAXLEN - ix) {
            flush();
        }
        ix += vsnprintf(buf+ix, MAXLEN - ix, fmt, args);

        need = snprintf(buf, 0, "%s", color[NONE]);
        if (need >= MAXLEN - ix) {
            flush();
        }
        ix += snprintf(buf+ix, MAXLEN - ix, "%s", color[NONE]);
    }

public:
    BufferedLogger(Sink<Buffer<uint8_t>> &snk) : Logger{snk} { }
    BufferedLogger() : Logger{devnull<Buffer<uint8_t>>} {}
    void flush() {
        if (!ix) return;
        out.push(std::move(Buffer<uint8_t>((const uint8_t *)buf, ix)));
        ix = 0;
    }
    bool full() override {
        return ix == MAXLEN;
    }
};


