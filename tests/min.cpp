#include <cstdio>
#include <cassert>
#define log(...)
/* #define log(...) fprintf(stderr, __VA_ARGS__) */
#include <utils/Queue.h>
#include <utils/Buffer.h>
#include <utils/Min.h>
#include <utils/Interfaces.h>
// XXX: rewrite into testing framework

struct Printer : Push<Buffer<uint8_t>> {
    void push(const Buffer<uint8_t> &b) override {
        push(std::move(Buffer<uint8_t>{b}));
    }
    void push(Buffer<uint8_t> &&b) override {
        for (auto c: b) {
            printf("\\%2x", c);
        }
        printf("\n");
    }
};

struct __attribute__((packed)) TestStruct {
    double pi=3.14;
    uint32_t sph=3600;
    bool tru = true;
} send, recv = {
    .pi=0,
    .sph=0,
    .tru=false,
};

void testFrame() {
    Frame f;
    f.pack(3.14);
    f.pack((uint32_t) 3600);

    double pi;
    uint32_t sph;
    f.unPack(pi);
    f.unPack(sph);

    printf("pi=%f\nsph=%d\n", pi, sph);
}
void testTx() {
    Printer p;
    Queue<Buffer<uint8_t>, 0> null;
    Min min{null, p};
    Frame f{1};
    f.pack(3.14);
    f.pack((uint32_t) 3600);

    min.push(f);
    f.id=0x10;
    min.push(f);
}
void testRx() {
    Printer p;
    Queue<Buffer<uint8_t>, 2> q;
    Min min{q, p};
    uint8_t input[][22] = {
        {
        0xaa, 0xaa, 0xaa,
        0x1, 0xc,
        0x40, 0x9, 0x1e, 0xb8, 0x51, 0xeb, 0x85, 0x1f,
        0x0, 0x0, 0xe, 0x10,
        0xe0, 0x29, 0xc2, 0x86,
        0x55,
        },
        {
        0xaa, 0xaa, 0xaa,
        0x10, 0xc,
        0x40, 0x9, 0x1e, 0xb8, 0x51, 0xeb, 0x85, 0x1f,
        0x0, 0x0, 0xe, 0x10,
        0x1b, 0x94, 0x1f, 0x9c,
        0x55,
        },
    };
    q.push({input[0], sizeof input[0]});
    q.push({input[1], sizeof input[1]});
    while (!min.empty()) {
        Frame f = min.pop();
            double pi;
            uint32_t sph;
            f.unPack(pi);
            f.unPack(sph);
            printf("id=%d pi=%f sph=%d\n", f.id, pi, sph);
    }
}
void testStructroundtrip() {
    Frame f{19};
    f.pack(true);
    f.pack((uint32_t)3600);
    f.pack(3.14);
    f.unPack(recv);
    printf("id=%d pi=%f sph=%d tru=%x\n", f.id, recv.pi, recv.sph, recv.tru);
    f = Frame{18};
    f.pack(send);
    f.unPack(recv);
    printf("id=%d pi=%f sph=%d tru=%x\n", f.id, recv.pi, recv.sph, recv.tru);
}
int main() {
    testFrame();
    testTx();
    testRx();
    testStructroundtrip();
    Queue<Buffer<uint8_t>, 3> q;
    Min m{q, q};
    Frame f{18}, r{};
    f.pack(send);
    m.push(f);
    if (!m.empty()) {
        r = m.pop();
        printf("id=%d\n", r.id);
        Printer{}.push(r.b);
    }
}

