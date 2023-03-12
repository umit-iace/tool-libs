#include <doctest/doctest.h>
#include <cassert>
#include <iostream>
#include <x/TimedFuncRegistry.h>
#include <x/Schedule.h>
using namespace std;

void callme(uint32_t t, uint32_t dt) {
    cout << "t: " << t << " dt: " << dt << endl;
}
struct Caller {
    void callme(uint32_t t, uint32_t dt) {
        cout << "Classcall: t: " << t << " dt: " << dt << endl;
    }
};

TEST_CASE("recurring time schedule") {
    Schedule::Recurring::Registry tfr;
    Scheduler s;
    Caller c;
    tfr.every(15, callme);
    tfr.every(20, c, &Caller::callme);

    for (uint32_t time=0; time < 100; ++time) {
        s.schedule(time, tfr);
        s.run();
    }
}

