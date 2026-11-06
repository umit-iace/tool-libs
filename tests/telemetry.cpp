#include <doctest/doctest.h>
#include <core/experiment.h>
#include <comm/telemetry.h>
#include <utils/queue.h>
#include <string>
// time is advanced manually in this test
void Kernel::idle() { }
Kernel k;

static void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}

/// looks like a fixed size Eigen vector
struct Vec2 {
    using Scalar = double;
    static constexpr int SizeAtCompileTime = 2;
    double v[2];
    const double *data() const { return v; }
};
/// looks like a dynamic size Eigen vector
struct VecX {
    using Scalar = double;
    static constexpr int SizeAtCompileTime = -1;
    const double *data() const { return nullptr; }
};
struct Pair {
    double pos, vel;
};

struct Rig {
    Vec2 x{{1, 2}};
    double y[3]{3, 4, 5};
    Pair p{6, 7};
    uint8_t mode = 8;

    struct State {
        static constexpr uint8_t id = 15;
        static constexpr char name[] = "state";
        template<typename F>
        static auto fields(const Rig &r, F &&f) {
            return f(r.x, r.y, r.p, r.mode);
        }
    };
    struct Slow {
        static constexpr uint8_t id = 16;
        static constexpr char name[] = "slow and long name";
        static constexpr uint32_t period = 100;
        template<typename F>
        static auto fields(const Rig &r, F &&f) {
            return f(r.mode);
        }
    };
    using Telemetry = telemetry::Frames<Rig, State, Slow>;
};

TEST_CASE("tool-libs: telemetry: field sizes") {
    CHECK(telemetry::size<Vec2>() == 16);
    CHECK(telemetry::size<double[3]>() == 24);
    CHECK(telemetry::size<Pair>() == 16);
    CHECK(telemetry::size<uint8_t>() == 1);
    CHECK(telemetry::payload<Rig, Rig::State>() == 4 + 16 + 24 + 16 + 1);
    CHECK(telemetry::payload<void, Experiment::StatusFrame>() == Experiment::STATUS_LEN);
    CHECK(telemetry::period<Rig::State>(20) == 20);
    CHECK(telemetry::period<Rig::Slow>(20) == 100);
    // a dynamic size matrix fails the build, its size is not known
    CHECK(telemetry::FixedMatrix<VecX>);
}

TEST_CASE("tool-libs: telemetry: frames are packed as sized") {
    FrameRegistry reg;
    Experiment e{&reg};
    Queue<Frame> out{50};
    Rig rig;
    telemetry::every(e.during(e.IDLE), 20, rig, out, Rig::Telemetry{});

    step(100);
    int state = 0, slow = 0;
    while (!out.empty()) {
        auto f = out.pop();
        if (f.id == Rig::Slow::id) {
            ++slow;
            CHECK(f.b.len == telemetry::payload<Rig, Rig::Slow>());
            continue;
        }
        REQUIRE(f.id == Rig::State::id);
        ++state;
        CHECK(f.b.len == telemetry::payload<Rig, Rig::State>());
        f.unpack<uint32_t>();
        for (double expected = 1; expected <= 7; ++expected) {
            CHECK(f.unpack<double>() == expected);
        }
        CHECK(f.unpack<uint8_t>() == 8);
    }
    CHECK(state == 5); // at 0, 20, .. 80 ms
    CHECK(slow == 1);
}

TELEMETRY_SUMMARY(telemetry::uart(115200), 20, Rig::Telemetry{}, Experiment::StatusFrame{})

TEST_CASE("tool-libs: telemetry: summary") {
    const auto &s = telemetrySummary;
    CHECK(std::string(s.link.name) == "UART 8N1");
    CHECK(s.link.capacity == 11520);
    REQUIRE(s.count == 3);
    CHECK(std::string(s.entries[0].name) == "state");
    CHECK(s.entries[0].bytes == 61 + telemetry::MIN_OVERHEAD);
    // names are cut to fit
    CHECK(std::string(s.entries[1].name) == "slow and lo");
    CHECK(s.entries[1].period == 100);
    CHECK(s.entries[2].id == Experiment::STATUS_ID);
    CHECK(s.entries[2].period == 250);
    CHECK(s.total() == doctest::Approx(71 * 50.0 + 15 * 10.0 + 31 * 4.0));
    CHECK(s.fits());

    // the same frames every 5 ms do not fit
    constexpr auto fast = telemetry::summary(telemetry::uart(115200), 5, Rig::Telemetry{});
    static_assert(!fast.fits());
    // a network link has a larger overhead per frame
    constexpr auto net = telemetry::summary(telemetry::udp(1000000), 20, Rig::Telemetry{});
    CHECK(net.entries[0].bytes == 61 + telemetry::MIN_OVERHEAD + 28);
}
