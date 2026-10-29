/** Kernel: registering before it is constructed, queue overruns
 *
 * `Kernel k` is defined in kernel_k.cpp, which is linked after this file:
 * the globals here are constructed before the Kernel
 */
#include <doctest/doctest.h>
#include <core/kern.h>

namespace {
/** like a sensor driver registering in its constructor */
struct Early {
    int calls{};
    size_t listSizeBefore;
    Schedule::Schedulable *handle;
    Early() : listSizeBefore(k.list.size) {
        handle = k.every(5, *this, &Early::call);
    }
    void call(uint32_t, uint32_t) { ++calls; }
} early;

void tickrun(int ms) {
    for (int i = 0; i < ms; ++i) {
        k.tick(1);
        k.Scheduler::run();
    }
}
int counted;
void count(uint32_t, uint32_t) { ++counted; }
}

TEST_CASE("tool-libs: kernel: registered before the kernel was constructed") {
    // the registration really happened before the Kernel's constructor
    CHECK(early.listSizeBefore == 0);
    REQUIRE(early.handle);
    tickrun(20);
    CHECK(early.calls == 4);
    CHECK(k.remove(early.handle));
}

TEST_CASE("tool-libs: kernel: more due calls than the queue holds") {
    counted = 0;
    Schedule::Schedulable *h[100];
    size_t before = k.q.capacity();
    for (auto &s : h) s = k.every(10, count);
    tickrun(30);
    // every one of them got called, nobody died
    CHECK(counted == 300);
    CHECK(k.q.capacity() > before);
    for (auto s : h) k.remove(s);
}
