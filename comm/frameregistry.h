/** @file frameregistry.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <utils/buffer.h>

/** pyWisp communication frame */
struct Frame {
    /** buffer containing the Frame payload */
    Buffer<uint8_t> b = 128;
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
        assert(cursor.pack + sz <= b.size);
        // dropped if it doesn't fit, also in release builds (NDEBUG)
        if (cursor.pack + sz > b.size) return *this;
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
        // only read data that was actually received
        assert(cursor.unpack + sz <= b.len);
        if (cursor.unpack + sz > b.len) return;
        memcpy(&value, b.buf + cursor.unpack, sz);
        cursor.unpack += sz;
    }
    /** return unpacked value from Frame
     *
     * can only be used explicitly as `unpack<typename>`
     */
    /** number of received bytes not unpacked yet */
    size_t remaining() const {
        return b.len > cursor.unpack ? b.len - cursor.unpack : 0;
    }
    template<typename T>
    T unpack() {
        const size_t sz = sizeof(T);
        // only read data that was actually received
        assert(cursor.unpack + sz <= b.len);
        T ret{};
        if (cursor.unpack + sz > b.len) return ret;
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

    /** number of possible Frame ids */
    static constexpr uint8_t IDS = 64;
    /** list of registered handlers */
    FrameHandler *list[IDS]{};
    /** register pure function FrameHandler for given id [0..63] */
    void setHandler(uint8_t id, FuncHandler::Func f) {
        set(id, new FuncHandler(f));
    }
    /** register class method FrameHandler for given id [0..63] */
    template<typename T>
    void setHandler(uint8_t id, T& base,
            typename MethodHandler<T>::Method method) {
        set(id, new MethodHandler(&base, method));
    }
    /** unregister the handler of given id, e.g. before its object dies
     *
     * don't call this from within the handler being removed
     */
    void removeHandler(uint8_t id) {
        if (id >= IDS) return;
        delete list[id];
        list[id] = nullptr;
    }

    /** handle given Frame and consume it
     *
     * you potentially have to call `.handle(std::move(frame))`
     * to have the compiler let you do this.
     */
    void handle(Frame &&f) {
        if (f.id >= IDS) return;
        FrameHandler *c = list[f.id];
        if (c) c->handle(f);
    }
    FrameRegistry() = default;
    // owns the handlers
    FrameRegistry(const FrameRegistry &) = delete;
    FrameRegistry& operator=(const FrameRegistry &) = delete;
    ~FrameRegistry() {
        for (auto &entry: list) {
            delete entry;
        }
    }
private:
    void set(uint8_t id, FrameHandler *h) {
        assert(id < IDS && list[id] == nullptr);
        if (id >= IDS) {
            delete h;
            return;
        }
        delete list[id];
        list[id] = h;
    }
};
