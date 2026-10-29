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
