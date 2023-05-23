#include <cstdio>
#include <comm/min.h>
#include <comm/bufferutils.h>
#include <doctest/doctest.h>

struct Printer : Sink<Buffer<uint8_t>> {
    void push(Buffer<uint8_t> &&b) override {
        printf("%.*s\n", b.len, b.buf);
    }
}p;
Hexify hex{p};

TEST_CASE("tool-libs: frame: pack / unpack roundtrip") {
    Frame f;
    f.pack<double>(3.14);
    f.pack((uint32_t) 3600);

    CHECK(f.unpack<double>() == 3.14);
    CHECK(f.unpack<uint32_t>() == 3600);
}
TEST_CASE("tool-libs: min: tx") {
    Min::Out out{hex};
    Frame f{1};
    f.pack(3.14);
    f.pack((uint32_t) 3600);

    out.push(f);
    f.id=0x10;
    out.push(f);
}
TEST_CASE("tool-libs: min: rx") {
    Queue<Buffer<uint8_t>> q{2};
    Min min{.in=q, .out=hex};
    uint8_t input[][22] = {
        {
        0xaa, 0xaa, 0xaa,
        0x1, 0xc,
        0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x9, 0x40,
        0x10, 0xe, 0x0, 0x0,
        0x13, 0x96, 0x30, 0x3c,
        0x55,
        },
        {
        0xaa, 0xaa, 0xaa,
        0x10, 0xc,
        0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x9, 0x40,
        0x10, 0xe, 0x0, 0x0,
        0xe8, 0x2b, 0xed, 0x26,
        0x55,
        },
    };
    uint8_t ids[] = {1, 0x10};
    q.push({input[0], sizeof input[0]});
    q.push({input[1], sizeof input[1]});
    int i = 0;
    while (!min.in.empty()) {
        Frame f = min.in.pop();
        CHECK(f.id == ids[i]);
        double pi = f.unpack<double>();
        uint32_t sph = f.unpack<uint32_t>();
        CHECK(pi == 3.14);
        CHECK(sph == 3600);
        ++i;
    }
    CHECK(i == 2);
}
TEST_CASE("tool-libs: frame: struct roundtrip") {
    struct __attribute__((packed)) TestStruct {
        double pi=3.14;
        uint32_t sph=3600;
        bool tru = true;
    } send, recv = {
        .pi=0,
        .sph=0,
        .tru=false,
    };
    Frame f{19};
    f.pack(3.14);
    f.pack<uint32_t>(3600);
    f.pack(true);
    recv = f.unpack<decltype(recv)>();
    CHECK(recv.pi - 3.14 == 0);
    CHECK(recv.sph - 3600 == 0);
    CHECK(recv.tru - true == 0);

    f = Frame{18};
    f.pack(send);
    recv = f.unpack<decltype(recv)>();
    CHECK(send.pi - recv.pi == 0);
    CHECK(send.sph - recv.sph == 0);
    CHECK(send.tru - recv.tru == 0);

    SUBCASE("smth") {
        Queue<Buffer<uint8_t>> q{3};
        Min m{.in=q, .out=q};
        Frame f{18}, r{};
        f.pack(send);
        m.out.push(f);
        if (!m.in.empty()) {
            r = m.in.pop();
            CHECK(r.id == f.id);
            for (size_t i = 0; i < f.b.len; ++i)
                CHECK(r.b[i] == f.b[i]);
            hex.push(std::move(r.b));
        }
    }
}
TEST_CASE("tool-libs: frame: big struct roundtrip") {
struct __attribute((packed)) BigStruct {
    uint8_t msg[75] ="\n\
        Hello World!             |\n\
        hoi                      |\n";
    double pi=3.14159265;
    uint32_t longs[4] = {123456,0,0, 234567};
    bool tru = true;
    void print(){
        printf("msg=%s pi=%lf l=[%d,%d,%d,%d] tru=%x\n"
                , msg, pi
                , longs[0],longs[1],longs[2],longs[3]
                , tru
              );
    }
} bigsend, bigrecv = { .msg = "", .pi=0, .longs = {0, 0}, .tru=false };
    auto f = Frame{16};
    f.pack(bigsend);
    bigrecv = f.unpack<decltype(bigrecv)>();
    CHECK(bigsend.pi - bigrecv.pi == 0);
    for (int i = 0; i < 4; ++i) {
        CHECK(bigsend.longs[i] - bigrecv.longs[i] == 0);
    }
    CHECK(bigsend.tru - bigrecv.tru == 0);

    CHECK(sizeof(bigrecv) == 100);
}

TEST_CASE("tool-libs: frame: bool double bug") {
    struct __attribute__((packed)) Data {
        bool b;
        double d;
    } snd {
        .b = true,
        .d = 3.14,
    }, rcv {};
    auto f = Frame{0}.pack(snd);
    rcv = f.unpack<Data>();
    CHECK(snd.b == rcv.b);
    CHECK(snd.d - rcv.d == doctest::Approx(0));
    f = Frame{0}.pack(snd);
    bool b = f.unpack<bool>();
    double d = f.unpack<double>();
    CHECK(snd.b == b);
    CHECK(snd.d - d == doctest::Approx(0));
}
