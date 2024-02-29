#pragma once
#include "can.h"

namespace CAN {
/** cf. https://www.can-cia.org/fileadmin/resources/documents/brochures/co_poster.pdf */
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
        return {
            .data = cmd | ix << 8 | sub << 24 | (uint64_t)data << 32,
            .id = (uint32_t)0x600 + nodeID,
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
struct PDOMap {
    uint16_t ix;
    uint8_t sub;
    uint8_t len;
};
/** CANOpen Receive Process Data Object Type */
struct RPDO {
    uint8_t N;
    enum : uint8_t {SYNC, CHANGE=255} type;
    uint32_t COB {}; // CAN OBject ID
    Buffer<PDOMap> map;
    Message write(uint64_t data) const {
        return {.data=data, .id = COB, .opts = {.dlc=8}};
    }
};
/** CANOpen Transmit Process Data Object Type */
struct TPDO {
    uint8_t N;
    enum : uint8_t {SYNC, CHANGE=255} type;
    uint32_t COB; // CAN OBject ID
    uint64_t data;
    Buffer<PDOMap> map;
};

/** CANOpen dispatch class
 *
 * use this as a middle layer between the CAN bus and the Device devices to
 * handle translation between the protocols.
 * call this class' `process` directly after the CAN irqHandler
 *
 * cf. https://www.microcontrol.net/wp-content/uploads/2021/10/td-03011e.pdf
 * cf. https://www.can-cia.org/fileadmin/resources/documents/brochures/co_poster.pdf
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
        TPDO *pdo[8] {};
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
    void registerpdo(TPDO *tpdo, Sink<TPDO> *dev) {
        assert ( pdo.n < 8 );
        pdo.dev[pdo.n] = dev;
        pdo.pdo[pdo.n] = tpdo;
        pdo.n += 1;
    }

    bool handlePDO(Message msg) {
        for (int i = 0; i < pdo.n; ++i) {
            auto tpdo = pdo.pdo[i];
            if (tpdo->COB == msg.id) {
                tpdo->data = msg.data;
                pdo.dev[i]->push(*tpdo);
                return true;
            }
        }
        return false;
    }
    bool handleSDO(Message msg) {
        uint16_t service = msg.id & ~0x7f;
        uint8_t id = msg.id & 0x7f;
        if (id == 0) return false;
        if (service == 0x80 ||  // SYNC
            service == 0x580 || // SDO server-to-client
            service == 0x600 || // SDO client-to-server
            service == 0x700 || // NMT control
            false) {
            if (ids[id]) {
                ids[id]->push(SDO::fromCanMsg(msg));
            }
            return true;
        }
        return false;
    }

    void process() {
        // TODO: what to do to CAN messages that _aren't_ CanOpen
        while (!can.empty()) {
            auto msg = can.pop();
            if (handlePDO(msg)) continue;
            if (handleSDO(msg)) continue;
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
    enum NMT : uint8_t {
        OPERATIONAL = 1,
        PREPARED = 2,
        PREOP = 0x80,
        RESET = 0x81,
        RESET_COMM = 0x82,
    };
    void nmt(NMT command, uint8_t nodeid) {
        can.push({.data = (uint64_t)(nodeid & 0x7f) << 8 | command,
                .id = 0, .opts = {.dlc = 2}});
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
    virtual void callback(SDO rq) { assert(false); };
    /** implement the callback method to handle incoming data */
    virtual void callback(TPDO rq) { assert(false); };
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
        if (pdo.COB == 0) pdo.COB = id + 0x80 + 0x100 * pdo.N;
        disablepdo(pdo);
        w8(0x1800+pdo.N-1, 0x2, pdo.type);
        /* w16(0x1800+pdo.N-1, 0x3, pdo.inhibit_time); */
        /* w16(0x1800+pdo.N-1, 0x5, pdo.event_timer); */
        w8(0x1a00+pdo.N-1, 0x0, 0); // clear number of mapped objects
        for (size_t i = 0; i < pdo.map.len; ++i) {
            auto &d = pdo.map[i];
            w32(0x1a00+pdo.N-1, i+1, d.ix << 16 | d.sub << 8 | d.len);
        }
        w8(0x1a00+pdo.N-1, 0x0, pdo.map.len); // set number of mapped objects
        w32(0x1800+pdo.N-1, 0x1, 1<<30 | pdo.COB); // enable PDO
        out.registerpdo(&pdo, this);
    };
    void disablepdo(TPDO &pdo) {
        w32(0x1800+pdo.N-1, 0x1, 1<<31); // clear
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
