#pragma once
#include "can.h"

namespace CAN {
namespace Open {
/** CANOpen Service Data Object xfer request
 *
 * this struct shouldn't have to be instantiated by the user directly.
 * use the helper functions of the Device instead.
 */
struct SDO {
    uint32_t data;  ///< up to 4 bytes of data
    uint16_t ix;    ///< index of DO
    uint8_t sub;    ///< subindex of DO
    uint8_t cmd;    ///< type of request
    uint8_t nodeID; ///< node ID of device

    Message toCanMsg() {
        uint32_t id{};
        uint8_t len{};
        switch (cmd>>4 & 0xf) {
        case 0x2: id = nodeID + 0x600; len = 8; break;
        /* case 0x4: id = nodeID + 0x580; len = 4; break; */
        case 0x4: id = nodeID + 0x600; len = 8; break;
        }
        return {
            .data = cmd | ix << 8 | sub << 24 | (uint64_t)data << 32,
            .id = id,
            .opts = { .rtr = 0, .ide = 0, .dlc = 8},
        };
    }
    static SDO fromCanMsg(Message msg) {
        return {
            .data = (uint32_t)(msg.data >> 32),
            .ix = (uint16_t)(msg.data >> 8),
            .sub = (uint8_t)(msg.data >> 24),
            .cmd = (uint8_t)msg.data,
            .nodeID = (uint8_t)(msg.id % 0x80),
        };
    }
};
/** CANOpen Receive Process Data Object Type */
struct RPDO {
    uint8_t N;
    enum : uint8_t {SYNC, CHANGE=255} type;
    uint32_t COB {};
    struct MAP {
        uint16_t ix;
        uint8_t sub;
        uint8_t len;
    };
    Buffer<MAP> map;
    Message write(uint64_t data) const {
        return {.data=data, .id = COB, .opts = {.dlc=8}};
    }
};
/** CANOpen Transmit Process Data Object Type */
struct TPDO {
    uint32_t COB;
    static TPDO fromCanMsg(Message msg) {
        return {.COB = msg.id};
    }
};

/** CANOpen dispatch class
 *
 * use this as a middle layer between the CAN bus and the Device devices to
 * handle translation between the protocols.
 * call this class' `process` directly after the CAN irqHandler
 *
 * cf. https://www.microcontrol.net/wp-content/uploads/2021/10/td-03011e.pdf
 */
struct Dispatch : Sink<SDO>, Sink<Message> {
    Dispatch(CAN &can) : can(can) { }
    CAN &can;
#if defined(DEBUG)
    Logger log;
#endif
    Sink<SDO> *ids[128] {};
    struct PDO {
        Sink<TPDO> *dev[8] {};
        uint16_t cobs[8] {};
        uint8_t n {};
    } pdo;

    using Sink<SDO>::push;
    void push(SDO &&rq) override {
        assert(ids[rq.nodeID] != nullptr); //must register first!
        can.push(rq.toCanMsg());
    }
    using Sink<Message>::push;
    void push(Message &&rq) override { can.push(rq); }

    void registersdo(uint16_t nodeID, Sink<SDO> *dev) {
        assert(nodeID != 0);
        assert(ids[nodeID] == nullptr);
        ids[nodeID] = dev;
    }
    void registerpdo(uint16_t COB, Sink<TPDO> *dev) {
        assert ( pdo.n < 8 );
        pdo.dev[pdo.n] = dev;
        pdo.cobs[pdo.n] = COB;
        pdo.n += 1;
    }

    Sink<TPDO> *isPDO(uint32_t cob) {
        for (int i = 0; i < pdo.n; ++i) {
            if (pdo.cobs[i] == cob) return pdo.dev[i];
        }
        return nullptr;
    }
    Sink<SDO> *isSDO(uint32_t cob) {
        uint16_t service = cob & ~0x7f;
        uint8_t id = cob & 0x7f;
        if (id == 0) return nullptr;
        if (service == 0x80 ||
            service == 0x580 ||
            service == 0x600 ||
            service == 0x700) {
            return ids[id];
        }
        return nullptr;
    }

    void process() {
        // TODO: what to do to CAN messages that _aren't_ CanOpen
        while (!can.empty()) {
            auto msg = can.pop();
            auto pdo = isPDO(msg.id);
            if (pdo != nullptr) {
                pdo->push(TPDO::fromCanMsg(msg));
                continue;
            }
            auto sdo = isSDO(msg.id);
            if (sdo != nullptr) {
                sdo->push(SDO::fromCanMsg(msg));
                continue;
            }
#if defined(DEBUG)
            // don't know what to do with received message!
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
                    );
#endif
        }
    }
};

/** CANOpen device wrapper class
 *
 * inherit and implement `callback` to use this interface on a CANOpen network
 */
struct Device : Sink<TPDO>, Sink<SDO> {
    Device(Dispatch &canopen, uint8_t id) : id(id), out(canopen)
    {
        canopen.registersdo(id, this);
    }
    uint8_t const id {};    ///< CANOpen node ID [1 - 127]
    Dispatch &out;

    struct State {
        Queue<SDO> q{60};
        bool waiting{};
    } state;
    /** implement the callback method to handle incoming data */
    virtual void callback(SDO rq) = 0;
    /** implement the callback method to handle incoming data */
    virtual void callback(TPDO rq) = 0;
    void process() {
        if (state.waiting || state.q.empty()) return;
        out.push(state.q.pop());
        state.waiting = true;
    }
    /** read DO with given index and subindex of this device */
    void read(uint16_t ix, uint8_t sub) {
        state.q.push({.ix=ix, .sub=sub, .cmd=0x40, .nodeID=id});
        process();
    }
    /** write given value to DO at given index and subindex */
    void w8(uint16_t ix, uint8_t sub, uint8_t val) {
        state.q.push({.data=val, .ix=ix, .sub=sub, .cmd=0x2f, .nodeID=id});
        process();
    }
    /** write given value to DO at given index and subindex */
    void w16(uint16_t ix, uint8_t sub, uint16_t val) {
        state.q.push({.data=val, .ix=ix, .sub=sub, .cmd=0x2b, .nodeID=id});
        process();
    }
    /** write given value to DO at given index and subindex */
    void w32(uint16_t ix, uint8_t sub, uint32_t val) {
        state.q.push({.data=val, .ix=ix, .sub=sub, .cmd=0x23, .nodeID=id});
        process();
    }
    /** enable receiving PDO on device */
    void enablepdo(RPDO &pdo) {
        if (pdo.COB == 0) pdo.COB = id + 0x100 * (pdo.N+1);
        disablepdo(pdo);
        w8(0x1400+pdo.N-1, 0x2, pdo.type); // set transmission type
        w8(0x1600+pdo.N-1, 0x0, 0); // clear number of mapped objects
        for (size_t i = 0; i < pdo.map.len; ++i) {
            auto &d = pdo.map[i];
            w32(0x1600+pdo.N-1, i+1, d.ix << 16 | d.sub << 8 | d.len);
        }
        w8(0x1600+pdo.N-1, 0x0, pdo.map.len); // set number of mapped objects
        w32(0x1400+pdo.N-1, 0x1, 1<<30 | pdo.COB);
    }
    void disablepdo(RPDO &pdo) {
        w32(0x1400+pdo.N-1, 0x1, 1<<31); // clear
    }
    /** send data through given pdo */
    void wpdo(const RPDO &rpdo, uint64_t val) {
        out.push(rpdo.write(val));
    }
    /** enable sending PDO on device */
    void enablepdo(TPDO &pdo) {
        //TODO: enable sending on device
        out.registerpdo(pdo.COB, this);
    };
    void disablepdo(TPDO &pdo) {
    };
    using Sink<SDO>::push;
    void push(SDO &&sdo) override {
        state.waiting=false;
        callback(sdo);
        process();
    }
    using Sink<TPDO>::push;
    void push(TPDO &&pdo) override {
        callback(pdo);
        process();
    }
};
}
}
