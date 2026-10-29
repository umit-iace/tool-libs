/** release builds (NDEBUG) must not write out of bounds when the
 * assertions are gone
 */
#define NDEBUG
#include <doctest/doctest.h>
#include <utils/queue.h>
#include <comm/frameregistry.h>
#include <utils/slice.h>

TEST_CASE("tool-libs: NDEBUG: Buffer drops what doesn't fit") {
    Buffer<uint8_t> b(2);
    b.append(1).append(2).append(3);
    CHECK(b.len == 2);
    CHECK(b.buf[1] == 2);
}

TEST_CASE("tool-libs: NDEBUG: Queue drops when full, pop on empty is harmless") {
    Queue<int> q(2);
    q.push(1);
    q.push(2);
    q.push(3); // dropped
    CHECK(q.size() == 2);
    CHECK(q.pop() == 1);
    CHECK(q.pop() == 2);
    CHECK(q.pop() == 0); // default value instead of stale data
    q.drop();
    CHECK(q.size() == 0);
    q.push(4);
    CHECK(q.size() == 1);
    CHECK(q.pop() == 4);
}

TEST_CASE("tool-libs: NDEBUG: Frame::pack drops what doesn't fit") {
    Frame f;
    for (int i = 0; i < 17; ++i) f.pack<uint64_t>(i); // 128 bytes fit
    CHECK(f.b.len == 128);
    CHECK(f.remaining() == 128);
}

TEST_CASE("tool-libs: NDEBUG: Slice reads beyond its range give a default value") {
    Buffer<int> b{1, 2, 3};
    Slice<int> tail{b, 2};
    CHECK(tail[0] == 3);
    CHECK(tail[1] == 0);
    Slice<int> past{b, 5};
    CHECK(past[0] == 0);
}
