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
