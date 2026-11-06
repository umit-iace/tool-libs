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

/** TCP Server communication backend
 *
 * byte stream with one client at a time, the next one is accepted after the
 * current one disconnected. The stream carries no framing of its own, data
 * may arrive split or merged differently than it was sent. Wrap it in
 * \ref FixedFrame to talk to pyWisp's TcpConnection.
 * ```
 * TCP tcp{"0.0.0.0", 45670};
 * FixedFrame<> ff{.in = tcp, .out = tcp};
 * k.every(1, ff, &FixedFrame<>::poll);
 * ```
 */
struct TCP:
        public Sink<Buffer<uint8_t>>,
        public Source<Buffer<uint8_t>> {
    int fd{-1};     ///< listening socket
    int client{-1}; ///< connected client, -1 if there is none
    static constexpr size_t BLEN = 2048;
    Queue<Buffer<uint8_t>> tx, rx;
    Buffer<uint8_t> wrk = Buffer<uint8_t>(BLEN);
    /** create TCP Server listening on ip/port
     *
     * data pushed while no client is connected is dropped
     */
    TCP (const char *ip, uint16_t port) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
        };
        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            fprintf(stderr, "invalid address: %s\n", ip);
            return;
        }
        fd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            perror("cannot create socket");
            return;
        }
        int on = 1; // restarting must not wait for TIME_WAIT of the last connection
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
        if (bind(fd, (sockaddr*)&addr, sizeof addr) == -1
                || listen(fd, 1) == -1) {
            perror("cannot listen on address");
            close(fd);
            fd = -1;
            return;
        }
    }
    ~TCP() {
        disconnect();
        if (fd >= 0) close(fd);
    }
    // owns the sockets
    TCP(const TCP &) = delete;
    TCP& operator=(const TCP &) = delete;
    bool full() override {
        return tx.full();
    }
    using Sink::push;
    void push(Buffer<uint8_t> &&b) override {
        if (client < 0) return; // nowhere to send it
        tx.push(std::move(b));
        process();
    }
    bool empty() override {
        if (rx.full()) return false; // consumer has to catch up first
        if (client < 0) accept();
        if (client < 0) return rx.empty();
        int l = recv(client, wrk.buf, wrk.size, 0);
        if (l == 0) { // client closed the connection
            disconnect();
            return rx.empty();
        }
        if (l < 0) {
            if (errno != EAGAIN) {
                perror("reading on socket error");
                disconnect();
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
        while (client >= 0 && !tx.empty()) {
            auto &msg = tx.front(); // partially sent data gets dropped in place
            // no SIGPIPE if the client is gone, send fails with EPIPE instead
            int l = send(client, msg.buf, msg.len, MSG_NOSIGNAL);
            if (l < 0) {
                if (errno != EAGAIN) {
                    perror("writing to socket error");
                    disconnect();
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
    /** close the connection to the client, the next one may connect */
    void disconnect() {
        if (client < 0) return;
        close(client);
        client = -1;
        // unsent data was meant for this client
        while (!tx.empty()) tx.pop();
    }
private:
    void accept() {
        if (fd < 0) return;
        client = accept4(fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0 && errno != EAGAIN) {
            perror("cannot accept connection");
        }
    }
};

/** simple wrapper around TTY & Logger
 * useful for creating simple user interactions
 */
struct IFACE: public TTY, public Logger {
    IFACE(const char *path): TTY(path), Logger(static_cast<TTY&>(*this)) {}
};
