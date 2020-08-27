/** @file RequestQueue.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef REQUESTQUEUE_H
#define REQUESTQUEUE_H

/**
 * Abstract class implementing a request queue that is useful for
 * making things happen asynchronously
 *
 * in case your specific Request class uses dynamically allocated memory,
 * make sure its copy operator and destructor work correctly.
 *
 * if you use the exists(Request) functionality, the `==` operator needs to
 * be correctly defined as well.
 *
 *
 * in order to use this queueing system, inherit it, and make sure to
 * implement the virtual methods according to your hardware, and expected
 * behavior.
 *
 * When processing of a request is finished, you must call `end`
 * in order to progress the queue!
 */
template<typename Request>
class RequestQueue {
public:
    /**
     * publicly visible request method
     *
     * override this to get the needed behavior
     *
     * useful functions to get there:
     *   * rqAdd
     *   * rqExists
     *   * rqPoll
     * @param r Request to add to queue
     * @return 0 if successful
     */
    virtual short request(Request r) {
        short ret = rqAdd(r);
        rqPoll();
        return ret;
    }

private:
    const unsigned int QUEUELENGTH;
    const unsigned int TIMEOUT;
    Request *queue = nullptr;
    unsigned long *timeOf = nullptr;
    unsigned int todo = 0;
    unsigned int iInIndex = 0;
    unsigned int iOutIndex = 0;
    bool bActive = false;
    bool bFull = false;

protected:
    /**
     * create a new request queue for a specific request type
     *
     * @param length length of queue
     * @param timeout timeout in ms
     */
    RequestQueue(unsigned int length, unsigned int timeout) :
            QUEUELENGTH(length), TIMEOUT(timeout) {
        queue = new Request[length]();
        timeOf = new unsigned long[length]();
    }

    ~RequestQueue() {
        delete[] queue;
        delete[] timeOf;
    }

    /**
     * add a request to the queue
     * @param r Request to add
     * @return 0 if successful, -1 otherwise
     */
    short rqAdd(Request &r) {
        if (bFull) {
            return -1;
        }
        queue[iInIndex] = r;
        todo++;
        /* timeOf[iInIndex] = getTime(); */

        inc(iInIndex);
        bFull = iInIndex == iOutIndex && bActive;
        return 0;
    }

    /**
     * check if some action needs to be taken, and take it
     */
    void rqPoll() {
        if (TIMEOUT && bActive && getTime() - timeOf[iOutIndex] > TIMEOUT) {
            bActive = false;
            rqTimeout(queue[iOutIndex]);
        }
        if (!bActive && todo){
            bActive = true;
            timeOf[iOutIndex] = getTime();
            rqBegin(queue[iOutIndex]);
        }
    }

    /**
     * check if request exists in queue
     * @param r request to look for
     * @return true if request is in queue
     */
    bool rqExists(Request &r) {
        for (auto index = iOutIndex; index != iInIndex; inc(index)) {
            if (r == queue[index]) {
                return true;
            }
        }
        return false;
    }

    /**
     * get request which is being handled
     */
    Request &rqCurrent() {
        return queue[iOutIndex];
    }

    /**
     * start callback to application.
     *
     * start processing the current request
     */
    virtual void rqBegin(Request &r) = 0;

    /**
     * timeout callback to application.
     *
     * this may be a good place to consider aborting the current request
     *
     * if the request should be taken out of the queue, call end()
     * in this method. otherwise the request will be restarted.
     */
    virtual void rqTimeout(Request &r) = 0;

    /**
     * end processing of the current request
     *
     * call this function in the application when processing the
     * request is done.\n
     * removes processed request from list, moves on to the next.
     */
    void rqEnd() {
        // remove request
        queue[iOutIndex].~Request();
        timeOf[iOutIndex] = 0;
        todo--;

        inc(iOutIndex);
        bFull = false;

        if (iOutIndex == iInIndex) {
            // queue empty
            bActive = false;
        }
        if (bActive) {
            timeOf[iOutIndex] = getTime();
            rqBegin(queue[iOutIndex]);
        }
    }

    /**
     * get current time in ms
     */
    virtual unsigned long getTime() = 0;

private:
    /**
     * inplace increment index with wraparound
     * @param index
     * @return incremented index
     */
    unsigned int inc(unsigned int &index) {
        return index = (index + 1) % QUEUELENGTH;
    }
};

#endif //REQUESTQUEUE_H
