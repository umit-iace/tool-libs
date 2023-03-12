#include "host.h"
#include "x/Kern.h"

#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <unistd.h>
using namespace std::chrono;

time_point start{steady_clock::now()};
auto dt{milliseconds{1}};
time_point next{steady_clock::now() + dt};

void Kernel::idle() {
    std::this_thread::sleep_until(next);
    auto now = steady_clock::now();
    next += dt;
    auto runtime = duration_cast<milliseconds>(now-start);
    this->tick(dt.count());
}

struct impl {
    FILE *tty = fopen("/dev/tty", "r+");
    ~impl() {fclose(tty);}
    struct READ: Source<Buffer<uint8_t>> {
        Queue<Buffer<uint8_t>> q{30};
        Buffer<uint8_t> b{512};
        int fd;
        READ(int fd) : fd(fd) { }
        bool empty() override {
            struct pollfd fds[1];
            int ret{};
            fds[0].fd = fd;
            fds[0].events = POLLIN;
            ssize_t l{};
            do {
                ret = poll(fds, 1, 0);
                if (ret <= 0) break;
                l = read(fd, b.buf, b.size);
                if (l == -1) break;
                b.len = l;
                if (l) {
                    q.push(std::move(b));
                    b = Buffer<uint8_t>{512};
                }
            } while (l == b.size);
            return q.empty();
        }
        Buffer<uint8_t> pop() override {
            return q.pop();
        }
    };
    struct WRITE: Sink<Buffer<uint8_t>> {
        int fd{};
        WRITE(int fd) : fd{fd} { }
        void push(Buffer<uint8_t> &&b) override {
            size_t off{};
            do {
                off += write(fd, b.buf+off, b.len-off);
            } while (off < b.len);
        }
    };

    READ readSock{fileno(stdin)}, readTTY{fileno(tty)};
    WRITE writeSock{fileno(stdout)}, writeTTY{fileno(tty)};
} impl;
Host host{
    .socket{
        .in = impl.readSock,
        .out = impl.writeSock,
    },
    .tty{
        .in = impl.readTTY,
        .out = impl.writeTTY,
    },
};
