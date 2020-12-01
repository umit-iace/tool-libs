/** @file DynamicArray.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef DYNAMICARRAY_H
#define DYNAMICARRAY_H

#include <cstddef>
#include <cstdlib>

/**
 * simple implementation of a dynamically allocated array
 */
template<typename T>
class DynamicArray {
private:
    ///\cond false
    const unsigned int min; /// minimum capacity for objects
    unsigned int cap = 0; /// current total capacity for objects
    unsigned int size = 0; /// current number of objects in array
    T *storage = nullptr;
    ///\endcond
public:
    /**
     * create new array
     * @param min starting capacity of array
     */
    DynamicArray(size_t min = 2) : min(min) {
        cap = min;
        storage = (T *)malloc(cap * sizeof(T));
        while (!storage); // can't alloc.
    }

    /**
     * destructor also calls destructors on remaining objects in array
     */
    ~DynamicArray() {
        for (unsigned int i = 0; i < size; ++i) {
            storage[i].~T();
        }
        free(storage);
    }

    /**
     * put new object at end of array
     * @param obj
     * @return true if successful
     */
    bool push_back(T &obj) {
        if (size == cap) {
            T *tmp = (T *)realloc(storage, cap * 2 * sizeof(T));
            if (!tmp) {
                return false;
            }
            storage = tmp;
            cap *= 2;
        }
        storage[size] = obj;
        ++size;
        return true;
    }

    /**
     * remove last element from array
     *
     * also destructs the removed element
     */
    void pop_back() {
        if (!size) {
            return;
        }
        storage[size--].~T();
        if (size > cap / 4 || cap / 2 < min) {
            return;
        }
        T *tmp = (T *)realloc(storage, cap / 2 * sizeof(T));
        if (!tmp) {
            return;
        }
        storage = tmp;
        cap /= 2;
    }

    /**
     * current amount of objects in array
     * @return len
     */
    size_t len() {
        return size;
    }

    /**
     * array subscript operator overload
     *
     * allows access to objects in the array.
     *
     * does not check for correct access!
     * @param idx index of accessed object
     * @return reference to object at index idx
     */
    T &operator[](size_t idx) {
        return storage[idx];
    }
};

#endif //DYNAMICARRAY_H
