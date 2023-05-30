#include "host.h"

#include <core/kern.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
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

void sighandler(int signum);

struct impl {
    struct Pipe {
        union {
            struct {
                int read, write;
            };
            int fd[2];
        };
        Pipe() {
            pipe(fd);
        }
    } p_in{}, p_out{};
    int c_pid {fork()};
    impl() {
        if (!c_pid) {
            // make child communicate with parent through pipes
            close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
            dup2(p_in.write, STDOUT_FILENO);
            dup2(p_out.read, STDIN_FILENO);
            close(p_in.read); close(p_in.write);
            close(p_out.read); close(p_out.write);
            while (true) {
                system("ncat -lui1 127.0.0.1 45670");
            }
        }
        close(p_in.write); close(p_out.read);
        struct sigaction act{};
        act.sa_handler = sighandler;
        sigfillset(&act.sa_mask);
        sigaction(SIGINT, &act, NULL);
    }
    ~impl() {
        if (c_pid) {
            kill(c_pid, 15);
            int stat;
            waitpid(c_pid, &stat, 0);
        }
    }
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

    READ readSock{p_in.read}, readTTY{STDIN_FILENO};
    WRITE writeSock{p_out.write}, writeTTY{STDOUT_FILENO};
} impl;

void sighandler(int signum) {
    exit(EXIT_SUCCESS);
}

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
