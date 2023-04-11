/** @file frameregistry.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/buffer.h>

/** pyWisp communication frame */
struct Frame {
    /** buffer containing the Frame payload */
    Buffer<uint8_t> b{128};
    /** buffer id: [0..63] */
    uint8_t id{};
    /** construct Frame with given id */
    Frame(uint8_t id=0) : id(id) { }
    /** pack value into Frame
     *
     * use `pack<typename>(value)` to be explicit
     **/
    template<typename T>
    Frame &pack(T value) {
        const size_t sz = sizeof(T);
        assert(cursor.pack + sz < b.size);
        memcpy(b.buf+cursor.pack, &value, sz);
        cursor.pack += sz;
        b.len += sz;
        return *this;
    }
    /** unpack into value from Frame
     *
     * use `unPack<typename>(location)` to be explicit
     */
    template<typename T>
    void unPack(T &value) {
        const size_t sz = sizeof(T);
        assert(cursor.unpack + sz < b.size);
        memcpy(&value, b.buf + cursor.unpack, sz);
        cursor.unpack += sz;
    }
    /** return unpacked value from Frame
     *
     * can only be used explicitly as `unpack<typename>`
     */
    template<typename T>
    T unpack() {
        const size_t sz = sizeof(T);
        assert(cursor.unpack + sz < b.size);
        T ret{};
        memcpy(&ret, b.buf + cursor.unpack, sz);
        cursor.unpack += sz;
        return ret;
    }
private:
    struct {
        uint8_t pack, unpack;
    } cursor{};
};

/** Registry for dispatching frames to their registered destinations */
struct FrameRegistry {
    /** Virtual Base for a callable Frame handler */
    struct FrameHandler {
        virtual ~FrameHandler(){}
        virtual void handle(Frame &f)=0;
    };
    /** pure function FrameHandler */
    struct FuncHandler: public FrameHandler {
        typedef void (*Func)(Frame &f);
        Func func{};
        FuncHandler(Func f) : func(f) { }
        void handle(Frame &f) override {
            return func(f);
        }
    };
    /** class method FrameHandler */
    template<typename T>
    struct MethodHandler: public FrameHandler {
        typedef void(T::*Method)(Frame &f);
        T* base;
        Method method;
        MethodHandler(T* b, Method m) : base(b), method(m) { }
        void handle(Frame &f) override {
            return (base->*method)(f);
        }
    };

    /** list of registered handlers */
    FrameHandler *list[64]{};
    /** register pure function FrameHandler for given id */
    void setHandler(uint8_t id, FuncHandler::Func f) {
        assert(list[id] == nullptr);
        list[id] = new FuncHandler(f);
    }
    /** register class method FrameHandler for given id */
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
        FrameHandler *c = list[f.id];
        if (c) c->handle(f);
    }
    ~FrameRegistry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
};
