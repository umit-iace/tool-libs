#include <core/kern.h>
#include <comm/canopen.h>
#include <linux/canimpl.h>

Kernel k;
using namespace CAN;
HW can ("can0");
Open::Dispatch canopen(can);

struct DEV : public Open::Device {
    DEV() : Device(canopen, 0x0F) { }
    void callback(Open::SDO rq) override { }
    void callback(Open::TPDO rq) override { }
} dev{};


int main() {
    k.every(1, [](uint32_t t, uint32_t dt) {
            canopen.process();
            });
    k.every(2000, [](uint32_t t, uint32_t dt) {
            dev.w32(0x3005, 0x1, t);
            });
    k.run();
    return 0;
};

