#include <doctest/doctest.h>
#include <core/kern.h>
#include <sys/comm.h>
#include <sys/stat.h>
#include <cstdlib>
#include <string>
#include <type_traits>
Kernel k;

TEST_CASE("tool-libs: linux: TTY partial writes") {
    char dir[] = "/tmp/tool-libs-test-XXXXXX";
    REQUIRE(mkdtemp(dir));
    std::string fifo = std::string(dir) + "/fifo";
    REQUIRE(mkfifo(fifo.c_str(), 0600) == 0);
    {
        TTY t{fifo.c_str()};
        int rd = open(fifo.c_str(), O_RDONLY | O_NONBLOCK);
        REQUIRE(rd >= 0);

        // bigger than the pipe buffer -> partial writes
        const size_t N = 200000;
        Buffer<uint8_t> b(N);
        for (size_t i = 0; i < N; ++i) b.append(i % 251);
        t.push(std::move(b));

        std::string got;
        char chunk[4096];
        for (int i = 0; i < 1000 && got.size() < N; ++i) {
            int l;
            while ((l = read(rd, chunk, sizeof chunk)) > 0) got.append(chunk, l);
            t.process();
        }
        REQUIRE(got.size() == N);
        bool same = true;
        for (size_t i = 0; i < N; ++i) same &= (uint8_t)got[i] == i % 251;
        CHECK(same);
        close(rd);
    }
    unlink(fifo.c_str());
    rmdir(dir);
}

TEST_CASE("tool-libs: linux: TTY EOF is not data") {
    TTY t{"/dev/null"};
    CHECK(t.empty());
}

TEST_CASE("tool-libs: linux: TTY that couldn't be opened") {
    TTY t{"/nonexistent/tty"};
    CHECK(t.fd < 0);
    CHECK(t.empty());
    t.push(Buffer<uint8_t>{1, 2, 3}); // dropped quietly
    CHECK(t.tx.empty());
    static_assert(!std::is_copy_constructible_v<TTY>);
}

TEST_CASE("tool-libs: linux: UDP peer") {
    UDP server{"127.0.0.1", 45911};
    UDP client{"127.0.0.1", 45912, "127.0.0.1", 45911};
    REQUIRE(server.fd >= 0);
    REQUIRE(client.fd >= 0);

    // nobody talked to the server yet: nowhere to send to
    server.push(Buffer<uint8_t>{9});
    CHECK(server.tx.empty());

    client.push(Buffer<uint8_t>{1, 2, 3});
    bool got = false;
    for (int i = 0; i < 100 && !got; ++i) {
        got = !server.empty();
        if (!got) usleep(1000);
    }
    REQUIRE(got);
    CHECK(server.pop().len == 3);

    // answers go back to the sender
    server.push(Buffer<uint8_t>{4, 5});
    got = false;
    for (int i = 0; i < 100 && !got; ++i) {
        got = !client.empty();
        if (!got) usleep(1000);
    }
    REQUIRE(got);
    CHECK(client.pop()[1] == 5);
    static_assert(!std::is_copy_constructible_v<UDP>);
}

// blocking client socket connected to 127.0.0.1:port
static int tcpClient(uint16_t port) {
    int c = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(c, (sockaddr*)&addr, sizeof addr) != 0) {
        close(c);
        return -1;
    }
    struct timeval tv{.tv_sec = 1};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    return c;
}

static bool waitData(Source<Buffer<uint8_t>> &s) {
    for (int i = 0; i < 100; ++i) {
        if (!s.empty()) return true;
        usleep(1000);
    }
    return false;
}

TEST_CASE("tool-libs: linux: TCP server") {
    TCP server{"127.0.0.1", 45913};
    REQUIRE(server.fd >= 0);
    static_assert(!std::is_copy_constructible_v<TCP>);

    // nobody connected yet: nowhere to send to
    server.push(Buffer<uint8_t>{9});
    CHECK(server.tx.empty());

    for (uint8_t round = 0; round < 2; ++round) {
        // the next client is accepted after the last one disconnected
        int c = tcpClient(45913);
        REQUIRE(c >= 0);
        uint8_t data[] = {1, 2, round};
        REQUIRE(send(c, data, sizeof data, 0) == 3);
        REQUIRE(waitData(server));
        auto b = server.pop();
        CHECK(b.len == 3);
        CHECK(b[2] == round);

        server.push(Buffer<uint8_t>{4, 5});
        uint8_t got[2]{};
        CHECK(recv(c, got, sizeof got, MSG_WAITALL) == 2);
        CHECK(got[1] == 5);

        close(c);
        for (int i = 0; i < 100 && server.client >= 0; ++i) {
            server.empty();
            usleep(1000);
        }
        CHECK(server.client < 0);
    }
}

TEST_CASE("tool-libs: linux: TCP partial sends") {
    TCP server{"127.0.0.1", 45914};
    int c = tcpClient(45914);
    REQUIRE(c >= 0);
    for (int i = 0; i < 100 && server.client < 0; ++i) {
        server.empty();
        usleep(1000);
    }
    REQUIRE(server.client >= 0);

    // bigger than the socket buffers -> partial sends
    const size_t N = 8000000;
    Buffer<uint8_t> b(N);
    for (size_t i = 0; i < N; ++i) b.append(i % 251);
    server.push(std::move(b));
    CHECK(!server.tx.empty());

    std::string got;
    char chunk[65536];
    struct timeval tv{.tv_usec = 1000};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    for (int i = 0; i < 10000 && got.size() < N; ++i) {
        int l;
        while ((l = recv(c, chunk, sizeof chunk, 0)) > 0) got.append(chunk, l);
        server.process();
    }
    REQUIRE(got.size() == N);
    bool same = true;
    for (size_t i = 0; i < N; ++i) same &= (uint8_t)got[i] == i % 251;
    CHECK(same);

    // a client that is gone doesn't kill the server (no SIGPIPE)
    close(c);
    for (int i = 0; i < 100 && server.client >= 0; ++i) {
        server.push(Buffer<uint8_t>{1});
        server.empty();
        usleep(1000);
    }
    CHECK(server.client < 0);
}

TEST_CASE("tool-libs: linux: TCP that couldn't listen") {
    TCP bad{"not an ip", 45915};
    CHECK(bad.fd < 0);
    bad.push(Buffer<uint8_t>{1});
    CHECK(bad.empty());
    TCP first{"127.0.0.1", 45915};
    TCP second{"127.0.0.1", 45915}; // port taken
    CHECK(first.fd >= 0);
    CHECK(second.fd < 0);
}

TEST_CASE("tool-libs: linux: TCP disconnect drops unsent data") {
    TCP server{"127.0.0.1", 45916};
    int c = tcpClient(45916);
    REQUIRE(c >= 0);
    for (int i = 0; i < 100 && server.client < 0; ++i) {
        server.empty();
        usleep(1000);
    }
    REQUIRE(server.client >= 0);
    Buffer<uint8_t> b(8000000);
    b.len = b.size;
    server.push(std::move(b));
    REQUIRE(!server.tx.empty());
    server.disconnect();
    CHECK(server.tx.empty());
    close(c);
}
