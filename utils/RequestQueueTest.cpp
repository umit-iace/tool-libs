/** @file RequestQueueTest.cpp
 *
 * Copyright (c) 2019 IACE
 */

#define BOOST_TEST_MODULE QueueTest

#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <cstdio>
#include "RequestQueue.h"

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

    void begin(TestRequest &r) override {
        if (!bStop) {
            r.doit();
            printf("done.\n");
            end();
        } else {
            printf("skp\n");
        }
    }

    void timeout(TestRequest &r) override {
        printf("aborted pin: %d\n", r.pin);
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

    using RequestQueue::exists;
};


BOOST_AUTO_TEST_CASE( emptyRequestTest ) {
    printf("\n\nemptyRequestTest\n\n");
    TestRequest::reset();
    const int fifosize = 10;
    const int testsize = 6;
    TestFifo fifo(fifosize);

    for (int i = 5; i < 5+testsize; ++i) {
        auto R = TestRequest(i, 0x11f7a345+i);
        fifo.request(R);
    }
    BOOST_CHECK_EQUAL(TestRequest::list[0], 5);
    BOOST_CHECK_EQUAL(TestRequest::list[testsize-1], 5+testsize-1);

}

BOOST_AUTO_TEST_CASE(overfullTest) {
    printf("\n\noverfullTest\n\n");
    TestRequest::reset();
    BOOST_CHECK_EQUAL(TestRequest::list[0], 0);
    BOOST_CHECK_EQUAL(TestRequest::index, 0);
    int fifosize = 5;
    int testsize = 10;
    TestFifo fifo(fifosize);
    fifo.stop();
    for (int i = 5; i < 5 + testsize; ++i) {
        auto R = TestRequest(i, 0x11f7a345+i);
        BOOST_CHECK_EQUAL(fifo.request(R), i-5 < fifosize ? 0 : -1);
    }
    // make sure request times out
    fifo.getTime();
    fifo.getTime();
    BOOST_CHECK_EQUAL(TestRequest::list[0], 0);
    BOOST_CHECK_EQUAL(TestRequest::list[testsize - 1], 0);
    fifo.start();
    auto r = TestRequest(121, 0xbabeface);
    BOOST_CHECK_EQUAL(fifo.request(r), -1);
    BOOST_CHECK_EQUAL(TestRequest::list[0], 5);
    BOOST_CHECK_EQUAL(TestRequest::list[fifosize - 1], fifosize-1+5);
    BOOST_CHECK_EQUAL(fifo.request(r), 0);
    BOOST_CHECK_EQUAL(TestRequest::list[fifosize], 121);
}


BOOST_AUTO_TEST_CASE(findRequestTest) {
    printf("\n\nfindRequestTest\n\n");
    TestRequest::reset();
    TestFifo fifo(2);
    auto R = TestRequest(0, 0x11f7a345);
    fifo.request(R);
    BOOST_CHECK_EQUAL(fifo.exists(R), false);
    fifo.stop();
    BOOST_CHECK_EQUAL(fifo.request(R), 0);
    fifo.start();
    fifo.getTime();
    fifo.getTime(); // make sure queue aborts on next poll()
    BOOST_CHECK_EQUAL(fifo.exists(R), true);
    fifo.request(R);
    BOOST_CHECK_EQUAL(fifo.exists(R), false);
}

