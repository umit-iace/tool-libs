#include <doctest/doctest.h>
#include <sys/sim.h>
Kernel k;

static uint16_t timestep(const char *arg) {
    char name[] = "sim";
    char *argv[] = {name, (char*)arg};
    return getSimTimeStep(2, argv);
}

TEST_CASE("tool-libs: sim: time step from command line") {
    CHECK(timestep("2") == 500);
    CHECK(timestep("1000") == 1);
}

TEST_CASE("tool-libs: sim: invalid time factor") {
    CHECK(timestep("0") == 1000);
    CHECK(timestep("abc") == 1000);
    CHECK(timestep("65537") == 1000); // would wrap to 1 as uint16_t
}
