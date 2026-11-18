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

struct Prefixed : Logger {
    using Logger::Logger;
    Buffer<uint8_t> pre() override {
        Buffer<uint8_t> b(256);
        b.len = snprintf((char*)b.buf, b.size, "(@1ms): ");
        return b;
    }
};

TEST_CASE("tool-libs: logger: tap gets the raw text of the levels in the mask") {
    Collect local, tap;
    Prefixed l{local};
    l.tap(&tap, Logger::bit(Logger::WARN) | Logger::bit(Logger::ERROR));

    l.info("not tapped");
    CHECK(tap.last.len == 0);
    CHECK(local.last.len > 0); // the local sink gets everything

    l.warn("w %d", 1);
    REQUIRE(tap.last.len == 4);
    CHECK(tap.last[0] == Logger::WARN);
    CHECK(memcmp(tap.last.buf + 1, "w 1", 3) == 0); // no prefix, no color

    l.error("bad");
    REQUIRE(tap.last.len == 4);
    CHECK(tap.last[0] == Logger::ERROR);
    CHECK(memcmp(tap.last.buf + 1, "bad", 3) == 0);
    std::string got((char*)local.last.buf, local.last.len);
    CHECK(got == "(@1ms): \u001b[1;31mbad\u001b[0m");

    l.tap(&tap, Logger::bit(Logger::NONE));
    l.print("p");
    REQUIRE(tap.last.len == 2);
    CHECK(tap.last[0] == Logger::NONE);
    CHECK(tap.last[1] == 'p');

    // a full tap doesn't block and doesn't touch the local sink
    Full f;
    l.tap(&f, Logger::bit(Logger::NONE));
    l.print("q");
    CHECK(l.tapDropped == 1);
    CHECK(l.dropped == 0);
    CHECK(local.last[local.last.len - 1] == 'q');

    // pushed Buffers are not tapped
    l.tap(&tap, 0xff);
    tap.last = Buffer<uint8_t>{};
    l.push(Buffer<uint8_t>{'x'});
    CHECK(tap.last.len == 0);
}
