#include <comm/fixedframe.h>
#include <doctest/doctest.h>

TEST_CASE("tool-libs: fixedframe: roundtrip") {
    Queue<Buffer<uint8_t>> wire{4};
    FixedFrame<8> ff{.in = wire, .out = wire};

    Frame f{200};
    f.pack<uint16_t>(0x1234);
    ff.out.push(f);
    REQUIRE(wire.front().len == 9); // id + padded payload
    CHECK(wire.front()[3] == 0);

    Frame big{3};
    for (int i = 0; i < 9; ++i) big.pack<uint8_t>(i);
    ff.out.push(big); // too long, dropped
    CHECK(ff.in.empty() == false);
    Frame r = ff.in.pop();
    CHECK(r.id == 200);
    CHECK(r.b.len == 8);
    CHECK(r.unpack<uint16_t>() == 0x1234);
    CHECK(ff.in.empty());
}

TEST_CASE("tool-libs: fixedframe: frames split across buffers") {
    Queue<Buffer<uint8_t>> wire{4};
    FixedFrame<2> ff{.in = wire, .out = wire};
    wire.push(Buffer<uint8_t>{7, 1});
    wire.push(Buffer<uint8_t>{2, 8, 3, 4, 9});
    REQUIRE(!ff.in.empty());
    Frame a = ff.in.pop();
    CHECK(a.id == 7);
    CHECK(a.b[1] == 2);
    REQUIRE(!ff.in.empty());
    CHECK(ff.in.pop().id == 8);
    CHECK(ff.in.empty()); // 9 waits for its payload
    wire.push(Buffer<uint8_t>{5, 6});
    REQUIRE(!ff.in.empty());
    CHECK(ff.in.pop().b[0] == 5);
}

TEST_CASE("tool-libs: fixedframe: registry") {
    Queue<Buffer<uint8_t>> wire{4};
    FixedFrame<> ff{.in = wire, .out = wire};
    static int got;
    ff.reg.setHandler(42, [](Frame &f) { got = f.unpack<int>(); });
    Frame f{42};
    f.pack(17);
    ff.out.push(f);
    ff.poll(0, 0);
    CHECK(got == 17);
}

TEST_CASE("tool-libs: fixedframe: rest kept while frame queue full") {
    Queue<Buffer<uint8_t>> wire{4};
    FixedFrame<1> ff{.in = wire, .out = wire};
    Buffer<uint8_t> many(60);
    for (uint8_t i = 0; i < 30; ++i) {
        many.append(i);
        many.append(i);
    }
    wire.push(std::move(many));
    int n = 0;
    while (!ff.in.empty()) {
        Frame f = ff.in.pop();
        CHECK(f.id == n);
        CHECK(f.b[0] == n);
        ++n;
    }
    CHECK(n == 30);
}
