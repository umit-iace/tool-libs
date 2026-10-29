#include <cstdio>
#include <string>
#include <comm/min.h>
#include <comm/bufferutils.h>
#include <doctest/doctest.h>

struct Printer : Sink<Buffer<uint8_t>> {
    bool full() override {
        return false;
    }
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

TEST_CASE("tool-libs: min: rx drops frame too big for Frame") {
    Queue<Buffer<uint8_t>> q{2};
    Min min{.in=q, .out=hex};
    Buffer<uint8_t> in(300);
    in.append({0xaa, 0xaa, 0xaa, 0x1, 200});
    for (int i = 0; i < 200; ++i) in.append(0x11);
    // followed by a valid frame
    in.append({
        0xaa, 0xaa, 0xaa,
        0x1, 0xc,
        0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x9, 0x40,
        0x10, 0xe, 0x0, 0x0,
        0x13, 0x96, 0x30, 0x3c,
        0x55,
    });
    q.push(std::move(in));
    REQUIRE(!min.in.empty());
    Frame f = min.in.pop();
    CHECK(f.id == 1);
    CHECK(f.unpack<double>() == 3.14);
    CHECK(min.in.empty());
}
TEST_CASE("tool-libs: min: full frame of header bytes roundtrip") {
    Queue<Buffer<uint8_t>> q{3};
    Min m{.in=q, .out=q};
    Frame f{1};
    // maximum number of stuff bytes
    while (f.b.len < f.b.size) f.pack<uint8_t>(Min::HEADER_BYTE);
    m.out.push(std::move(f));
    REQUIRE(!m.in.empty());
    Frame r = m.in.pop();
    CHECK(r.id == 1);
    REQUIRE(r.b.len == r.b.size);
    for (size_t i = 0; i < r.b.len; ++i) {
        CHECK(r.unpack<uint8_t>() == Min::HEADER_BYTE);
    }
}

TEST_CASE("tool-libs: min: CRC32 matches the standard check value") {
    CRC32 crc;
    for (char c : std::string("123456789")) crc.step(c);
    CHECK(crc.finalize() == 0xcbf43926);
}

struct Concat : Sink<Buffer<uint8_t>> {
    std::string all;
    bool full() override { return false; }
    void push(Buffer<uint8_t> &&b) override { all.append((char*)b.buf, b.len); }
};

TEST_CASE("tool-libs: min: more frames in one buffer than the queue holds") {
    Concat c;
    Min::Out out{c};
    for (int i = 0; i < 25; ++i) out.push(Frame{(uint8_t)i}.pack<uint32_t>(i));
    Queue<Buffer<uint8_t>> q{2};
    q.push(Buffer<uint8_t>((const uint8_t*)c.all.data(), c.all.size()));
    Min::In in{q};
    int got = 0;
    while (!in.empty()) {
        Frame f = in.pop();
        CHECK(f.id == got);
        CHECK(f.unpack<uint32_t>() == (uint32_t)got);
        ++got;
    }
    CHECK(got == 25); // used to be 20, the rest was thrown away
}

#include <comm/line.h>
TEST_CASE("tool-libs: line filter: more lines in one buffer than the queue holds") {
    std::string text;
    for (int i = 0; i < 40; ++i) text += std::to_string(i) + "\n";
    Queue<Buffer<uint8_t>> q{2};
    q.push(Buffer<uint8_t>((const uint8_t*)text.data(), text.size()));
    LineFilter lines{q};
    int got = 0;
    while (!lines.empty()) {
        auto l = lines.pop();
        CHECK(std::string((char*)l.buf, l.len) == std::to_string(got));
        ++got;
    }
    CHECK(got == 40);
}
