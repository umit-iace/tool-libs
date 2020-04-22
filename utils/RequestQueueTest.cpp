/** @file RequestQueueTest.cpp
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
public:
    TestFifo(unsigned int l): RequestQueue(l, 2) {}

    int getIn() {
        return this->iInIndex;
    }

    int getOut() {
        return this->iOutIndex;
    }

    bool isActive() {
        return this->bActive;
    }

    void processRequest(TestRequest &r) {
        printf("Pin: %d\n\t on port: %X\n"
               "mem: %f\n", r.pin, r.gpio, r.dMem);
        if (!bStop) {
            printf("processed.\n");
            endProcess();
        } else {
            printf("not processed.\n");
        }
    }

    TestRequest *getQueue(){
        return this->queue;
    }

    unsigned long getTime() override {
        static unsigned long time = 1;
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
    bool find(TestRequest &r) {
        return findRequest(r);
    }
};


BOOST_AUTO_TEST_CASE( emptyRequestTest ) {
        printf("\n\nemptyRequestTest\n\n");
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

        BOOST_CHECK_EQUAL(fifo.isActive(), false);

        BOOST_CHECK_EQUAL(memcmp(fifo.getQueue(), zeros, fifosize*sizeof(TestRequest)), 0);
}

BOOST_AUTO_TEST_CASE(overfullTest) {
        printf("\n\noverfullTest\n\n");
        int fifosize = 5;
        int testsize = 10;
        TestFifo fifo(fifosize);
        fifo.stop();
        for (int i = 0; i < testsize; ++i) {
            auto R = TestRequest(i, 0x11f7a345+i);
            BOOST_CHECK_EQUAL(fifo.request(R), i < fifosize ? 0 : -1);
        }
        fifo.start();
        auto r = TestRequest(121, 0xbabeface);
        BOOST_CHECK_EQUAL(fifo.request(r), -1);
        BOOST_CHECK_EQUAL(fifo.getIn(), 0);
        BOOST_CHECK_EQUAL(fifo.request(r), 0);
        BOOST_CHECK_EQUAL(fifo.getIn(), 1);
}

BOOST_AUTO_TEST_CASE(findRequestTest) {
        printf("\n\nfindRequestTest\n\n");
        TestFifo fifo(2);
        auto R = TestRequest(0, 0x11f7a345);
        fifo.request(R);
        BOOST_CHECK_EQUAL(fifo.find(R), false);
        fifo.stop();
        fifo.request(R);
        BOOST_CHECK_EQUAL(fifo.find(R), true);
}

