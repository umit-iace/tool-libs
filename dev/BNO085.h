/** @file BNO085.h
 *
 * Copyright (c) 2025 IACE
 */
#pragma once

#include "sys/i2c.h"
#include "utils/slice.h"
#include <cmath>
#include <core/logger.h>


/**
 * Implementation of the BNO085 9-axis motion processor
 *
 * for details, see
 * - https://www.ceva-ip.com/wp-content/uploads/BNO080_085-Datasheet.pdf
 * - https://cdn-learn.adafruit.com/downloads/pdf/adafruit-9-dof-orientation-imu-fusion-breakout-bno085.pdf
 * - https://github.com/sparkfun/Qwiic_IMU_BNO080/blob/master/Documents/SH-2-Reference-Manual-v1.2.pdf
 *
 * TODO: calibration?
 */

struct BNO085 : I2C::Device {
    /** possible I2C addresses */
    enum ADDR {
        DEFAULT = 0b1001010, //0x4a
        ALT = 0b1001011, //0x4b
    };

    /** Enum for defining wanted Units of angles */
    enum Unit : uint8_t {RADIANS, DEGREES, NONE};
    static constexpr double rad2deg = 180. / M_PI;

    /** Euler angles */
    struct Euler {
        double yaw, pitch, roll;
        Unit unit;
        Euler toUnit(Unit u) {
            if (u == unit) return *this;
            switch (u) {
            case RADIANS: return {
                              .yaw = yaw / rad2deg,
                              .pitch = pitch / rad2deg,
                              .roll = roll / rad2deg,
                              .unit = u,
                          };
            case DEGREES: return {
                              .yaw = yaw * rad2deg,
                              .pitch = pitch * rad2deg,
                              .roll = roll * rad2deg,
                              .unit = u,
                          };
            default: return {.yaw=0,.pitch=0,.roll=0,.unit=NONE};
            }
        }
    };

    /** Quaternion of pose */
    struct Quaternion {
        double r,i,j,k;
        /** return pose in euler angles, in given Unit */
        Euler toEuler(Unit u=RADIANS) const {
            double sqr = r*r;
            double sqi = i*i;
            double sqj = j*j;
            double sqk = k*k;
            double factor = u==RADIANS ? 1. : rad2deg;

            return {
                .yaw = factor * atan2(2.0 * (i * j + k * r), (sqi - sqj - sqk + sqr)),
                .pitch = factor * asin(-2.0 * (i * k - j * r) / (sqi + sqj + sqk + sqr)),
                .roll = factor * atan2(2.0 * (j * k + i * r), (-sqi - sqj + sqk + sqr)),
                .unit = u,
            };
        };
    };

    /** Rotation about axes, in given Unit/s */
    struct Rotation {
        double x,y,z;
        Unit unit;
        Rotation toUnit(Unit u) {
            if (u == unit) return *this;
            switch (u) {
            case RADIANS: return {
                              .x = x / rad2deg,
                              .y = y / rad2deg,
                              .z = z / rad2deg,
                              .unit = u,
                          };
            case DEGREES: return {
                              .x = x * rad2deg,
                              .y = y * rad2deg,
                              .z = z * rad2deg,
                              .unit = u,
                          };
            default: return {.x=0,.y=0,.z=0,.unit=NONE};
            }
        }
    };

    /** high-speed Gyro-Integrated Rotation Vector
     * includes approximations of rotation velocities
     */
    struct GIRV {
        Quaternion pose;
        Rotation rot;
    };

    BNO085(Sink<I2C::Request> &bus, uint8_t addr=DEFAULT) : I2C::Device{bus, addr} {}
    using Data = Buffer<uint8_t>;
    using View = Slice<uint8_t>;
    int8_t read8(View d) { return d[0]; }
    int16_t read16(View d) { return d[0] | d[1] << 8; }
    int32_t read32(View d) { return d[0] | d[1] << 8 | d[2] << 16 | d[3] << 24; }

    constexpr double Q(uint8_t p) {
        return 1. / (1 << p);
    }

    enum Channel : uint8_t {
        C_SHTP, C_DEV, C_CTRL, C_NORM, C_WAKE, C_GIRV, C_LAST,
    };

    uint8_t seq[C_LAST] {};

    /** SHTP Message */
    struct Message {
        Channel chan;
        Data cargo;
    };

    struct Transaction {
        struct Operation {
            Message msg;
            uint8_t rsp; // ID of response message
            int16_t seq = -1; // used as 'in-flight' flag + matching
        };
        Queue<Operation> q;
        bool active = false;
    };

    /** simple fire & forget message sending
     * returns the sequence of sent message
     */
    uint8_t sendMessage(const Message &msg) {
        Data data = msg.cargo.len + 4;
        uint8_t sq = seq[msg.chan]++;
        // build header
        data.append({
            (uint8_t) data.size, (uint8_t)(data.size >> 8),
            msg.chan, sq,
        });
        for (auto &d: msg.cargo) data.append(d);
        bus.trypush({
            .dev = this,
            .data = data,
        });
        return sq-1;
    }

    void sendTransaction(Queue<Transaction> &q, Transaction &&t) {
        if (q.full()) return; // drop transaction. something went wrong
        q.push(std::move(t));
        if (q.front().active == false) sendQueued(q);
    }

    void sendQueued(Queue<Transaction> &q) {
        if (q.empty()) return;
        auto &t = q.front();
        if (t.active) return; // still in flight
        t.active = true;
        if (t.q.empty()) return; //XXX: or just pop outer & sendQueued(q)
        auto &oper = t.q.front();
        if (oper.seq != -1) return; // still in flight. shouldn't get here.
        oper.seq = sendMessage(oper.msg);
    }

    enum Commands : uint8_t { CMD_GET_ADV, CMD_GET_ERR };

    void getAdvertisement(bool all) {
        sendMessage({
            .chan = C_SHTP,
            .cargo = {CMD_GET_ADV, all},
        });
    }
    void getErrors() {
        sendMessage({
            .chan = C_SHTP,
            .cargo = {CMD_GET_ERR},
        });
    }

    enum AdvTag : uint8_t {
        RESERVED, GUID, MAX_CPH_WRITE, MAX_CPH_READ, MAX_XFER_WRITE, MAX_XFER_READ,
        NORMAL_CH, WAKE_CH, APP_NAME, CH_NAME
    };

    void logAdvertisement(View data) {
        lg->info("advertisement\n");
        uint8_t last_guid {};
        while (data.len) {
            assert (data.len > 2);
            AdvTag tag {data[0]};
            size_t len {data[1]};
            int val;
            switch (len) {
                case 1: val = read8({data,2}); break;
                case 2: val = read16({data,2}); break;
                case 4: val = read32({data,2}); break;
            }
            switch ((uint8_t)tag) {
            case GUID: last_guid = val;
                    lg->print("GUID: %d\n", val); break;
            case MAX_CPH_WRITE:
                    lg->print("MaxCargoPlusHeaderWrite: %d\n", val); break;
            case MAX_CPH_READ:
                    lg->print("MaxCargoPlusHeaderRead: %d\n", val); break;
            case MAX_XFER_WRITE:
                    lg->print("MaxTransferWrite: %d\n", val); break;
            case MAX_XFER_READ:
                    lg->print("MaxTransferRead: %d\n", val); break;
            case NORMAL_CH:
                    lg->print("NormalChannel: %d\n", val); break;
            case WAKE_CH:
                    lg->print("WakeChannel: %d\n", val); break;
            case APP_NAME:
                    lg->print("AppName: %.*s\n", len, &data[2]); break;
            case CH_NAME:
                    lg->print("ChannelName: %.*s\n", len, &data[2]); break;
            case 0x80:
                    if (last_guid == 0) {
                        lg->print("SHTP Version: %.*s\n", len, &data[2]); break;
                    } else {
                        lg->print("Version: %.*s\n", len, &data[2]); break;
                    }
            case 0x81:
                    if (last_guid == 0) {
                        lg->print("SHTP Timeout: %dms\n", val); break;
                    } else {
                        lg->print("Report Lengths:\n");
                        auto rpl = Slice{data,2};
                        for ( size_t i = 0; i < rpl.len; i+=2) {
                            lg->print("\t[ 0x%.2x : %d ]\n", rpl[i], rpl[i+1]);
                        }
                        break;
                    }
            default:
                    lg->print("Wrong Tag [%d], len [%d]\n", tag, len); break;
            }
            data = Slice{data, len+2};
        }
    }

    enum ERR : uint8_t {
        NO_ERR, MAX_READ_EXCEED, HOST_WRITE_SHORT, MAX_WRITE_EXCEED, HOST_WRITE_INVALID,
        FRAGMENT_UNSUPPORTED_START, FRAGMENT_UNSUPPORTED_CONT, CMD_NOT_RECOGNIZED, PARAM_NOT_RECOGNIZED,
        CHANNEL_NOT_RECOGNIZED, REQ_WHILE_PENDING, WRITE_WHILE_RESP, ERR_TRUNC_LIST,
    };

    void logErrorList(View data) {
        lg->info("error list\n");
        for (auto &d: data) {
            ERR err {d};
            switch (err) {
            case NO_ERR: lg->print("\tNO_ERR\n", err); break;
            case MAX_READ_EXCEED: lg->print("\tMAX_READ_EXCEED\n", err); break;
            case HOST_WRITE_SHORT: lg->print("\tHOST_WRITE_SHORT\n", err); break;
            case MAX_WRITE_EXCEED: lg->print("\tMAX_WRITE_EXCEED\n", err); break;
            case HOST_WRITE_INVALID: lg->print("\tHOST_WRITE_INVALID\n", err); break;
            case FRAGMENT_UNSUPPORTED_START: lg->print("\tFRAGMENT_UNSUPPORTED_START\n", err); break;
            case FRAGMENT_UNSUPPORTED_CONT: lg->print("\tFRAGMENT_UNSUPPORTED_CONT\n", err); break;
            case CMD_NOT_RECOGNIZED: lg->print("\tCMD_NOT_RECOGNIZED\n", err); break;
            case PARAM_NOT_RECOGNIZED: lg->print("\tPARAM_NOT_RECOGNIZED\n", err); break;
            case CHANNEL_NOT_RECOGNIZED: lg->print("\tCHANNEL_NOT_RECOGNIZED\n", err); break;
            case REQ_WHILE_PENDING: lg->print("\tREQ_WHILE_PENDING\n", err); break;
            case WRITE_WHILE_RESP: lg->print("\tWRITE_WHILE_RESP\n", err); break;
            case ERR_TRUNC_LIST: lg->print("\tERR_TRUNC_LIST\n", err); break;
            }
        }
    }

    void handleSHTP(View cargo) {
        if (lg) lg->print("Response Handler: ");
        switch (cargo[0]) {
        case CMD_GET_ADV: if(lg) logAdvertisement({cargo, 1}); break;
        case CMD_GET_ERR: if(lg) logErrorList({cargo, 1}); break;
        default: if(lg) lg->warn("unknown SHTP response: [%d]\n", cargo[0]);
        }
    }

    void handleWakeNormal(View cargo) {
        while (cargo.len) switch (cargo[0]) {
        case 0x05: cargo = handleRotVecReport(cargo); break;
        case 0xFA: cargo = handleRebaseTimeStamp(cargo); break;
        case 0xFB: cargo = handleBaseTimeStamp(cargo); break;
        case 0xFC: cargo = handleFeatureResponse(cargo); break;
        default:
               if (lg) lg->warn("unrecognized Report: [0x%.2x]\n", cargo[0]);
               if (lg) logData({*cargo.buf,0});
               return;
        }
    }

    View handleRotVecReport(View cargo) {
        assert(cargo[0] == 0x05); // report ID
        this->rotvec = {
            .r = read16({cargo, 10}) * Q(14),
            .i = read16({cargo, 4}) * Q(14),
            .j = read16({cargo, 6}) * Q(14),
            .k = read16({cargo, 8}) * Q(14),
        };
        this->rotvec_accuracy = read16({cargo, 12}) * Q(12);
        return {cargo, 14};
    }

    void handleGIRV(View cargo) {
        // note: does not contain report ID
        this->girv.pose = {
            .r = read16({cargo, 6}) * Q(14),
            .i = read16({cargo, 0}) * Q(14),
            .j = read16({cargo, 2}) * Q(14),
            .k = read16({cargo, 4}) * Q(14),
        };
        this->girv.rot = {
            .x = read16({cargo, 8}) * Q(10),
            .y = read16({cargo, 10}) * Q(10),
            .z = read16({cargo, 12}) * Q(10),
            .unit = RADIANS,
        };
    }

    View handleBaseTimeStamp(View cargo) {
        assert(cargo[0] == 0xFB); // report ID
        int32_t tb_delta = read32({cargo, 1});
        (void) tb_delta;
        return {cargo, 5};
    }

    View handleRebaseTimeStamp(View cargo) {
        assert(cargo[0] == 0xFA); // report ID
        int32_t rb_delta = read32({cargo, 1});
        (void) rb_delta;
        return {cargo, 5};
    }

    void logData(View cargo) {
        for (uint8_t c: cargo) {
            lg->print("%#x ", c);
        }
        lg->print("\n");
    }

    struct Feature {
        uint32_t report_interval, batch_interval, config_word;
        uint16_t sensitivity;
        uint8_t flags;
        uint8_t reportID;
    };

    void setFeature(Feature feat) {
        sendMessage({
            .chan = C_CTRL,
            .cargo = {
                0xFD, feat.reportID, feat.flags,
                (uint8_t)feat.sensitivity, (uint8_t)(feat.sensitivity>>8),
                (uint8_t)(feat.report_interval >> 0),
                (uint8_t)(feat.report_interval >> 8),
                (uint8_t)(feat.report_interval >> 16),
                (uint8_t)(feat.report_interval >> 24),
                (uint8_t)(feat.batch_interval >> 0),
                (uint8_t)(feat.batch_interval >> 8),
                (uint8_t)(feat.batch_interval >> 16),
                (uint8_t)(feat.batch_interval >> 24),
                (uint8_t)(feat.config_word >> 0),
                (uint8_t)(feat.config_word >> 8),
                (uint8_t)(feat.config_word >> 16),
                (uint8_t)(feat.config_word >> 24),
            },
        });
    }

    View handleFeatureResponse(View cargo) {
        (void)cargo;
        if (lg) lg->print("handling Feature Response.. TODO\n");
        return {cargo, 17};
    }

    struct {
        Queue<Transaction> write, read;
    } qs;

    void handleControl(View cargo) {
        switch (cargo[0]) {
        case 0xF5: handleFRSWriteResponse(cargo); return;
        case 0xF3: handleFRSReadResponse(cargo); return;
        default: if (lg) {
                     lg->warn("unhandled message on control channel: [%#x]\n", cargo[0]);
                     logData({*cargo.buf, 0});
                 }
        }
    }

    void handleFRSWriteResponse(View d) {
        assert(d[0] == 0xF5);
        if (qs.write.empty()) return; //something went wrong
        auto &t = qs.write.front();
        if (t.q.empty()) return; //something went wrong
        auto &op = t.q.front();
        if (op.rsp != 0xF5 && lg) lg->warn("response 0xF5, expected [%#x]\n", op.rsp);
        if (lg) logFRSWriteResponse(d);
        uint8_t status = d[1];
        switch (status) {
        // operation finished
        case 0: if (!t.q.empty()) {
                    t.q.drop();
                    sendQueued(qs.write);
                }
                break;
        // progress
        case 8: break;
        // error or transaction done
        default: if (!qs.write.empty()) {
                    qs.write.drop();
                    sendQueued(qs.write);
                }
                break;
        }
    }

    void logFRSWriteResponse(View d) {
        uint8_t status = d[1];
        uint16_t word = read16({d, 2});
        lg->info("FRS Write Response: offset [%d]\n", word);
        switch (status) {
        case 0: lg->print("Word(s) received\n"); break;
        case 1: lg->print("unrecognized FRS type\n"); break;
        case 2: lg->print("busy\n"); break;
        case 3: lg->print("write completed\n"); break;
        case 4: lg->print("write mode ready\n"); break;
        case 5: lg->print("write failed\n"); break;
        case 6: lg->print("data received while not in write mode\n"); break;
        case 7: lg->print("invalid length\n"); break;
        case 8: lg->print("record valid\n"); break;
        case 9: lg->print("record invalid\n"); break;
        case 10: lg->print("device error\n"); break;
        case 11: lg->print("record is read only\n"); break;
        case 12: lg->print("FRS memory is full\n"); break;
        }
    }

    void handleFRSReadResponse(View d) {
        assert(d[0] == 0xF3);
        if (lg) logFRSReadResponse(d);
        uint8_t status = d[1] & 0xf;
        switch (status) {
        case 0: // progress
            break;
        default: // complete or error
            qs.read.pop(); sendQueued(qs.read); break;
        }
    }

    void logFRSReadResponse(View d) {
        lg->info("FRS Read Response:\n");
        uint8_t len = d[1] >> 4;
        uint8_t status = d[1] & 0xf;
        uint16_t offset = read16({d, 2});
        uint32_t w0 = read32({d, 4});
        uint32_t w1 = read32({d, 8});
        switch (status) {
        case 0: lg->print("no error\n"); break;
        case 1: lg->warn("unrecognized FRS type [0x%.4x]\n", read16({d,12})); break;
        case 2: lg->print("busy\n"); break;
        case 3: lg->print("read record completed\n"); break;
        case 5: lg->print("record empty\n"); break;
        case 8: lg->print("device error\n"); break;
        }
        lg->print("offset: %d\n", offset);
        lg->print("length of data: %d\n", len);
        lg->print("data0: %#x\n", w0);
        lg->print("data1: %#x\n", w1);
        lg->print("FRS Type: %#x\n", (uint16_t)read16({d,12}));
    }

    void readFRS(uint16_t type) {
        Transaction t {
            .q = Buffer<Transaction::Operation>{{
                .msg = {
                    .chan = C_CTRL,
                    .cargo = {
                        // FRS Read request
                        0xF4,
                        0, 0, 0, //reserved
                        (uint8_t)type, (uint8_t)(type>>8),
                        0, 0,
                    },
                },
                .rsp = 0xF3,
            }},
            .active = false,
        };
        sendTransaction(qs.read, std::move(t));
    }

    struct GIRVConfig {
        enum Reference : uint16_t {
            GameRotVec_6Ax = 0x0207,
            AbsRotVec_9Ax = 0x0204,
        } ref = GameRotVec_6Ax;
        uint32_t sync = 10000; // ref sync in microseconds
        uint32_t max_err = 0x10c15238; // pi/6 @ Qpoint of 29
        uint32_t prediction = 0; // in seconds, Qpoint of 10
        uint32_t alpha = 0; // 0.303072543909142
        uint32_t beta = 0; // 0.113295896384921
        uint32_t gamma = 0; // 0.002776219713054
    };

    void configure(GIRVConfig cfg) {
        // configuration recordID = 0xA1A2
        // FRS recordID = 0xE324
        Transaction t {
        .q = Buffer<Transaction::Operation>{{
            .msg = {
                .chan = C_CTRL,
                .cargo = {
                    0xF7, // FRS write request
                    0, // reserved
                    7, 0, //length of record in WORDS
                    0xa2, 0xa1, //configuration record id
                },
            },
            .rsp = 0xF5,
            }, {
            .msg = {
                .chan = C_CTRL,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    0, 0, //offset
                    (uint8_t)cfg.ref, (uint8_t)(cfg.ref >> 8),
                    0, 0,
                    (uint8_t)cfg.sync, (uint8_t)(cfg.sync >> 8),
                    (uint8_t)(cfg.sync >> 16), (uint8_t)(cfg.sync >> 24),
                },
            },
            .rsp = 0xF5,
            }, {
            .msg = {
                .chan = C_CTRL,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    2, 0, //offset
                    (uint8_t)cfg.max_err, (uint8_t)(cfg.max_err >> 8),
                    (uint8_t)(cfg.max_err >> 16), (uint8_t)(cfg.max_err >> 24),
                    (uint8_t)cfg.prediction, (uint8_t)(cfg.prediction >> 8),
                    (uint8_t)(cfg.prediction >> 16), (uint8_t)(cfg.prediction >> 24),
                },
            },
            .rsp = 0xF5,
            }, {
            .msg = {
                .chan = C_CTRL,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    4, 0, //offset
                    (uint8_t)cfg.alpha, (uint8_t)(cfg.alpha >> 8),
                    (uint8_t)(cfg.alpha >> 16), (uint8_t)(cfg.alpha >> 24),
                    (uint8_t)cfg.beta, (uint8_t)(cfg.beta >> 8),
                    (uint8_t)(cfg.beta >> 16), (uint8_t)(cfg.beta >> 24),
                },
            },
            .rsp = 0xF5,
            }, {
            .msg = {
                .chan = C_CTRL,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    6, 0, //offset
                    (uint8_t)cfg.gamma, (uint8_t)(cfg.gamma >> 8),
                    (uint8_t)(cfg.gamma >> 16), (uint8_t)(cfg.gamma >> 24),
                    0,0,0,0,
                },
            },
            .rsp = 0xF5,
        }}
        };
        sendTransaction(qs.write, std::move(t));
    }

    void callback(const I2C::Request &rq) override {
        if (!rq.opts.read) {
            //delete
            /* if (rq.data.len >4) { */
            /*     lg->info("sent: "); */
            /*     logData({rq.data,0}); */
            /* } */
            //delete
            return; // do nothing on successful writes
        }

        if (rq.data.len >= 4) {
            /* bool continuation = rq.data[1] & 0x80; */
            const unsigned int len = (uint16_t)(rq.data[1] & 0x7f) << 8 | rq.data[0];
            if (len == 0xffff) return; // 'length of 65535 is an error.'
            const Channel chan = (Channel)rq.data[2];
            const uint8_t seq = rq.data[3];
            if (len == 0) return; // length of 0 indicates 'no data'
            if (seq != this->seq[chan]) {
                //error?
                if (lg) lg->warn("expected seq: %d, got %d\n", this->seq[chan], seq);
            }
            this->seq[chan] = seq + 1;
            if (len > rq.data.len) { // need to get more data
                bus.trypush({
                        .dev = this,
                        .data = len,
                        .opts = {
                            .read = true,
                        },
                });
                return;
            }

            View data {rq.data, 4};
            switch (chan) {
            case C_SHTP: handleSHTP(data); return;
            /* case C_DEV: handleDevice(data); return; */
            case C_CTRL: handleControl(data); return;
            case C_NORM ... C_WAKE: handleWakeNormal(data); return;
            case C_GIRV: handleGIRV(data); return;
            default:
                         if (lg) {
                lg->print("unrecognized data:\n");
                logData({rq.data, 0});
                         }
            }

        }
    }

    double rotvec_accuracy;
    /** high-accuracy 9-axis sensor-fused rotation vector */
    Quaternion rotvec;
    /** high-speed Gyro-Integrated Rotation Vector,
     * includes approximations of rotation velocities
     */
    GIRV girv;

    /** call this regularly to make sure we receive messages from the device
     * when it wants to send them
     */
    void poll(uint32_t, uint32_t) {
        bus.trypush({
            .dev = this,
            .data = 4,
            .opts = { .read = true },
        });
    }

    /** enable high-accuracy rotation vector at given frequency */
    void enableRotVec(uint32_t Hz=400) {
        setFeature({
            .report_interval = 1'000'000 / Hz,
            .reportID = 0x05,
        });
    }

    /** disable high-accuracy rotation vector */
    void disableRotVec() {
        setFeature({
            .report_interval = 0,
            .reportID = 0x05,
        });
    }

    /** enable high-speed GIRV at given frequency */
    void enableGIRV(uint32_t Hz=500) {
        configure({
            .ref = GIRVConfig::AbsRotVec_9Ax,
        });
        setFeature({
            .report_interval = 1'000'000 / Hz,
            .reportID = 0x2a,
        });
    }

    /** disable high-speed GIRV */
    void disableGIRV() {
        setFeature({
            .report_interval = 0,
            .reportID = 0x2a,
        });
    }

    /** restart the device */
    void restart() {
        sendMessage({
            .chan = C_DEV,
            .cargo = { 1 },
        });
    }

    /** return approximated accuracy in given Unit */
    double accuracy(Unit u=RADIANS) {
        switch (u) {
        case RADIANS: return rotvec_accuracy;
        case DEGREES: return rotvec_accuracy * rad2deg;
        default: return 0;
        }
    }

    BufferedLogger *lg = nullptr;
};
