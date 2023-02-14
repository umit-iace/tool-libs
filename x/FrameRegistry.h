#pragma once
#include "utils/Min.h"

/**
 * Registry for dispatching frames to their required destinations
 */
struct FrameRegistry {
    struct Callable {
        virtual ~Callable(){}
        virtual void call(Frame &f)=0;
    };
    using Func = void (*)(Frame &f);
    struct FuncHandler: public Callable {
        Func func{};
        FuncHandler(Func f) : func(f) { }
        void call(Frame &f) override {
            return func(f);
        }
    };
    template<typename T>
    struct MethodHandler: public Callable {
        typedef void(T::*Method)(Frame &f);
        T* base;
        Method method;
        MethodHandler(T* b, Method m) : base(b), method(m) { }
        void call(Frame &f) override {
            return (base->*method)(f);
        }
    };

    Callable *list[64]{};
    /** register function to handle all frames with given id */
    void setHandler(uint8_t id, Func f) {
        assert(list[id] == nullptr);
        list[id] = new FuncHandler(f);
    }
    /** register class instance & method to handle all frames with given id */
    /* template<typename T> */
    /* using Method = MethodHandler<T>::Method; */
    template<typename T>
    void setHandler(uint8_t id, T& base,
            typename MethodHandler<T>::Method method) {
        assert(list[id] == nullptr);
        list[id] = new MethodHandler(&base, method);
    }

    /** handle given Frame and consume it
     *
     * you potentially have to call `.handle(std::move(frame))`
     * to have the compiler let you do this.
     */
    void handle(Frame &&f) {
        Callable *c = list[f.id];
        if (c) c->call(f);
    }
    ~FrameRegistry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
};
