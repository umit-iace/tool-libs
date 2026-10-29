/** @file registry.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once


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
    /** register given class instance to handle */
    void reg(T *inst, H *handle) {
        for (unsigned int i = 0; i < len; ++i) {
            assert(t[i] != inst); // class instance already registered!
        }
        assert(len < sz); // increase the registry size of the peripheral
        if (len >= sz) return;
        t[len] = inst;
        h[len] = handle;
        ++len;
    }
    /** forget given class instance, e.g. in its destructor */
    void unreg(T *inst) {
        for (unsigned int i = 0; i < len; ++i) {
            if (t[i] != inst) continue;
            for (; i + 1 < len; ++i) {
                t[i] = t[i + 1];
                h[i] = h[i + 1];
            }
            --len;
            return;
        }
    }
    /** get registered class instance from handle
     *
     * nullptr if it isn't registered, e.g. because the registry was full
     */
    T* from(H *handle) {
        for (unsigned int i = 0; i < len; ++i) {
            if (h[i] == handle) return t[i];
        }
        assert(false); // make sure to register the (instance,handle) pair first
        return nullptr;
    }
};
