#include <doctest/doctest.h>
#include <string>
#include <core/experiment.h>
// time is advanced manually in this test
void Kernel::idle() { }
Kernel k;

static void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}

static Buffer<uint8_t> msg(uint8_t lvl, size_t n) {
    Buffer<uint8_t> b(1 + n);
    b.append(lvl);
    for (size_t i = 0; i < n; ++i) b.append('a' + i % 26);
    return b;
}

TEST_CASE("tool-libs: experiment: log frames split long messages") {
    Queue<Frame> out{10};
    Experiment::LogFrames lf;
    lf.out = &out;

    lf.push(msg(Logger::WARN, 250));
    REQUIRE(out.size() == 2);
    auto a = out.pop(), b = out.pop();
    CHECK(a.id == Experiment::LOG_ID);
    CHECK(b.id == Experiment::LOG_ID);
    CHECK(a.b.len == 128);
    CHECK(a.b[0] == (Logger::WARN | Experiment::LOG_MORE));
    CHECK(b.b.len == 1 + 250 - 127);
    CHECK(b.b[0] == Logger::WARN);
    CHECK(a.b[1] == 'a');
    CHECK(b.b[1] == 'a' + 127 % 26);

    lf.push(msg(Logger::ERROR, 5));
    REQUIRE(out.size() == 1);
    a = out.pop();
    CHECK(a.b.len == 6);
    CHECK(a.b[0] == Logger::ERROR);

    // exactly one frame full of text needs no second one
    lf.push(msg(Logger::INFO, 127));
    REQUIRE(out.size() == 1);
    CHECK(out.pop().b[0] == Logger::INFO);
}

TEST_CASE("tool-libs: experiment: enableLog sends k.log and the experiment messages once") {
    FrameRegistry reg;
    Queue<Frame> out{10};
    {
        Experiment e{&reg};
        e.enableLog(out, Logger::bit(Logger::INFO) | Logger::bit(Logger::ERROR));

        k.log.warn("not in the mask");
        CHECK(out.empty());
        k.log.error("broken %d", 7);
        REQUIRE(out.size() == 1);
        auto f = out.pop();
        CHECK(f.b[0] == Logger::ERROR);
        CHECK(std::string((char*)f.b.buf + 1, f.b.len - 1) == "broken 7");

        reg.handle(std::move(Frame{1}.pack<uint8_t>(0b01))); // start
        step(2);
        REQUIRE(out.size() == 1); // from the experiment logger, not again from k.log
        f = out.pop();
        CHECK(f.b[0] == Logger::INFO);
        CHECK(std::string((char*)f.b.buf + 1, f.b.len - 1) == "started\n");
    }
    // the tap of k.log is removed with the experiment
    k.log.error("gone");
    CHECK(out.empty());
}
