#include "sys/host.h"

#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <setjmp.h>
using namespace std::chrono;

time_point start{steady_clock::now()};
auto dt{microseconds{1000}};
time_point next{steady_clock::now() + dt};

void Kernel::idle() {
    std::this_thread::sleep_until(next);
    auto now = steady_clock::now();
    next += dt;
    auto runtime = duration_cast<milliseconds>(now-start);
    this->tick(1);
}

void Kernel::setTimeStep(uint16_t dt_us) {
    dt = microseconds{dt_us};
}

void sighandler(int signum) {
    k.exit(signum == SIGINT ? 0 : -1);
    if (signum == SIGSEGV) {
        /* fprintf(stderr, "detected SIGSEGV, attempting clean shutdown\n"); */
        longjmp(k.jbf, 1);
    }
}

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
            close(STDIN_FILENO); close(STDOUT_FILENO);
            dup2(p_in.write, STDOUT_FILENO);
            dup2(p_out.read, STDIN_FILENO);
            close(p_in.read); close(p_in.write);
            close(p_out.read); close(p_out.write);
            int ret = 0;
            while (ret != 0x7f00) {
                fprintf(stderr, "starting ncat\n");
                ret = system("ncat -lui1 127.0.0.1 45670");
            }
            exit(EXIT_FAILURE);
        }
        close(p_in.write); close(p_out.read);
        struct sigaction act{};
        act.sa_handler = sighandler;
        sigfillset(&act.sa_mask);
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGSEGV, &act, NULL);
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
        Buffer<uint8_t> b = 512;
        const int fd, pid;
        READ(int fd, int pid) : fd(fd), pid(pid) { }
        bool empty() override {
            int stat = 0;
            if (pid == waitpid(pid, &stat, WNOHANG) && WIFEXITED(stat))
                exit(EXIT_FAILURE);
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
                    b = 512;
                }
            } while (l == b.size);
            return q.empty();
        }
        Buffer<uint8_t> pop() override {
            return q.pop();
        }
    };
    struct WRITE: Sink<Buffer<uint8_t>> {
        const int fd{};
        WRITE(int fd) : fd{fd} { }
        void push(Buffer<uint8_t> &&b) override {
            size_t off{};
            do {
                off += write(fd, b.buf+off, b.len-off);
            } while (off < b.len);
        }
    };

    READ readSock{p_in.read, c_pid}, readTTY{STDIN_FILENO, c_pid};
    WRITE writeSock{p_out.write}, writeTTY{STDOUT_FILENO};
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
