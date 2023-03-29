#include <doctest/doctest.h>
#include <utility>
#include <utils/buffer.h>
#include <comm/frameregistry.h>

void getStart(Frame &f) {
    static bool frst = true;
    CHECK(f.id == 1);
    CHECK(f.unpack<bool>() == frst);
    frst = false;
}
void getPi(Frame &f) {
    CHECK(f.id == 10);
    CHECK(f.unpack<double>() == doctest::Approx(3.14));
}
struct Exp{
    bool frst = true;
    void getStart(Frame &f) {
        CHECK(f.id == 1);
        CHECK(f.unpack<bool>() == frst);
        frst = false;
    }
    void getPi(Frame &f) {
        CHECK(f.id == 10);
        CHECK(f.unpack<double>() == doctest::Approx(3.14));
    }
};
TEST_CASE("tool-libs: frame registry:") {
    FrameRegistry reg;
    SUBCASE("function handler") {
        reg.setHandler(1, getStart);
        reg.setHandler(10, getPi);
    }
    SUBCASE("method handler") {
        static Exp exp;
        reg.setHandler(1, exp, &Exp::getStart);
        reg.setHandler(10, exp, &Exp::getPi);
    }
    Frame e{1}, f{10}, g{1};
    e.pack(true);
    f.pack(3.14);
    g.pack(false);

    reg.handle(std::move(e));
    reg.handle(std::move(f));
    reg.handle(std::move(g));
}
