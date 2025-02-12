#pragma once
#include <utils/queue.h>
#include <core/logger.h>

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

/** TTY communication backend */
struct TTY:
        public Sink<Buffer<uint8_t>>,
        public Source<Buffer<uint8_t>> {
    int fd{};
    static constexpr size_t BLEN = 512;
    Queue<Buffer<uint8_t>> tx, rx;
    Buffer<uint8_t> wrk = BLEN;
    /** open given path */
    TTY (const char *path) {
        fd = open(path, O_RDWR | O_NONBLOCK);
    }
    ~TTY() {
        close(fd);
    }
    bool full() override {
        return tx.full();
    }
    using Sink::push;
    void push(Buffer<uint8_t> &&b) override {
        tx.push(std::move(b));
        process();
    }
    bool empty() override {
        int l = read(fd, wrk.buf, wrk.size);
        if (l < 0) {
            if (errno != EAGAIN) {
                perror("reading on fd error");
            }
            return rx.empty();
        }
        wrk.len = l;
        rx.push(std::move(wrk));
        wrk = BLEN;
        return false;
    }
    Buffer<uint8_t> pop() override {
        return rx.pop();
    }
    void process() {
        //transmit side
        while (!tx.empty()) {
            auto msg = tx.front();
            int l = write(fd, msg.buf, msg.len);
            if (l < 0) {
                if (errno != EAGAIN) {
                    perror("writing to fd error");
                }
                break;
            } else if ((size_t)l < msg.len) {
                for (size_t i = 0; i < msg.len-l; i++){
                    msg.buf[i] = msg.buf[i+l];
                }
                msg.len -= l;
            } else { //success
                tx.pop();
            }
        }
    }
};

/** UDP Server communication backend
 *
 * server will send data to last connected client
 */
struct UDP:
        public Sink<Buffer<uint8_t>>,
        public Source<Buffer<uint8_t>> {
    int fd{};
    struct {
        struct sockaddr addr {};
        socklen_t len {sizeof (struct sockaddr)};
    } peer;
    static constexpr size_t BLEN = 128;
    Queue<Buffer<uint8_t>> tx, rx;
    Buffer<uint8_t> wrk = BLEN;
    /** create UDP Server listening on ip/port */
    UDP (const char *ip, uint16_t port) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
        };
        inet_pton(AF_INET, ip, &addr.sin_addr);

        fd = socket(PF_INET, SOCK_DGRAM, 0);
        if (bind(fd, (sockaddr*)&addr, sizeof addr) == -1 ) {
            perror("cannot bind socket to address");
            return;
        }
        if (fcntl(fd, F_SETFL,
                    fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
            perror("cannot set O_NONBLOCK on socket");
            return;
        }
    }
    ~UDP() {
        close(fd);
    }
    bool full() override {
        return tx.full();
    }
    using Sink::push;
    void push(Buffer<uint8_t> &&b) override {
        tx.push(std::move(b));
        process();
    }
    bool empty() override {
        int l = recvfrom(fd, wrk.buf, wrk.size, 0,
                &peer.addr, &peer.len);
        if (l < 0) {
            if (errno != EAGAIN) {
                perror("reading on socket error");
            }
            return rx.empty();
        }
        wrk.len = l;
        rx.push(std::move(wrk));
        wrk = BLEN;
        return false;
    }
    Buffer<uint8_t> pop() override {
        return rx.pop();
    }
    void process() {
        //transmit side
        while (!tx.empty()) {
            auto msg = tx.front();
            int l = sendto(fd, msg.buf, msg.len, 0,
                        &peer.addr, peer.len);
            if (l < 0) {
                if (errno != EAGAIN) {
                    perror("writing to socket error");
                }
                break;
            } else if ((size_t)l < msg.len) {
                for (size_t i = 0; i < msg.len-l; i++){
                    msg.buf[i] = msg.buf[i+l];
                }
                msg.len -= l;
            } else { //success
                tx.pop();
            }
        }
    }
};

/** simple wrapper around TTY & Logger
 * useful for creating simple user interactions
 */
struct IFACE: public TTY, public Logger {
    IFACE(const char *path): TTY(path), Logger(*(TTY*)this) {}
};
