#include <doctest/doctest.h>
#include <core/kern.h>
#include <sys/comm.h>
#include <sys/stat.h>
#include <cstdlib>
#include <string>
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
