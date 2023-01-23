#include <cstdio>
#include <cassert>
#include <utils/Queue.h>
#include <utils/Buffer.h>
#define DEBUGLOG(...) fprintf(__VA_ARGS__)
#include <utils/Min.h>
// XXX: rewrite into testing framework

struct Printer : RequestQueue<Buffer> {
    void rqBegin(Buffer *b) override {
        for (uint32_t i = 0; i < b->len; ++i) {
            printf("\\%2x", b->payload[i]);
        }
        printf("\n");
        rqEnd();
    }
    void rqTimeout(Buffer *b) override {}
    unsigned long getTime() override {return 0;}
    Printer() : RequestQueue(10, 0) {}
};
struct Bufferer : RequestQueue<Buffer> {
    Queue<Buffer, 2> q;
    void rqBegin(Buffer *b) override {
        q.push(*b);
    }
    void rqTimeout(Buffer *b) override {}
    unsigned long getTime() override {return 0;}
    Bufferer() : RequestQueue(10, 0) {}
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
    Min min{&p};
    Frame f{1};
    f.pack(3.14);
    f.pack((uint32_t) 3600);

    /* min.send(1, f.b.payload, f.b.len); */
    min.send(f);
    f.id=0x10;
    min.send(f);
    /* min.send(0x10, f.b.payload, f.b.len); */
}
void testRx() {
    Printer p;
    Min min{&p};
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
    for (int i = 0; i < 22; ++i) {
        min.recv(input[0][i]);
    }
    for (int i = 0; i < 22; ++i) {
        min.recv(input[1][i]);
    }
    while (min.available()) {
        Frame f = min.getFrame();
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
    Bufferer b{};
    Printer p{};
    Min m{&b};
    Frame f{18}, r{};
    f.pack(send);
    m.send(f);
    auto buf = b.q.pop();
    for (size_t ix=0; ix < buf.len; ++ix) {
        m.recv(buf.at(ix));
    }
    r = m.getFrame();
    m = Min{&p};
    printf("id=%d\n", r.id);
    /* for (size_t ix=0; r.id != 0; ++ix, r=m.getFrame()) { */
    /*     m.recv(f.b.at(ix)); */
    /* } */
    m.send(r);

}

