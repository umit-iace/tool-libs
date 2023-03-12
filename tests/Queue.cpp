#include <doctest/doctest.h>
#include <cassert>
#include <utils/Queue.h>
#include <iostream>
using namespace std;
struct data {
    double *d{nullptr};
    data(double d): d(new double{d}) {
        MESSAGE("constr: ", d);
    }
    ~data() {
        MESSAGE("destr: ", d);
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
    Buffer<uint8_t> c = q.pop();
    size_t i{};
    for (auto ch : c) {
        got[i++] = ch;
    }
    for (size_t i = 0; i < 5; ++i) {
        CHECK(got[i] == want[i]);
    }
}
TEST_CASE("tool-libs: queue: data struct with const/destr") {
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
