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
 *
 * if you use the findRequest functionality, the `==` operator needs to
 * be correctly defined as well.
 *
 *
 * in order to use this Queue system, inherit it, and make sure to
 * implement the `getTime` according to your hardware,
 * and the `processRequest` function to process your specific request.
 *
 * When processing a request is truly done, you must call `endProcess`
 * in order to progress the queue!
 */
template<typename Request>
class RequestQueue {
public:
    /**
     * publicly visible request method
     *
     * by default simply adds request to queue
     *
     * can be overloaded
     * @param r Request to add to queue
     * @return 0 if successful
     */
    virtual int request(Request r) {
        return addRequest(r);
    }
#ifdef BOOST_TEST_MODULE
protected:
#else
private:
#endif
    const unsigned int QUEUELENGTH;
    const unsigned int TIMEOUT;
    Request *queue = nullptr;
    unsigned long long *timeOf = nullptr;
    unsigned int iInIndex = 0;
    unsigned int iOutIndex = 0;
    bool bActive = false;
    bool bFull = false;

protected:
    /**
     * add a request to the queue
     * @param r Request to add
     * @return 0 if successful
     */
    int addRequest(Request &r) {
        int ret = -1;
        if (!this->bFull) {
            queue[iInIndex] = r;
            timeOf[iInIndex] = getTime();

            inc(iInIndex);
            bFull = iInIndex == iOutIndex && this->bActive;
            ret = 0;
        }

        if (!this->bActive) {
            this->bActive = true;
            this->processRequest(queue[iOutIndex]);
        } else if (TIMEOUT && getTime() - timeOf[iOutIndex] > TIMEOUT) {
            // update timing to reflect restart
            for (auto index = iOutIndex; index != iInIndex; inc(index)) {
                timeOf[index] = getTime();
            }
            // restart
            this->processRequest(queue[iOutIndex]);
        }
        return ret;
    }

    /**
     * create a new request queue for a specific request type
     *
     * @param length length of queue
     * @param timeout timeout in [ms] before retransmission of unanswered request
     */
    RequestQueue(unsigned int length, unsigned int timeout) :
            QUEUELENGTH(length), TIMEOUT(timeout) {
        queue = new Request[length]();
        timeOf = new unsigned long long[length]();
    }

    ~RequestQueue() {
        delete[] queue;
        delete[] timeOf;
    }

    /**
     * inplace increment index with wraparound
     * @param index
     * @return incremented index
     */
    unsigned int inc(unsigned int &index) {
        return index = (index + 1) % QUEUELENGTH;
    }

    /**
     * check if request is currently in queue
     * @param r request to look for
     * @param updateTime if true, updates found request time to _now_
     * @return true if the request is found in the queue
     */
    bool findRequest(Request &r, bool updateTime = false) {
        for (auto index = iOutIndex; index != iInIndex; inc(index)) {
            if (r == queue[index]) {
                if (updateTime) {
                    timeOf[index] = getTime();
                }
                return true;
            }
        }
        return false;
    }

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
        // remove request
        queue[iOutIndex].~Request();
        timeOf[iOutIndex] = 0;

        inc(iOutIndex);
        bFull = false;

        if (iOutIndex == iInIndex) {
            // queue empty
            bActive = false;
        } else {
            processRequest(queue[iOutIndex]);
        }
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
