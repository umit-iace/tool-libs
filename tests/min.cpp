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
struct __attribute((packed)) BigStruct {
    uint8_t msg[75] ="\n\
        Hello World!             |\n\
        hoi                      |\n";
    double pi=3.14159265;
    uint32_t longs[4] = {123456,0,0, 234567};
    bool tru = true;
    void print(){
        printf("msg=%s pi=%lf l=[%d,%d,%d,%d] tru=%x\n"
                , msg, pi
                , longs[0],longs[1],longs[2],longs[3]
                , tru
              );
    }
} bigsend, bigrecv = { .msg = "", .pi=0, .longs = {0, 0}, .tru=false };

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
        0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x9, 0x40,
        0x10, 0xe, 0x0, 0x0,
        0x13, 0x96, 0x30, 0x3c,
        0x55,
        },
        {
        0xaa, 0xaa, 0xaa,
        0x10, 0xc,
        0x1f, 0x85, 0xeb, 0x51, 0xb8, 0x1e, 0x9, 0x40,
        0x10, 0xe, 0x0, 0x0,
        0xe8, 0x2b, 0xed, 0x26,
        0x55,
        },
    };
    q.push({input[0], sizeof input[0]});
    q.push({input[1], sizeof input[1]});
    while (!min.empty()) {
        Frame f = min.pop();
            double pi = f.unpack<double>();
            uint32_t sph = f.unpack<uint32_t>();
            printf("id=%d pi=%f sph=%d\n", f.id, pi, sph);
    }
}
void testStructroundtrip() {
    Frame f{19};
    f.pack(3.14);
    f.pack((uint32_t)3600);
    f.pack(true);
    recv = f.unpack<decltype(recv)>();
    printf("id=%d pi=%f sph=%d tru=%x\n", f.id, recv.pi, recv.sph, recv.tru);
    f = Frame{18};
    f.pack(send);
    recv = f.unpack<decltype(recv)>();
    printf("id=%d pi=%f sph=%d tru=%x\n", f.id, recv.pi, recv.sph, recv.tru);
    f = Frame{16};
    f.pack(bigsend);
    bigrecv = f.unpack<decltype(bigrecv)>();
    printf("id=%d ", f.id);
    bigrecv.print();
    printf("size of BigStruct: %ld\n", sizeof(bigrecv));
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

