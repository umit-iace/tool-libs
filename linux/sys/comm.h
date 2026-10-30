#pragma once
#include <utils/queue.h>
#include <core/logger.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
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
    Buffer<uint8_t> wrk = Buffer<uint8_t>(BLEN);
    /** open given path */
    TTY (const char *path) {
        fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        // reported once, afterwards the TTY behaves like a closed one
        if (fd < 0) perror("cannot open tty");
    }
    ~TTY() {
        if (fd >= 0) close(fd);
    }
    // owns the file descriptor
    TTY(const TTY &) = delete;
    TTY& operator=(const TTY &) = delete;
    bool full() override {
        return tx.full();
    }
    using Sink::push;
    void push(Buffer<uint8_t> &&b) override {
        if (fd < 0) return; // nowhere to send it
        tx.push(std::move(b));
        process();
    }
    bool empty() override {
        if (rx.full()) return false; // consumer has to catch up first
        if (fd < 0) return rx.empty();
        int l = read(fd, wrk.buf, wrk.size);
        if (l == 0) return rx.empty(); // nothing read, e.g. EOF
        if (l < 0) {
            if (errno != EAGAIN) {
                perror("reading on fd error");
            }
            return rx.empty();
        }
        wrk.len = l;
        rx.push(std::move(wrk));
        wrk = Buffer<uint8_t>(BLEN);
        return false;
    }
    Buffer<uint8_t> pop() override {
        return rx.pop();
    }
    void process() {
        //transmit side
        while (!tx.empty()) {
            auto &msg = tx.front(); // partially sent data gets dropped in place
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
    int fd{-1};
    /** where data is sent to: given, or whoever sent us data last */
    struct {
        struct sockaddr_storage addr {};
        socklen_t len {};
    } peer;
    /// receive buffer, a datagram is read at once and anything beyond is lost,
    /// pywisp sends all frames of e.g. an experiment start in one datagram of up to 1400 bytes
    static constexpr size_t BLEN = 2048;
    Queue<Buffer<uint8_t>> tx, rx;
    Buffer<uint8_t> wrk = Buffer<uint8_t>(BLEN);
    /** create UDP Server listening on ip/port, answering whoever sends data
     *
     * data pushed before the first datagram arrived is dropped
     */
    UDP (const char *ip, uint16_t port) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
        };
        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            fprintf(stderr, "invalid address: %s\n", ip);
            return;
        }
        fd = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            perror("cannot create socket");
            return;
        }
        if (bind(fd, (sockaddr*)&addr, sizeof addr) == -1 ) {
            perror("cannot bind socket to address");
            close(fd);
            fd = -1;
            return;
        }
    }
    /** create UDP Server listening on ip/port, sending to peerip/peerport */
    UDP (const char *ip, uint16_t port, const char *peerip, uint16_t peerport)
            : UDP(ip, port) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(peerport),
        };
        if (inet_pton(AF_INET, peerip, &addr.sin_addr) != 1) {
            fprintf(stderr, "invalid peer address: %s\n", peerip);
            return;
        }
        memcpy(&peer.addr, &addr, sizeof addr);
        peer.len = sizeof addr;
    }
    ~UDP() {
        if (fd >= 0) close(fd);
    }
    // owns the socket
    UDP(const UDP &) = delete;
    UDP& operator=(const UDP &) = delete;
    bool full() override {
        return tx.full();
    }
    using Sink::push;
    void push(Buffer<uint8_t> &&b) override {
        // nowhere to send it (yet)
        if (fd < 0 || peer.len == 0) return;
        tx.push(std::move(b));
        process();
    }
    bool empty() override {
        if (rx.full()) return false; // consumer has to catch up first
        if (fd < 0) return rx.empty();
        struct sockaddr_storage from {};
        socklen_t fromlen = sizeof from;
        int l = recvfrom(fd, wrk.buf, wrk.size, 0,
                (sockaddr*)&from, &fromlen);
        if (l >= 0) {
            memcpy(&peer.addr, &from, fromlen);
            peer.len = fromlen;
        }
        if (l == 0) return rx.empty(); // nothing read, e.g. EOF
        if (l < 0) {
            if (errno != EAGAIN) {
                perror("reading on socket error");
            }
            return rx.empty();
        }
        wrk.len = l;
        rx.push(std::move(wrk));
        wrk = Buffer<uint8_t>(BLEN);
        return false;
    }
    Buffer<uint8_t> pop() override {
        return rx.pop();
    }
    void process() {
        //transmit side
        while (!tx.empty()) {
            auto &msg = tx.front(); // partially sent data gets dropped in place
            int l = sendto(fd, msg.buf, msg.len, 0,
                        (sockaddr*)&peer.addr, peer.len);
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
    IFACE(const char *path): TTY(path), Logger(static_cast<TTY&>(*this)) {}
};
