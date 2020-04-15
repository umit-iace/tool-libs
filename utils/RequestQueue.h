/** @file RequestQueue.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H

/**
 * Abstract class implementing a request queue
 *
 * in case your specific Request class uses dynamically allocated memory,
 * make sure its copy operator and destructor work correctly.
 */
template<typename Request>
class RequestQueue {
protected:
    unsigned int FIFOLENGTH;
    Request *queue;
    unsigned long long *timeOf = {};
    unsigned int iInIndex = 0;
    unsigned int iOutIndex = 0;
    bool bFirstOp = true;

public:
    /**
     * create a new request queue for a specific request type
     *
     * @param length length of queue
     */
    RequestQueue(unsigned int length) {
        FIFOLENGTH = length;
        queue = new Request[length]();
        timeOf = new unsigned long long[length]();
    }

    ~RequestQueue() {
        delete[] queue;
        delete[] timeOf;
    }

    /**
     * add a request to the queue
     *
     * TODO: overfilling the queue is still possible
     */
    void request(Request &r) {
        while (iInIndex == iOutIndex && !bFirstOp) {
            // intentionally empty. shouldn't happen
            asm("nop");
        }

        queue[iInIndex] = r;
        timeOf[iInIndex] = getTime();

        iInIndex = (iInIndex + 1) % FIFOLENGTH;
        if (bFirstOp) {
            this->initProcess();
        } else {
            // TODO timeouts
        }
    }

protected:
    /**
     * get request at outindex
     */
    Request &lastRequest() {
        return queue[iOutIndex];
    }

    /**
     * end processing the next request in the queue.
     *
     * call this function when processing the request is done.
     * removes processed request from list, increments outindex
     */
    void endProcess() {
        queue[iOutIndex].~Request();
        timeOf[iOutIndex] = 0;
        iOutIndex = (iOutIndex + 1) % FIFOLENGTH;
        initProcess();
    }

    /**
     * initialize processing the next request in the queue
     */
    void initProcess() {
        if (iOutIndex == iInIndex) { // this was the last request in the queue
            bFirstOp = true;
            return;
        }
        bFirstOp = false;
        processRequest(queue[iOutIndex]);
    }

    /**
     * process a given request
     */
    virtual void processRequest(Request &) = 0;

    /**
     * get current time in ms
     */
    virtual unsigned long getTime() = 0;
};

#endif //REQUESTQUEUE_H
