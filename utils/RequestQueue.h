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
     * implement this to get the needed behavior
     *
     * useful functions to get there:
     *   * add
     *   * find
     *   * check
     * @param r Request to add to queue
     * @return 0 if successful
     */
    virtual short request(Request r) = 0;

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

    /// possible return values of RequestQueue methods
    enum qRet {
        QOK,            ///< no need to take action
        QFULL,          ///< queue is full, unable to add request
    }

    /**
     * add a request to the queue
     * @param r Request to add
     * @return 0 if successful
     */
    enum qRet add(Request &r) {
        enum qRet ret = QFULL;
        if (!bFull) {
            queue[iInIndex] = r;
            timeOf[iInIndex] = getTime();

            inc(iInIndex);
            bFull = iInIndex == iOutIndex && bActive;
            ret = QOK;
        }
    return ret;
    }

    /**
     * check if some action needs to be taken, and take it
     */
    void check() {
        if (TIMEOUT && getTime() - timeOf[iOutIndex] > TIMEOUT) {
            abort();
        } else if (!bActive) {
            bActive = true;
            begin();
        }
    }

    /**
     * check if request is currently in queue
     * @param r request to look for
     * @return true if the request is found in the queue
     */
    bool find(Request &r) {
        for (auto index = iOutIndex; index != iInIndex; inc(index)) {
            if (r == queue[index]) {
                return true;
            }
        }
        return false;
    }

    /**
     * get request at outindex
     */
    Request &current() {
        return queue[iOutIndex];
    }

    /**
     * process a given request
     */
    virtual void begin() = 0;

    /**
     * abort a given request
     */
    virtual void abort() = 0;

    /**
     * end processing the next request in the queue.
     *
     * call this function when processing the request is done.
     * removes processed request from list, increments outindex
     */
    void end() {
        // remove request
        queue[iOutIndex].~Request();
        timeOf[iOutIndex] = 0;

        inc(iOutIndex);
        bFull = false;

        if (iOutIndex == iInIndex) {
            // queue empty
            bActive = false;
        } else {
            begin();
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
