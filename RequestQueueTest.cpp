/** @file QueueTests.cpp
 *
 * Copyright (c) 2019 IACE
 */

#define BOOST_TEST_MODULE QueueTest

#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <cstdio>
#include "RequestQueue.h"
class TestRequest {
public:
    double dMem = 3.6;
    char **sTest = nullptr;
    struct {
        unsigned int pin;
        unsigned int gpio;
    };

    TestRequest() {
        dMem = 0;
        pin = 0;
        gpio = 0;
    }

    TestRequest(int p, unsigned int g) :
            pin(p), gpio(g)
    {
    }

    ~TestRequest() {
        pin = 0;
        gpio = 0;
        dMem = 0;
    }
};

class TestFifo: public RequestQueue<TestRequest> {
public:
    TestFifo(unsigned int l): RequestQueue(l) {}

    int getIn() {
        return this->iInIndex;
    }

    int getOut() {
        return this->iOutIndex;
    }

    bool getFirstOp() {
        return this->bFirstOp;
    }

    void processRequest(TestRequest &r) {
        printf("Pin: %d\n\t on port: %X\n"
               "mem: %f\n", r.pin, r.gpio, r.dMem);
        endProcess();
    }

    TestRequest *getQueue(){
        return this->queue;
    }

    unsigned long getTime() override {
        return 0;
    }
};


BOOST_AUTO_TEST_CASE( singleRequestTest ) {
        const int fifosize = 10;
        const int testsize = 6;
        TestFifo fifo(fifosize);
        uint8_t zeros[fifosize*sizeof(TestRequest)] = {};

        BOOST_CHECK_EQUAL(fifo.getIn(), fifo.getOut());

        for (int i = 0; i < testsize; ++i) {
            auto R = TestRequest(i, 0x11f7a345+i);
            fifo.request(R);
        }

        BOOST_CHECK_EQUAL(fifo.getIn(), fifo.getOut());

        BOOST_CHECK_EQUAL(fifo.getIn(), testsize);

        BOOST_CHECK_EQUAL(fifo.getFirstOp(), true);

        BOOST_CHECK_EQUAL(memcmp(fifo.getQueue(), zeros, fifosize*sizeof(TestRequest)), 0);
}
