#pragma once

#include <assert.h>

/** registry that maps classes to HAL handles
 *
 * glue for making the HAL work better with classes
 */
template<typename T, typename H, unsigned int sz>
class Registry {
    T *t[sz];
    H *h[sz];
    unsigned int len = 0;
public:
    /* register given class instance to handle */
    void reg(T *inst, H *handle) {
        for (unsigned int i = 0; i < len; ++i) {
            assert(t[i] != inst);
        }
        t[len] = inst;
        h[len] = handle;
        ++len;
    }
    /* get registered class instance from handle */
    T* from(H *handle) {
        for (unsigned int i = 0; i < len; ++i) {
            if (h[i] == handle) return t[i];
        }
        assert(false); // make sure to register the (instance,handle) pair first
        return nullptr;
    }
};
