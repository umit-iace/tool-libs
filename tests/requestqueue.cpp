/** @file RequestQueueTest.cpp
 *
 * Copyright (c) 2019 IACE
 */

#include <doctest/doctest.h>
#include <cstdint>
#include <cstdio>
#include <utils/RequestQueue.h>

#define LEN (10)

class TestRequest {
public:
    inline static unsigned int list[LEN] = {};
    inline static unsigned int index = 0;
    static void reset() {
        for (int i = 0; i < LEN; ++i) {
            list[i] = 0;
        }
        index = 0;
    }
    double dMem = 3.6;
    char **sTest = nullptr;
    struct {
        unsigned int pin;
        unsigned int gpio;
    };

    TestRequest() : dMem(0), pin(0), gpio(0) { }

    TestRequest(int p, unsigned int g) : pin(p), gpio(g) { }

    ~TestRequest() {
        pin = 0;
        gpio = 0;
        dMem = 0;
    }

    void doit() {
        printf("Pin: %d\n\t on port: %X\n"
               "mem: %f\n", pin, gpio, dMem);
        list[index++] = pin;
    }

    bool operator==(TestRequest &r) {
        bool ret = true;
        ret &= this->dMem == r.dMem;
        ret &= this->sTest == r.sTest;
        ret &= this->pin == r.pin;
        ret &= this->gpio == r.gpio;
        return ret;
    }
};

class TestFifo: public RequestQueue<TestRequest> {
    bool bStop = false;
    uint32_t time = 0;
public:
    TestFifo(unsigned int l): RequestQueue(l, 2) {}

    void rqBegin(TestRequest *r) override {
        if (!bStop) {
            r->doit();
            printf("done.\n");
            rqEnd();
        } else {
            printf("skp\n");
        }
    }

    void rqTimeout(TestRequest *r) override {
        printf("aborted pin: %d\n", r->pin);
    }

    unsigned long getTime() override {
        return time++;
    }

    void stop() {
        printf("stopped\n");
        this->bStop = true;
    }

    void start() {
        printf("started\n");
        this->bStop = false;
    }

    using RequestQueue::rqExists;
};


TEST_CASE("tool-libs: requestqueue: emptyRequestTest") {
    TestRequest::reset();
    const int fifosize = 10;
    const int testsize = 6;
    TestFifo fifo(fifosize);

    for (int i = 5; i < 5+testsize; ++i) {
        auto R = new TestRequest(i, 0x11f7a345+i);
        fifo.request(R);
    }
    CHECK(TestRequest::list[0] == 5);
    CHECK(TestRequest::list[testsize-1] == 5+testsize-1);

}

TEST_CASE("tool-libs: requestqueue: overfullTest") {
    TestRequest::reset();
    CHECK(TestRequest::list[0] == 0);
    CHECK(TestRequest::index == 0);
    int fifosize = 5;
    int testsize = 10;
    TestFifo fifo(fifosize);
    fifo.stop();
    for (int i = 5; i < 5 + testsize; ++i) {
        auto R = new TestRequest(i, 0x11f7a345+i);
        CHECK(fifo.request(R) == (i-5 < fifosize ? 0 : -1));
    }
    // make sure request times out
    fifo.getTime();
    fifo.getTime();
    CHECK(TestRequest::list[0] == 0);
    CHECK(TestRequest::list[testsize - 1] == 0);
    fifo.start();
    CHECK(fifo.request(new TestRequest(121, 0xbabeface)) == -1);
    CHECK(TestRequest::list[0] == 5);
    CHECK(TestRequest::list[fifosize - 1] == fifosize-1+5);
    CHECK(fifo.request(new TestRequest(121, 0xbabeface)) == 0);
    CHECK(TestRequest::list[fifosize] == 121);
}


TEST_CASE("tool-libs: requestqueue: findRequestTest") {
    TestRequest::reset();
    TestFifo fifo(2);
    auto R = new TestRequest(0, 0x11f7a345);
    auto R1 = new TestRequest(1, 0x11f7a345);
    auto R2 = new TestRequest(2, 0x11f7a345);
    fifo.request(R);
    CHECK(fifo.rqExists(R) == false);
    fifo.stop();
    CHECK(fifo.request(R1) == 0);
    fifo.start();
    fifo.getTime();
    fifo.getTime(); // make sure queue aborts on next poll()
    CHECK(fifo.rqExists(R1) == true);
    fifo.request(R2);
    CHECK(fifo.rqExists(R2) == false);
}
