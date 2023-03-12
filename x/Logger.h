#pragma once

#include <cstdio>
#include <cstdarg>
template<typename T>
struct DevNull: Sink<T> {
    void push(T &&) {
    }
};
template<typename T>
inline DevNull<T> devnull;

/** simple printf-style logging infrastructure */
struct Logger : Sink<Buffer<uint8_t>> {
    enum Lvl {NONE, INFO, WARN};
    const char *clr[3] = {"\u001b[0m","\u001b[32m", "\u001b[31m"};
    Sink<Buffer<uint8_t>> &out;
    virtual Buffer<uint8_t> init() {
        auto ret = Buffer<uint8_t>{256};
        ret.len = snprintf((char*)ret.buf, ret.size, "(@%ldms): ", k.time);
        return ret;
    }
    void mklog(Lvl lvl, const char *fmt, va_list args) {
        Buffer<uint8_t> b{init()};
        if (lvl != NONE) puts(b, clr[lvl]);
        b.len += vsnprintf((char*)b.buf+b.len, b.size-b.len, fmt, args);
        if (lvl != NONE) puts(b, clr[NONE]);
        out.push(std::move(b));
    }
    void puts(Buffer<uint8_t> &b, const char *s) {
        b.len += snprintf((char*)b.buf+b.len, b.size-b.len, s);
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
        Buffer<uint8_t> tmp{init()};
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
