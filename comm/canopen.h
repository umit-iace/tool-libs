/** @file canopen.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include "can.h"
#include <utils/deadline.h>
#include <core/kern.h>
#include <core/critical.h>


namespace CAN {
/** cf. https://www.can-cia.org/fileadmin/resources/documents/brochures/co_poster.pdf */
namespace Open {
/** CANOpen Network ManagemenT Commands */
enum class NMT : uint8_t {
    START = 1,          ///< enter OPERATIONAL  0x5
    STOP = 2,           ///< enter STOPPED      0x4
    PREOP = 0x80,       ///< enter PREOP        0x7f
    RESET_APP = 0x81,   ///< ... -> PREOP
    RESET_COMM = 0x82,  ///< ... -> PREOP
};
/** CANOpen Device states */
enum class STATE : uint8_t {
    STOPPED = 4,        ///< STOPPED
    OPERATIONAL = 5,    ///< OPERATIONAL
    PREOP = 0x7f,       ///< PRE-OPERATIONAL
};
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

    Message toMessage() {
        return {
            .data = cmd | (uint64_t)ix << 8 | (uint64_t)sub << 24 | (uint64_t)data << 32,
            .id = (uint32_t)0x600 + nodeID,
            .opts = { .rtr = 0, .ide = 0, .dlc = 8},
        };
    }
    static SDO fromMessage(Message msg) {
        return {
            .data = (uint32_t)(msg.data >> 32),
            .ix = (uint16_t)(msg.data >> 8),
            .sub = (uint8_t)(msg.data >> 24),
            .cmd = (uint8_t)msg.data,
            .nodeID = (uint8_t)(msg.id % 0x80),
        };
    }
};
/** Single Entry in PDO Data List */
struct PDOMap {
    uint16_t ix;    ///< index of Data Object
    uint8_t sub;    ///< subindex of Data Object
    uint8_t len;    ///< length of object [bits] {8, 16, 32}
    int32_t data;   ///< received data // data to send
};
/** Type of PDO transmission */
enum PDOType : uint8_t {
    SYNC,       ///< synchronous == on every NMT SYNC message
    CYCLIC=254, ///< cyclic == after inhibit_time
    CHANGE=255, ///< change == ASAP on change of parameters
};
/** CANOpen Receive Process Data Object Type */
struct RPDO {
    uint8_t N;              ///< Index of PDO [1 .. x]
    PDOType type;           ///< reception type
    uint32_t COB {};        ///< CAN OBject ID -- leave empty for defaults
    Buffer<PDOMap> map;     ///< PDO Data Map
    Message toMessage() const {
        uint64_t data = 0;
        uint8_t shift = 0;
        for (auto &i : map) {
            if (shift + i.len > 64) break;
            data |= ((uint64_t)(uint32_t)i.data & mask(i.len)) << shift;
            shift += i.len;
        }
        return {.data=data, .id = COB, .opts = {.dlc=(uint8_t)(shift / 8)}};
    }
    static uint64_t mask(uint8_t bits) {
        return bits >= 64 ? ~0ull : (1ull << bits) - 1;
    }
};
/** CANOpen Transmit Process Data Object Type */
struct TPDO {
    uint8_t N;              ///< Index of PDO [1 .. x]
    PDOType type;           ///< transmission type
    uint32_t COB {};        ///< CAN OBject ID -- leave empty for defaults
    uint32_t inhibit_time;  ///< inhibit time in 0.1ms steps
    Buffer<PDOMap> map;     ///< PDO Data Map
    void receive(uint64_t d) {
        uint8_t shift = 0;
        for (auto &i : map) {
            if (shift + i.len > 64) break;
            i.data = d >> shift & RPDO::mask(i.len);
            shift += i.len;
        }
    }
};

/** CANOpen dispatch class
 *
 * use this as a middle layer between the CAN bus and the Device devices to
 * handle translation between the protocols.
 * call this class' `process` directly after the CAN irqHandler
 *
 * cf. https://www.microcontrol.net/wp-content/uploads/2021/10/td-03011e.pdf
 *
 * cf. https://www.can-cia.org/fileadmin/resources/documents/brochures/co_poster.pdf
 */
struct Dispatch : Sink<SDO>, Sink<Message> {
    enum LOGLEVEL { NONE, WARN, INFO} log;
    Dispatch(CAN &can, LOGLEVEL log=NONE) : log(log), can(can) { }
    CAN &can;
    Sink<SDO> *ids[128] {};
    struct PDO {
        Sink<TPDO> *dev[8] {};
        TPDO *pdo[8] {};
        uint8_t n {};
    } pdo;

    /** send NMT command to nodeid, rsp broadcast it (id = 0) */
    void nmt(NMT command, uint8_t nodeid) {
        can.trypush({.data = (uint64_t)(nodeid & 0x7f) << 8 | (uint8_t)command,
                .id = 0, .opts = {.dlc = 2}});
    }
    /** send SYNC message on network */
    void sync() {
        can.trypush({.id = 0x80, .opts = {.dlc = 0}});
    }
    /** send HEARTBEAT message to node id, signal own state */
    void heartbeat(uint8_t id, STATE state) {
        can.trypush({.data = (uint8_t)state, .id = (uint32_t)0x700 + id, .opts = {.dlc = 1}});
    }
    /** send GUARD message to node id */
    void guard(uint8_t id) {
        can.trypush({.id = (uint32_t)0x700 + id, .opts = {.rtr = true}});
    }
    /** call directly after CAN interrupt handler processed */
    void process() {
        // TODO: what to do to CAN messages that _aren't_ CanOpen
        while (!can.empty()) {
            auto msg = can.pop();
            if (handlePDO(msg)) continue;
            if (handleSDO(msg)) continue;
            // don't know what to do with received message!
            if (log > NONE) k.log.warn("unhandled: [id: %" PRIx32 "] [%02x %02x %02x %02x %02x %02x %02x %02x]\n",
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
        }
    }

    bool full() override {
        return can.full();
    }
    using Sink<SDO>::push;
    void push(SDO &&rq) override {
        //assert(ids[rq.nodeID] != nullptr); //must register first!
        if (ids[rq.nodeID] == nullptr) {
            if (log > NONE) k.log.warn("ID [0x%x] NOT registered!\n", (unsigned)rq.nodeID);
            return;
        }
        can.push(rq.toMessage());
    }
    using Sink<Message>::push;
    void push(Message &&rq) override { can.push(rq); }

    void registerSDO(uint16_t nodeID, Sink<SDO> *dev) {
        assert(nodeID != 0 && nodeID < 128);
        assert(ids[nodeID] == nullptr);
        if (nodeID == 0 || nodeID >= 128) return;
        ids[nodeID] = dev;
    }
    /** forget device, e.g. before it dies */
    void unregisterSDO(uint16_t nodeID) {
        CriticalSection cs; // process() may run from the CAN interrupt
        if (nodeID < 128) ids[nodeID] = nullptr;
    }
    void registerPDO(TPDO *tpdo, Sink<TPDO> *dev) {
        for (uint8_t i = 0; i < pdo.n; i++) {
            if (pdo.pdo[i] == tpdo) {
                assert(pdo.dev[i] == dev);
                return;
            }
        }
        assert ( pdo.n < 8 );
        if (pdo.n >= 8) return;
        CriticalSection cs; // process() may run from the CAN interrupt
        pdo.dev[pdo.n] = dev;
        pdo.pdo[pdo.n] = tpdo;
        pdo.n += 1;
    }
    /** forget all PDOs of device, e.g. before it dies */
    void unregisterPDOs(Sink<TPDO> *dev) {
        CriticalSection cs;
        uint8_t n = 0;
        for (uint8_t i = 0; i < pdo.n; ++i) {
            if (pdo.dev[i] == dev) continue;
            pdo.dev[n] = pdo.dev[i];
            pdo.pdo[n] = pdo.pdo[i];
            ++n;
        }
        pdo.n = n;
    }

    bool handlePDO(Message msg) {
        for (int i = 0; i < pdo.n; ++i) {
            auto tpdo = pdo.pdo[i];
            if (tpdo->COB == msg.id
                    && msg.opts.dlc) {
                tpdo->receive(msg.data);
                pdo.dev[i]->push(*tpdo);
                if (log > WARN) {
                    k.log.info("handled PDO id: %" PRIx32 "\n", msg.id);
                    k.log.info("          data: %" PRIx64 "\n", msg.data);
                    for (size_t m = 0; m < tpdo->map.len; ++m) {
                        k.log.info("    TPDO data%u: %" PRIx32 "\n",
                                (unsigned)m, (uint32_t)tpdo->map[m].data);
                    }
                }
                return true;
            }
        }
        return false;
    }
    bool handleSDO(Message msg) {
        uint16_t service = msg.id & ~0x7f;
        uint8_t id = msg.id & 0x7f;
        if (id == 0) return false;
        if (//service == 0x80 ||  // SYNC
            service == 0x580 || // SDO server-to-client
            //service == 0x600 || // SDO client-to-server
            //service == 0x700 || // NMT control
            false) {
            if (ids[id]) {
                ids[id]->push(SDO::fromMessage(msg));
            }
            if (log > WARN) k.log.info("handled SDO id: %" PRIx32 "\n", msg.id);
            return true;
        }
        return false;
    }
};

/** CANOpen device wrapper class
 *
 * inherit and implement `callback` to use this interface on a CANOpen network
 */
struct Device : Sink<TPDO>, Sink<SDO> {
    /** Construct new CANOpen Device on network with given ID */
    /** registers with the kernel, which works before the kernel itself is
     * constructed. Dispatch has to be constructed first though, e.g. define
     * both in the same file with the Dispatch first
     */
    Device(Dispatch &canopen, uint8_t id) : id(id), out(canopen)
    {
        assert(id > 0 && id < 128);
        canopen.registerSDO(id, this);
        // keeps the SDO queue going if a response gets lost
        ticker = k.every(1, *this, &Device::tick);
    }
    virtual ~Device() {
        k.remove(ticker);
        out.unregisterSDO(id);
        out.unregisterPDOs(this);
    }
    Device(const Device &) = delete;
    Device& operator=(const Device &) = delete;
    uint8_t const id {};    ///< CANOpen node ID [1 - 127]
    Dispatch &out;

    struct State {
        Queue<SDO> q{60};
        Deadline next{};
    } state;
    /** implement the callback method to handle incoming data */
    virtual void callback(SDO rq) { assert(false); };
    /** implement the callback method to handle incoming data */
    virtual void callback(TPDO rq) { assert(false); };
    /** read DO with given index and subindex of this device */
    void read(uint16_t ix, uint8_t sub) {
        pushorqueue({.ix=ix, .sub=sub, .cmd=0x40, .nodeID=id});
    }
    /** write given value to DO at given index and subindex */
    void w8(uint16_t ix, uint8_t sub, uint8_t val) {
        pushorqueue({.data=(uint32_t)val, .ix=ix, .sub=sub, .cmd=0x2f, .nodeID=id});
    }
    /** write given value to DO at given index and subindex */
    void w16(uint16_t ix, uint8_t sub, uint16_t val) {
        pushorqueue({.data=(uint32_t)val, .ix=ix, .sub=sub, .cmd=0x2b, .nodeID=id});
    }
    /** write given value to DO at given index and subindex */
    void w32(uint16_t ix, uint8_t sub, uint32_t val) {
        pushorqueue({.data=(uint32_t)val, .ix=ix, .sub=sub, .cmd=0x23, .nodeID=id});
    }
    /** send PDO */
    void wPDO(const RPDO &rpdo) {
        (*(Sink<Message>*)&out).trypush(rpdo.toMessage());
    }
    /** enable receiving PDO on device */
    void enablePDO(RPDO &pdo) {
        if (pdo.COB == 0) pdo.COB = id + 0x100 * (pdo.N+1);
        disablePDO(pdo);
        w8(0x1400+pdo.N-1, 0x2, pdo.type); // set transmission type
        w8(0x1600+pdo.N-1, 0x0, 0); // clear number of mapped objects
        for (size_t i = 0; i < pdo.map.len; ++i) {
            auto &d = pdo.map[i];
            w32(0x1600+pdo.N-1, i+1, (uint32_t)d.ix << 16 | (uint32_t)d.sub << 8 | d.len);
        }
        w8(0x1600+pdo.N-1, 0x0, pdo.map.len); // set number of mapped objects
        w32(0x1400+pdo.N-1, 0x1, 1u<<30 | pdo.COB);
    }
    /** disable receiving PDO on device */
    void disablePDO(RPDO &pdo) {
        w32(0x1400+pdo.N-1, 0x1, 1u<<31); // clear
    }
    /** enable sending PDO on device */
    void enablePDO(TPDO &pdo) {
        if (pdo.COB == 0) pdo.COB = id + 0x80 + 0x100 * pdo.N;
        disablePDO(pdo);
        w8(0x1800+pdo.N-1, 0x2, pdo.type);
        w16(0x1800+pdo.N-1, 0x3, pdo.inhibit_time);
        /* w16(0x1800+pdo.N-1, 0x5, pdo.event_timer); */
        w8(0x1a00+pdo.N-1, 0x0, 0); // clear number of mapped objects
        for (size_t i = 0; i < pdo.map.len; ++i) {
            auto &d = pdo.map[i];
            w32(0x1a00+pdo.N-1, i+1, (uint32_t)d.ix << 16 | (uint32_t)d.sub << 8 | d.len);
        }
        w8(0x1a00+pdo.N-1, 0x0, pdo.map.len); // set number of mapped objects
        w32(0x1800+pdo.N-1, 0x1, 1u<<30 | pdo.COB); // enable PDO
        out.registerPDO(&pdo, this);
    };
    /** disable sending PDO on device */
    void disablePDO(TPDO &pdo) {
        w32(0x1800+pdo.N-1, 0x1, 1u<<31); // clear
    };
    bool full() override {
        return (*(Sink<Message>*)&out).full();
    }
    using Sink<SDO>::push;
    void push(SDO &&sdo) override {
        state.next = {};
        callback(std::move(sdo));
        process();
    }
    using Sink<TPDO>::push;
    void push(TPDO &&pdo) override {
        callback(std::move(pdo));
        process();
    }
    /** send the next queued SDO once the last one got its response or
     * timed out
     *
     * runs from the main loop and from Dispatch::process, which may be
     * called in the CAN interrupt
     */
    void process() {
        SDO next;
        {
            // only take it out under the lock, sending may log (and wait)
            CriticalSection cs;
            if (state.q.empty()) return;
            if (state.next.when && !state.next(k.time)) return;
            if (full()) return; // try again later
            next = state.q.pop();
            state.next = Deadline::after(k.time, 2);
        }
        out.push(std::move(next));
    }
    void pushorqueue(SDO &&sdo) {
        // always go through the queue to keep requests in order
        {
            CriticalSection cs;
            state.q.trypush(std::move(sdo));
        }
        process();
    }
private:
    Schedule::Schedulable *ticker{};
    void tick(uint32_t, uint32_t) { process(); }
};
}
}
