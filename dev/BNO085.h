/** @file BNO085.h
 *
 * Copyright (c) 2025 IACE
 */
#pragma once

#include "sys/i2c.h"
#include "utils/slice.h"
#include <cmath>
#include <core/logger.h>
extern BufferedLogger lg;

struct Euler {
    double yaw, pitch, roll;
};

struct Quaternion {
    double r,i,j,k;
    Euler toEuler() const {
        double sqr = r*r;
        double sqi = i*i;
        double sqj = j*j;
        double sqk = k*k;

        return {
            .yaw = atan2(2.0 * (i * j + k * r), (sqi - sqj - sqk + sqr)),
            .pitch = asin(-2.0 * (i * k - j * r) / (sqi + sqj + sqk + sqr)),
            .roll = atan2(2.0 * (j * k + i * r), (-sqi - sqj + sqk + sqr)),
        };
    };
};

struct Axes {
    double x,y,z;
};
struct GIRV {
    Quaternion q;
    Axes a;
};

/**
 * Implementation of the BNO085 9-axis motion processor
 *
 * see
 * https://www.ceva-ip.com/wp-content/uploads/BNO080_085-Datasheet.pdf
 * https://cdn-learn.adafruit.com/downloads/pdf/adafruit-9-dof-orientation-imu-fusion-breakout-bno085.pdf
 * https://github.com/sparkfun/Qwiic_IMU_BNO080/blob/master/Documents/SH-2-Reference-Manual-v1.2.pdf
 * for details
 *
 * TODO: calibration?
 * want: Rotation Vector (2.2.4)
 *
 * note: At system startup, the hub must send its full advertisement message (see 5.2 and 5.3) to the
 * host. It must not send any other data until this step is complete.
 */

struct BNO085 {
    struct SHTP : I2C::Device {
        SHTP(Sink<I2C::Request> &bus, uint8_t addr) : I2C::Device{bus, addr} {}
        using Data = Buffer<uint8_t>;
        using View = Slice<uint8_t>;

        struct Message {
            uint8_t chan;
            Data cargo;
        };
        struct Entry {
            Message msg;
            int seq = -1; // used as 'in-flight' flag + matching in callback
        };
        struct FRS {
            Queue<Entry> write, read;
        } qs;
        void sendQ(Queue<Entry> &queue) {
            if (queue.empty()) return;
            if (queue.front().seq != -1) return;
            auto &entry = queue.front();
            auto chan = entry.msg.chan;
            entry.seq = chans[chan].out++;
            Data data = entry.msg.cargo.len + 4;
            data.append({
                (uint8_t) data.size, (uint8_t)(data.size >> 8),
                chan, (uint8_t)entry.seq,
            });
            for (auto &d : entry.msg.cargo) data.append(d);
            bus.trypush({
                .dev = this,
                .data = data,
            });
        }
        void queueOrPush(Queue<Entry> &queue, const Message &msg) {
            if (queue.full()) return; //drop message. something went wrong
            queue.push({.msg = msg});
            if (queue.front().seq == -1) sendQ(queue);
        }

        struct Seq {
            uint8_t out = 0, in = 0;
        };
        Seq chans[8] {};
        /* Buffer<char> chan_names[8] {}; */
        /* struct App { */
        /*     /1* uint8_t guid; *1/ // ID in array index */
        /*     int chans; // number of channels of application */
        /* }; */
        /* App apps[8] {}; */
        struct Command {
            using Params = Buffer<uint8_t>;
            uint8_t cmd;
            Params params;
        };
        GIRV girv;

        struct FRS3 {
            uint32_t version, range, resolution;
            uint16_t revision, power;
            uint32_t min_period;
            uint16_t FIFO_reserved, FIFO_max;
            uint16_t vID_len, batch;
            uint16_t Qp2, Qp1;
            uint16_t Qp3, metadata_len;
        } __attribute__((packed));
        struct FRS4 {
            uint32_t version, range, resolution;
            uint16_t revision, power;
            uint32_t min_period;
            uint16_t FIFO_reserved, FIFO_max;
            uint16_t vID_len, batch;
            uint16_t Qp2, Qp1;
            uint16_t Qp3, metadata_len;
            uint32_t max_period;
        } __attribute__((packed));

        int8_t read8(View d) { return d[0]; }
        int16_t read16(View d) { return d[0] | d[1] << 8; }
        int32_t read32(View d) { return d[0] | d[1] << 8 | d[2] << 16 | d[3] << 24; }
        constexpr double Q(uint8_t p) {
            return 1. / (1 << p);
        }

        void check() {
            bus.trypush({
                    .dev = this,
                    .data = 4,
                    .opts = { .read = true },
            });
        }
        enum CMD : uint8_t { GET_ADV, SND_ERR, };
        void sendCommand(CMD cmd, Command::Params p) {
            uint16_t len = 4 + 1 + p.len;
            Buffer<uint8_t> data = len;
            data.append({
                    /// HEADER
                        (uint8_t)len, (uint8_t)(len>>8), // length of data
                        0, //channel
                        chans[0].out++, // sequence number
                    /// COMMAND
                        cmd,
            });
            for (auto &param : p) {
                data.append(param);
            }
            bus.trypush({
                    .dev = this,
                    .data = data,
            });
            check();
        }
        void handleSHTP(View cargo) {
            lg.print("Response Handler: ");
            switch (cargo[0]) {
            case GET_ADV: logAdvertisement({cargo, 1}); break;
            case SND_ERR: logErrorList({cargo, 1}); break;
            default: lg.warn("unknown SHTP response: [%d]\n", cargo[0]);
            }
        }
        enum Tag : uint8_t {
            RESERVED, GUID, MAX_CPH_WRITE, MAX_CPH_READ, MAX_XFER_WRITE, MAX_XFER_READ,
            NORMAL_CH, WAKE_CH, APP_NAME, CH_NAME
        };
        struct Adv {
            uint32_t cphWr, cphRd, xferWr, xferRd, nrm, wak;
            uint8_t guid;
            char app[20]{}, chan[20]{};
        };

        void logAdvertisement(View data) {
            lg.info("advertisement\n");
            uint8_t last_guid {};
            while (data.len) {
                assert (data.len > 2);
                Tag tag {data[0]};
                size_t len {data[1]};
                int val;
                switch (len) {
                    /* case 1: val = data[2]; break; */
                    /* case 2: val = data[2] | data[3] << 8; break; */
                    /* case 4: val = data[2] | data[3] << 8 | data[4] << 16 | data[5] << 24; break; */
                    case 1: val = read8({data,2}); break;
                    case 2: val = read16({data,2}); break;
                    case 4: val = read32({data,2}); break;
                }
                switch ((uint8_t)tag) {
                case GUID: last_guid = val;
                        lg.print("GUID: %d\n", val); break;
                case MAX_CPH_WRITE:
                        lg.print("MaxCargoPlusHeaderWrite: %d\n", val); break;
                case MAX_CPH_READ:
                        lg.print("MaxCargoPlusHeaderRead: %d\n", val); break;
                case MAX_XFER_WRITE:
                        lg.print("MaxTransferWrite: %d\n", val); break;
                case MAX_XFER_READ:
                        lg.print("MaxTransferRead: %d\n", val); break;
                case NORMAL_CH:
                        lg.print("NormalChannel: %d\n", val); break;
                case WAKE_CH:
                        lg.print("WakeChannel: %d\n", val); break;
                case APP_NAME:
                        lg.print("AppName: %.*s\n", len, &data[2]); break;
                case CH_NAME:
                        lg.print("ChannelName: %.*s\n", len, &data[2]); break;
                case 0x80:
                        if (last_guid == 0) {
                            lg.print("SHTP Version: %.*s\n", len, &data[2]); break;
                        } else {
                            lg.print("Version: %.*s\n", len, &data[2]); break;
                        }
                case 0x81:
                        if (last_guid == 0) {
                            lg.print("SHTP Timeout: %dms\n", val); break;
                        } else {
                            lg.print("Report Lengths:\n");
                            auto rpl = Slice{data,2};
                            for ( size_t i = 0; i < rpl.len; i+=2) {
                                lg.print("\t[ 0x%.2x : %d ]\n", rpl[i], rpl[i+1]);
                            }
                            break;
                        }
                default:
                        lg.print("Wrong Tag [%d], len [%d]\n", tag, len); break;
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
            lg.info("error list\n");
            for (auto &d: data) {
                ERR err {d};
                switch (err) {
				case NO_ERR: lg.print("\tNO_ERR\n", err); break;
				case MAX_READ_EXCEED: lg.print("\tMAX_READ_EXCEED\n", err); break;
				case HOST_WRITE_SHORT: lg.print("\tHOST_WRITE_SHORT\n", err); break;
				case MAX_WRITE_EXCEED: lg.print("\tMAX_WRITE_EXCEED\n", err); break;
				case HOST_WRITE_INVALID: lg.print("\tHOST_WRITE_INVALID\n", err); break;
				case FRAGMENT_UNSUPPORTED_START: lg.print("\tFRAGMENT_UNSUPPORTED_START\n", err); break;
				case FRAGMENT_UNSUPPORTED_CONT: lg.print("\tFRAGMENT_UNSUPPORTED_CONT\n", err); break;
				case CMD_NOT_RECOGNIZED: lg.print("\tCMD_NOT_RECOGNIZED\n", err); break;
				case PARAM_NOT_RECOGNIZED: lg.print("\tPARAM_NOT_RECOGNIZED\n", err); break;
				case CHANNEL_NOT_RECOGNIZED: lg.print("\tCHANNEL_NOT_RECOGNIZED\n", err); break;
				case REQ_WHILE_PENDING: lg.print("\tREQ_WHILE_PENDING\n", err); break;
				case WRITE_WHILE_RESP: lg.print("\tWRITE_WHILE_RESP\n", err); break;
				case ERR_TRUNC_LIST: lg.print("\tERR_TRUNC_LIST\n", err); break;
                }
            }
        }

        void handleWakeNormal(View cargo) {
            while (cargo.len) switch (cargo[0]) {
            case 0x05: cargo = handleRotVecReport(cargo); break;
            case 0xFA: cargo = handleRebaseTimeStamp(cargo); break;
            case 0xFB: cargo = handleBaseTimeStamp(cargo); break;
            case 0xFC: cargo = handleFeatureResponse(cargo); break;

            default:
                   lg.warn("unrecognized Report: [0x%.2x]\n", cargo[0]);
                   logData({*cargo.buf,0});
                   return;
            }
        }

        View handleRotVecReport(View cargo) {
            assert(cargo[0] == 0x05); // report ID
            this->r = {
                .r = read16({cargo, 10}) * Q(14),
                .i = read16({cargo, 4}) * Q(14),
                .j = read16({cargo, 6}) * Q(14),
                .k = read16({cargo, 8}) * Q(14),
            };
            this->accuracy = read16({cargo, 12}) * Q(12);
            return {cargo, 14};
        }

        void handleGIRV(View cargo) {
            // note: does not contain report ID
            this->girv.q = {
                .r = read16({cargo, 6}) * Q(14),
                .i = read16({cargo, 0}) * Q(14),
                .j = read16({cargo, 2}) * Q(14),
                .k = read16({cargo, 4}) * Q(14),
            };
            this->girv.a = {
                .x = read16({cargo, 8}) * Q(10),
                .y = read16({cargo, 10}) * Q(10),
                .z = read16({cargo, 12}) * Q(10),
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
            for (auto c: cargo) {
            /* for (unsigned int i = 0; i < cargo.len; ++i) { */
                /* lg.print("0x%.2x ", cargo[i]); */
                lg.print("0x%.2x ", c);
            }
            lg.print("\n");
        }

        struct Feature {
            uint32_t report_interval, batch_interval, config_word;
            uint16_t sensitivity;
            uint8_t flags;
            uint8_t reportID;
        };
        void setFeature(Feature feat) {
            bus.trypush({
                    .dev = this,
                    .data = {
                    /// HEADER
                        21, 0, // length of data
                        2, //channel
                        chans[2].out++, // sequence number
                    /// COMMAND
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
            /* queueOrPush({ */
            /*     .chan = 2, */
            /*     .cargo = { 0xFD, feat.reportID, feat.flags, */
            /*         (uint8_t)feat.sensitivity, (uint8_t)(feat.sensitivity>>8), */
            /*         (uint8_t)(feat.report_interval >> 0), */
            /*         (uint8_t)(feat.report_interval >> 8), */
            /*         (uint8_t)(feat.report_interval >> 16), */
            /*         (uint8_t)(feat.report_interval >> 24), */
            /*         (uint8_t)(feat.batch_interval >> 0), */
            /*         (uint8_t)(feat.batch_interval >> 8), */
            /*         (uint8_t)(feat.batch_interval >> 16), */
            /*         (uint8_t)(feat.batch_interval >> 24), */
            /*         (uint8_t)(feat.config_word >> 0), */
            /*         (uint8_t)(feat.config_word >> 8), */
            /*         (uint8_t)(feat.config_word >> 16), */
            /*         (uint8_t)(feat.config_word >> 24), */
            /*     }, */
            /* }); */
        }

        View handleFeatureResponse(View cargo) {
            (void)cargo;
            lg.print("handling Feature Response.. TODO\n");
            return {cargo, 17};
        }

        void handleControl(View cargo) {
            switch (cargo[0]) {
            case 0xF5: logFRSWriteResponse(cargo); return;
            case 0xF3: logFRSReadResponse(cargo); return;
            default: lg.warn("unhandled message on control channel: [%#x]\n", cargo[0]); logData({*cargo.buf, 0});
            }
        }

        void logFRSWriteResponse(View d) {
            assert(d[0] == 0xF5);
            uint8_t status = d[1];
            uint16_t word = read16({d, 2});
            lg.info("FRS Write Response: offset [%d]\n", word);
            switch (status) {
            case 0: lg.print("Word(s) received\n"); break;
            case 1: lg.print("unrecognized FRS type\n"); break;
            case 2: lg.print("busy\n"); break;
            case 3: lg.print("write completed\n"); break;
            case 4: lg.print("write mode ready\n"); break;
            case 5: lg.print("write failed\n"); break;
            case 6: lg.print("data received while not in write mode\n"); break;
            case 7: lg.print("invalid length\n"); break;
            case 8: lg.print("record valid\n"); break;
            case 9: lg.print("record invalid\n"); break;
            case 10: lg.print("device error\n"); break;
            case 11: lg.print("record is read only\n"); break;
            case 12: lg.print("FRS memory is full\n"); break;
            }
            if (!qs.write.empty()) qs.write.pop();
            sendQ(qs.write);
        }

        void logFRSReadResponse(View d) {
            assert(d[0] == 0xF3);
            lg.info("FRS Read Response:\n");
            uint8_t len = d[1] >> 4;
            uint8_t status = d[1] & 0xf;
            uint16_t offset = read16({d, 2});
            uint32_t w0 = read32({d, 4});
            uint32_t w1 = read32({d, 8});
            switch (status) {
            case 0: lg.print("no error\n"); break;
            case 1: lg.warn("unrecognized FRS type [0x%.4x]\n", read16({d,12})); break;
            case 2: lg.print("busy\n"); break;
            case 3: lg.print("read record completed\n"); break;
            case 5: lg.print("record empty\n"); break;
            case 8: lg.print("device error\n"); break;
            }
            lg.print("offset: %d\n", offset);
            lg.print("length of data: %d\n", len);
            lg.print("data0: %#x\n", w0);
            lg.print("data1: %#x\n", w1);
            lg.print("FRS Type: %#x\n", (uint16_t)read16({d,12}));
            /* if (!qs.read.empty()) qs.read.pop(); */
            /* sendQ(qs.read); */
        }

        void readFRS(uint16_t type) {
            queueOrPush(qs.read, {
                .chan = 2,
                .cargo = {
                    // FRS Read request
                    0xF4,
                    0, 0, 0, //reserved
                    (uint8_t)type, (uint8_t)(type>>8),
                    0, 0,
                },
            });
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

            // write request
            queueOrPush(qs.write, {
                .chan = 2,
                .cargo = {
                    0xF7, // FRS write request
                    0, // reserved
                    7, 0, //length of record in WORDS
                    0xa2, 0xa1, //configuration record id
                },
            });
            // write data
            queueOrPush(qs.write, {
                .chan = 2,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    0, 0, //offset
                    (uint8_t)cfg.ref, (uint8_t)(cfg.ref >> 8),
                    0, 0,
                    (uint8_t)cfg.sync, (uint8_t)(cfg.sync >> 8),
                    (uint8_t)(cfg.sync >> 16), (uint8_t)(cfg.sync >> 24),
                },
            });
            queueOrPush(qs.write, {
                .chan = 2,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    2, 0, //offset
                    (uint8_t)cfg.max_err, (uint8_t)(cfg.max_err >> 8),
                    (uint8_t)(cfg.max_err >> 16), (uint8_t)(cfg.max_err >> 24),
                    (uint8_t)cfg.prediction, (uint8_t)(cfg.prediction >> 8),
                    (uint8_t)(cfg.prediction >> 16), (uint8_t)(cfg.prediction >> 24),
                },
            });
            queueOrPush(qs.write, {
                .chan = 2,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    4, 0, //offset
                    (uint8_t)cfg.alpha, (uint8_t)(cfg.alpha >> 8),
                    (uint8_t)(cfg.alpha >> 16), (uint8_t)(cfg.alpha >> 24),
                    (uint8_t)cfg.beta, (uint8_t)(cfg.beta >> 8),
                    (uint8_t)(cfg.beta >> 16), (uint8_t)(cfg.beta >> 24),
                },
            });
            queueOrPush(qs.write, {
                .chan = 2,
                .cargo = {
                    0xF6, // FRS write data request
                    0, //reserved
                    6, 0, //offset
                    (uint8_t)cfg.gamma, (uint8_t)(cfg.gamma >> 8),
                    (uint8_t)(cfg.gamma >> 16), (uint8_t)(cfg.gamma >> 24),
                    0,0,0,0,
                },
            });
        }

        void callback(const I2C::Request &rq) override {
            if (!rq.opts.read) {
                //delete
                if (rq.data.len >4) {
                    lg.info("sent: ");
                    logData({rq.data,0});
                }
                //delete
                return; // do nothing on successful writes
            }

            if (rq.data.len >= 4) {
                /* bool continuation = rq.data[1] & 0x80; */
                const unsigned int len = (uint16_t)(rq.data[1] & 0x7f) << 8 | rq.data[0];
                if (len == 0xffff) return; // 'length of 65535 is an error.'
                const uint8_t chan = rq.data[2];
                const uint8_t seq = rq.data[3];
                if (len == 0) return; // length of 0 indicates 'no data'
                if (seq != chans[chan].in) {
                    //error?
                    lg.warn("expected seq: %d, got %d\n", chans[chan].in, seq);
                }
                chans[chan].in = seq + 1;
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
                case 0: handleSHTP(data); return;
                /* case 1: handleDevice(data); return; */
                case 2: handleControl(data); return;
                case 3 ... 4: handleWakeNormal(data); return;
                case 5: handleGIRV(data); return;
                }

                lg.print("unrecognized data:\n");
                logData({rq.data, 0});
            }
        }

    Quaternion r;
    double accuracy;
    } shtp;
    enum ADDR {
        DEFAULT = 0b1001010, //0x4a
        ALT = 0b1001011, //0x4b
    };

    BNO085(Sink<I2C::Request> &bus, enum ADDR addr=DEFAULT)
        : shtp{bus, addr} {}

    void getErrors() {
        shtp.sendCommand(SHTP::SND_ERR, 0);
    }

    void getAdvertisement(bool hub) {
        shtp.sendCommand(SHTP::GET_ADV, {hub});
    }

    void checkCargo() {
        shtp.check();
    }

    void enableRotVec(uint32_t Hz=400) {
        shtp.setFeature({
            .report_interval = 1'000'000 / Hz,
            .reportID = 0x05,
        });
    }
    void disableRotVec() {
        shtp.setFeature({
            .report_interval = 0,
            .reportID = 0x05,
        });
    }

    void enableGIRV(uint32_t Hz=500) {
        shtp.configure({
                .ref = SHTP::GIRVConfig::AbsRotVec_9Ax,
        });
        shtp.setFeature({
            .report_interval = 1'000'000 / Hz,
            .reportID = 0x2a,
        });
    }
    void disableGIRV() {
        shtp.setFeature({
            .report_interval = 0,
            .reportID = 0x2a,
        });
    }

    /* void restart() { */
    /*     shtp.device.reset(); */
    /* } */

    Quaternion getQ() {
        return shtp.r;
    }

    Euler getEuler() {
        return shtp.r.toEuler();
    }
};
