#include <cassert>
#include <utils/Schedule.h>
#include <iostream>
using namespace std;

void printstuff(uint32_t time_ms, uint32_t dt_ms) {
    cout << time_ms << " " << dt_ms << endl;
}

int main() {
    TimeSchedule<4> s;

    s.append(printstuff, {305, 10});
    s.append(printstuff, {306, 10});
    s.append(printstuff, {307, 20});
    s.append(printstuff, {308, 60});
    while (!s.empty()) {
        s.run();
    }
}
