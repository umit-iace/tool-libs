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
    void push(T &&) override {
    }
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
    void mklog(Lvl lvl, const char *fmt, va_list args) {
        Buffer<uint8_t> b = pre();
        if (lvl != NONE) {
            b.len += snprintf((char*)b.buf+b.len, b.size-b.len, "%s", color[lvl]);
        }
        b.len += vsnprintf((char*)b.buf+b.len, b.size-b.len, fmt, args);
        if (lvl != NONE) {
            b.len += snprintf((char*)b.buf+b.len, b.size-b.len, "%s", color[NONE]);
        }
        out.push(std::move(b));
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
    /** push otherwisely prepared Buffer through Logger */
    void push(Buffer<uint8_t> &&b) {
        Buffer<uint8_t> tmp = pre();
        for (auto &c : b) { tmp.append(c); }
        out.push(std::move(tmp));
    }
    /** create Logger wrapping a Buffer Sink */
    Logger(Sink<Buffer<uint8_t>> &snk) : out{snk} { }
    Logger() : out{devnull<Buffer<uint8_t>>} {}
    void *operator new(size_t sz, Logger *where) {
        return where;
    }
};
