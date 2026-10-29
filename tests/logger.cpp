#include <doctest/doctest.h>
#include <string>
#include <cstring>
#include <utils/queue.h>
#include <core/logger.h>

struct Collect : Sink<Buffer<uint8_t>> {
    Buffer<uint8_t> last;
    bool full() override { return false; }
    void push(Buffer<uint8_t> &&b) override { last = std::move(b); }
};

TEST_CASE("tool-libs: logger: truncate long messages") {
    Collect c;
    Logger l{c};
    char big[400];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = 0;
    l.info("%s", big);
    REQUIRE(c.last.len < c.last.size);
    // color is reset even though the message got truncated
    const char *reset = "\u001b[0m";
    CHECK(memcmp(c.last.buf + c.last.len - strlen(reset), reset, strlen(reset)) == 0);
}

TEST_CASE("tool-libs: logger: buffered logger formats all arguments") {
    Collect c;
    BufferedLogger l{c};
    l.info("%s %d %s\n", "hello", 42, "world");
    l.flush();
    std::string got((char*)c.last.buf, c.last.len);
    CHECK(got.find("hello 42 world\n") != std::string::npos);
}

TEST_CASE("tool-libs: logger: buffered logger truncates long messages") {
    Collect c;
    BufferedLogger l{c};
    char big[400];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = 0;
    l.print("%s", big);
    CHECK(l.ix < BufferedLogger::MAXLEN);
    l.flush();
    CHECK(c.last.len < (size_t)BufferedLogger::MAXLEN);
}

struct Full : Sink<Buffer<uint8_t>> {
    int pushed = 0;
    bool full() override { return true; }
    void push(Buffer<uint8_t> &&) override { ++pushed; }
};

TEST_CASE("tool-libs: logger: a full sink doesn't block, messages are counted") {
    Full f;
    Logger l{f};
    l.info("lost %d\n", 1);
    l.push(Buffer<uint8_t>{1, 2, 3});
    CHECK(f.pushed == 0);
    CHECK(l.dropped == 2);

    BufferedLogger b{f};
    b.info("lost too\n");
    b.flush();
    CHECK(f.pushed == 0);
    CHECK(b.dropped == 1);
}

TEST_CASE("tool-libs: logger: pushing more than the prefix buffer holds") {
    Collect c;
    Logger l{c};
    Buffer<uint8_t> big(600);
    for (int i = 0; i < 600; ++i) big.append('a' + i % 26);
    l.push(std::move(big));
    REQUIRE(c.last.len == 600); // the default prefix is empty
    CHECK(c.last[599] == 'a' + 599 % 26);
}

TEST_CASE("tool-libs: logger: point it to another sink") {
    Collect a, b;
    Logger l{a};
    l.print("one");
    l.setSink(b);
    l.print("two");
    CHECK(a.last.len == 3);
    CHECK(b.last.len == 3);
    CHECK(memcmp(b.last.buf, "two", 3) == 0);
}
