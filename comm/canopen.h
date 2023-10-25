#pragma once
#include "can.h"

namespace CAN {
namespace Open {
struct Device;
/** CANOpen Data Object xfer request
 *
 * this struct shouldn't have to be instantiated by the user directly.
 * use the helper functions of the Device instead.
 */
struct Request {
    Device *dev;     ///< requesting device
    uint32_t data;  ///< up to 4 bytes of data
    uint16_t ix;    ///< index of DO
    uint8_t sub;    ///< subindex of DO
    uint8_t cmd;    ///< type of request
};

/** CANOpen device wrapper class
 *
 * inherit and implement `callback` to use this interface on a CANOpen network
 */
struct Device {
    Device(Sink<Request> &canopen, uint8_t id) : id(id), out(canopen) { }
    uint8_t const id {};    ///< CANOpen node ID [1 - 127]
    Sink<Request> &out;

    struct State {
        Queue<Request> q{60};
        bool waiting{};
    } state;
    /** implement the callback method to handle incoming data */
    virtual void callback(Request rq) = 0;
    void process() {
        if (state.waiting || state.q.empty()) return;
        out.push(state.q.pop());
        state.waiting = true;
    }
    /** read DO with given index and subindex of this device */
    void read(uint16_t ix, uint8_t sub) {
        state.q.push({.dev=this, .ix=ix, .sub=sub, .cmd=0x40});
        process();
    }
    /** write given value to DO at given index and subindex */
    void w8(uint16_t ix, uint8_t sub, uint8_t val) {
        state.q.push({.dev=this, .data=val, .ix=ix, .sub=sub, .cmd=0x2f});
        process();
    }
    /** write given value to DO at given index and subindex */
    void w16(uint16_t ix, uint8_t sub, uint16_t val) {
        state.q.push({.dev=this, .data=val, .ix=ix, .sub=sub, .cmd=0x2b});
        process();
    }
    /** write given value to DO at given index and subindex */
    void w32(uint16_t ix, uint8_t sub, uint32_t val) {
        state.q.push({.dev=this, .data=val, .ix=ix, .sub=sub, .cmd=0x23});
        process();
    }
    /** Receive Process Data Object Type */
    struct RPDO {
        uint8_t N;
        enum : uint8_t {SYNC, CHANGE=255} type;
        uint32_t COB;
        struct MAP {
            uint16_t ix;
            uint8_t sub;
            uint8_t len;
        };
        Buffer<MAP> map;
    };
    /** enable receiving PDO on device */
    void enable(RPDO pdo) {
        if (pdo.COB == 0) pdo.COB = id + 0x100 * (pdo.N-1);
        w32(0x1400+pdo.N-1, 0x1, 1<<31); // clear first
        w8(0x1400+pdo.N-1, 0x2, pdo.type); // set transmission type
        w8(0x1600+pdo.N-1, 0x0, 0); // clear number of mapped objects
        for (size_t i = 0; i < pdo.map.len; ++i) {
            auto &d = pdo.map[i];
            w32(0x1600+pdo.N-1, i+1, d.ix << 16 | d.sub << 8 | d.len);
        }
        w8(0x1600+pdo.N-1, 0x0, pdo.map.len); // set number of mapped objects
        w32(0x1400+pdo.N-1, 0x1, pdo.COB);
    };
    /** Transmit Process Data Object Type */
    /* struct TPDO { */
    /*     uint8 */
    /* void enable(const TPDO &pdo) { */
    /* }; */
};
/** CANOpen dispatch class
 *
 * use this as a middle layer between the CAN bus and the Device devices to
 * handle translation between the protocols.
 * call this class' `process` directly after the CAN irqHandler
 *
 * cf. https://www.microcontrol.net/wp-content/uploads/2021/10/td-03011e.pdf
 */
struct Dispatch : Sink<Request> {
    Dispatch(CAN &can) : can(can) { }
    CAN &can;
#if defined(DEBUG)
    Logger log;
#endif
    Device *ids[128] {};
    struct PDO {
        Device *dev[8] {};
        uint16_t cobs[8] {};
        uint8_t n {};
    } pdo;

    using Sink<Request>::push;
    void push(Request &&rq) override {
        assert(ids[rq.dev->id] == nullptr);
        ids[rq.dev->id] = rq.dev;
        can.push(msgFromRq(rq));
    }
    void registerpdo(uint16_t COB, Device *dev) {
        assert ( pdo.n < 8 );
        pdo.dev[pdo.n] = dev;
        pdo.cobs[pdo.n] = COB;
        pdo.n += 1;
    }

    void process() {
        // TODO: what to do to CAN messages that _aren't_ CanOpen
        while (!can.empty()) {
            auto msg = can.pop();
            Device *receiver = findregistered(msg.id);
            if (!receiver) {
                // don't know what to do with received message!
#if defined(DEBUG)
                log.warn("COdispatch: [id: %d] [x%2x x%2x x%2x x%2x x%2x x%2x x%2x x%2x]\n",
                        msg.id,
                        (uint8_t)(msg.data),
                        (uint8_t)(msg.data>>8),
                        (uint8_t)(msg.data>>16),
                        (uint8_t)(msg.data>>24),
                        (uint8_t)(msg.data>>32),
                        (uint8_t)(msg.data>>40),
                        (uint8_t)(msg.data>>48),
                        (uint8_t)(msg.data>>56)
                        )
#endif
                continue;
            }
            receiver->callback(rqFromMsg(receiver,msg));
            receiver->state.waiting=false;
            receiver->process();
        }
    }
    static Request rqFromMsg(Device *dev, Message msg) {
        return {
            .dev = dev,
            .data = (uint32_t)(msg.data >> 32),
            .ix = (uint16_t)(msg.data >> 8),
            .sub = (uint8_t)(msg.data >> 24),
            .cmd = (uint8_t)msg.data,
        };
    }
    static Message msgFromRq(Request rq) {
        uint8_t len{};
        uint32_t id{};
        switch (rq.cmd & 0xf) {
        case 0xf: len = 1; break;
        case 0xb: len = 2; break;
        case 0x3: len = 4; break;
        case 0:break;
#if defined(DEBUG)
        default: log.warn("COdispatch: can't use len indicator: [0x%x]\n", rq.cmd &0xf);
#endif
        }
        switch (rq.cmd>>4 & 0xf) {
        case 0x2: id = rq.dev->id + 0x600; break;
        case 0x4: id = rq.dev->id + 0x580; break;
        }
        return {
            .data = rq.cmd | rq.ix << 8 | rq.sub << 24 | (uint64_t)rq.data << 32,
            .id = id,
            .opts = { .rtr = 0, .ide = 0, .dlc = (uint8_t)(len + 4)},
        };
    }

    Device *findregistered(uint32_t cob) {
        // check pdos first
        for (int i = 0; i < pdo.n; ++i) {
            if (pdo.cobs[i] == cob) return pdo.dev[i];
        }
        uint16_t service = cob & ~0x7f;
        uint8_t id = cob & 0x7f;
        if (id == 0) return nullptr;
        if (service == 0x80 ||
            service == 0x580 ||
            service == 0x600 ||
            service == 0x700) {
            Device *tmp = ids[id];
            ids[id] = nullptr;
            return tmp;
        }
        return nullptr;
    }
};
}
}
