#include <doctest/doctest.h>
#include <utils/queue.h>
#include <iostream>
using namespace std;
struct data {
    double *d{nullptr};
    data(double d=0): d(new double{d}) {
        // MESSAGE("constr: ", d, " @", this->d);
    }
    ~data() {
        // MESSAGE("destr: ", d);
        delete d;
        d = 0;
    }
    data(const data &dat) : d(new double{*dat.d}) {
    }
    data& operator=(const data &dat) {
        *d = *dat.d;
        return *this;
    }
    data(data &&dat) noexcept : d(dat.d) {
        dat.d = nullptr;
    }
    data& operator=(data &&dat) noexcept {
        d = dat.d;
        dat.d = 0;
        return *this;
    }
    operator double() { return *d; }
};
TEST_CASE("tool-libs: queue: move buffer") {
    uint8_t want[] = "abcde";
    uint8_t got[5]{};
    Buffer<uint8_t> b{want, 5};
    Queue<Buffer<uint8_t>> q{1};
    q.push(std::move(b));
    CHECK(!b.len);
    CHECK(!b.size);
    Buffer<uint8_t> c = q.pop();
    size_t i{};
    for (auto ch : c) {
        got[i++] = ch;
    }
    for (size_t i = 0; i < 5; ++i) {
        CHECK(got[i] == want[i]);
    }
}
TEST_CASE("tool-libs: queue: data struct with cstr/destr") {
    Queue<struct data> q{3};
    double want[3] = {4, 8, 3.14};
    double got[3]{};
    for (auto d: want) {
        q.push(d);
    }
    size_t i{};
    while (!q.empty()) {
        got[i++] = q.pop();
    }
    for (i = 0; i < 3; i++) {
        CHECK(got[i] == want[i]);
    }
}
TEST_CASE("tool-libs: queue: roundabout") {
    double want[20];
    for (size_t i = 1; i < 21; ++i) {
        want[i-1] = 10./i;
    }
    Queue<double> q{5};
    SUBCASE("on empty") {
        for (auto d: want) {
            q.push(d);
            CHECK(q.pop() == d);
        }
    }
    SUBCASE("on full") {
        size_t i{};
        for (auto d: want) {
            q.push(d);
            if (q.full()) CHECK(q.pop() == want[i++]);
        }
        while (!q.empty()) {
            CHECK(q.pop() == want[i++]);
        }
    }
}
TEST_CASE("tool-libs: queue: indexed access") {
    Queue<int> q{10};
    for (int i = 0; i < 7; ++i) {
        q.push(i);
        CHECK(i == q.getAt(0));
        q.pop();
    }
    CHECK(q.size() == 0);
    for (int i = 0; i < 5; ++i) {
        q.push(i);
    }
    for (int i = 0; i < 5; ++i) {
        CHECK(i == q.getAt(i));
    }
}
TEST_CASE("tool-libs: queue: Buffer copy constructor") {
    Buffer<int> b = {1,2,3,4};
    Queue<int> q(b);
    CHECK(q.size() == 4);
    for (int i = 0; i < 4; ++i) {
        CHECK(i+1 == q.pop());
    }
    CHECK(q.empty());
}
TEST_CASE("tool-libs: queue: Buffer move constructor") {
    Buffer<int> b = {1,2,3,4};
    Queue<int> q(std::move(b));
    CHECK(!b.buf);
    CHECK(!b.len);
    CHECK(!b.size);
    CHECK(q.size() == 4);
    for (int i = 0; i < 4; ++i) {
        CHECK(i+1 == q.pop());
    }
    CHECK(q.empty());
}
TEST_CASE("tool-libs: queue: copy operator") {
    Queue<int> q{10}, q2;
    for (int i = 0; i < 7; ++i) {
        q.push(i);
        CHECK(i == q.getAt(i));
    }
    q2 = q;
    for (int i = 0; i < 7; ++i) {
        CHECK(i == q.getAt(i));
        CHECK(i == q2.getAt(i));
    }
}
TEST_CASE("tool-libs: queue: copy constructor") {
    Queue<int> q{10};
    for (int i = 0; i < 7; ++i) {
        q.push(i);
        CHECK(i == q.getAt(i));
    }
    Queue q2{q};
    for (int i = 0; i < 7; ++i) {
        CHECK(i == q.getAt(i));
        CHECK(i == q2.getAt(i));
    }
}
TEST_CASE("tool-libs: queue: move operator") {
    Queue<int> q{10}, q2;
    for (int i = 0; i < 7; ++i) {
        q.push(i);
        CHECK(i == q.getAt(i));
    }
    q2 = std::move(q);
    CHECK(q.empty());
    CHECK(q.full());
    for (int i = 0; i < 7; ++i) {
        CHECK(i == q2.getAt(i));
    }
}
TEST_CASE("tool-libs: queue: move constructor") {
    Queue<int> q{10};
    for (int i = 0; i < 7; ++i) {
        q.push(i);
        CHECK(i == q.getAt(i));
    }
    auto q2{std::move(q)};
    CHECK(q.empty());
    CHECK(q.full());
    for (int i = 0; i < 7; ++i) {
        CHECK(i == q2.getAt(i));
    }
}
TEST_CASE("tool-libs: queue: q of qs") {
    using Inner = Queue<int>;
    Queue<Inner> out(10);
    int i = 1;
    while (!out.full()) {
        Inner in (i);
        for (int dat = 0; dat < i; ++dat) {
            in.push(dat);
        }
        CHECK(in.full());
        CHECK(in.getAt(i-1) == i-1);
        out.push(std::move(in));
        i++;
    }
    CHECK(out.full());
    i = 0;
    while (!out.empty()) {
        // MESSAGE("out not empty: ", out.size());
        auto q = out.pop();
        // MESSAGE("popped q of size ", q.size());
        CHECK(q.size() == ++i);
        int j {0};
        while (!q.empty()) {
            CHECK(j++ == q.pop());
            // MESSAGE("checked q entry ", j-1);
        }
        CHECK(q.empty());
        // MESSAGE("q empty");
    }
    CHECK(out.empty());
}
TEST_CASE("tool-libs: queue: buffer of buffers cstr") {
    using Inner = Buffer<int>;
    Buffer<Inner> cnstr = { {1,2,3}, {4,5,6}, };
    Queue<Inner> out(std::move(cnstr));
    CHECK(!cnstr.size);
    CHECK(!cnstr.len);
    CHECK(!cnstr.buf);
    CHECK(out.size() == 2);
    auto b = out.pop();
    CHECK(b[0] == 1);
    CHECK(b[1] == 2);
    CHECK(b[2] == 3);
}
