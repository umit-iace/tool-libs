/** @file ODrive.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <core/kern.h>


/** support for the odriverobotics.com v3 Motor Driver UART interface */
struct ODrive {
    /* incoming stream */
    Source<Buffer<uint8_t>> &in;
    /* outgoing stream */
    Sink<Buffer<uint8_t>> &out;
    // commands we know about / want to support maybe
    static constexpr struct {
        char velocity[15];
        char current[11];
        char get[6];
        char clr[4];
        char feed[6];
        char state[29];
        char readstate[26];
    } cmds {
        .velocity = "v %d %.3f %.f\n", // Motor Velocity FeedForward
        .current = "c %d %.3f\n", // Motor Current
        .get = "f %d\n", // Motor
        .clr = "sc\n", // clear errors
        .feed = "u %d\n", // Motor, feed watchdog
        .state = "w axis%d.requested_state %d\n", // Motor, requested state
        .readstate = "r axis%d.current_state\n", // Motor, read axis state
    };
    enum CMDS { VELO, CURR, GET, CLR, FEED, STATE, READSTATE };
    /** commands the ODrive answers, all others only reply on errors */
    static bool answered(CMDS cmd) { return cmd == GET || cmd == READSTATE; }
    /** axis states from the ODrive firmware that can be requested */
    enum AxisState { IDLE = 1, STARTUP_SEQUENCE = 2, CLOSED_LOOP = 8 };
    struct Motor; //< forward declaration, needed for REQ
    struct REQ {
        enum CMDS cmd;
        Motor *m; // pointer to requesting motor, used for callback with data
        Buffer<uint8_t> out; // data to send out
    };
    /* queue of pending requests, commands behind an unanswered request wait
     * for its answer, so this has to hold a few cycles of all periodic commands */
    Queue<REQ> q{32};
    bool _alive{false};
    bool alive() {
        if (_alive && q.full()) { // stopped responding. assume dead
            _alive = false;
            // flush pending queue
            while (!q.empty()) q.pop();
        }
        return _alive;
    }
    /** single motor controlled by the ODrive interface */
    struct Motor {
        ODrive *drive; ///< pointer to controlling ODrive
        uint8_t side; ///< the ODrive hardware supports 2 Motors
        double pos{0}; ///< last measured position
        double vel{0}; ///< last measured velocity [rot/s]
        int state{0}; ///< last read axis state, 0 if unknown
        /** set speed in rot/s */
        void setSpeed(double speed) {
            if (!drive->alive()) {
                pos = vel = 0;
                return;
            }
            Buffer<uint8_t> cmd(32);
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.velocity, side, speed, 0.);
            drive->q.trypush({
                    .cmd = VELO,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        /** get new measurements from motor */
        void measure() {
            if (!drive->alive()) {
                pos = vel = 0;
                return;
            }
            Buffer<uint8_t> cmd(32);
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.get, side);
            drive->q.trypush({
                    .cmd = GET,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        /** reset the axis watchdog, `v` commands do this as well */
        void feed() {
            if (!drive->alive()) return;
            Buffer<uint8_t> cmd(32);
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.feed, side);
            drive->q.trypush({
                    .cmd = FEED,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        /** request an axis state, e.g. CLOSED_LOOP after the watchdog expired */
        void request(AxisState state) {
            if (!drive->alive()) return;
            Buffer<uint8_t> cmd(32);
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.state, side, state);
            drive->q.trypush({
                    .cmd = STATE,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        /** read the axis state into `state` */
        void readState() {
            if (!drive->alive()) {
                state = 0;
                return;
            }
            Buffer<uint8_t> cmd(32);
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.readstate, side);
            drive->q.trypush({
                    .cmd = READSTATE,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        void callback(REQ req, const Buffer<uint8_t> &resp) {
            assert(req.m == this);
            char str[64];
            terminate(resp, str);
            char *next = nullptr;
            if (req.cmd == READSTATE) {
                // a lone integer, anything else is an answer that belongs elsewhere
                long v = strtol(str, &next, 10);
                if (next != str && *next == 0 && 0 <= v && v < 32) state = v;
                else state = 0;
                return;
            }
            if (req.cmd != GET) return; // ignore responses we didn't request
            pos = strtod(str, &next);
            next++; // white space
            vel = strtod(next, nullptr);
        }
    };
    /** clear the errors of both axes, e.g. after the watchdog expired */
    void clearErrors() {
        if (!alive()) return;
        Buffer<uint8_t> cmd(8);
        cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.clr);
        q.trypush({
                .cmd = CLR,
                .m = nullptr, // no response expected
                .out = std::move(cmd),
                });
        poll();
    }
    /** copy response into a '\0' terminated string for parsing */
    template<size_t N>
    static void terminate(const Buffer<uint8_t> &b, char (&str)[N]) {
        size_t n = b.len < N - 1 ? b.len : N - 1;
        memcpy(str, b.buf, n);
        str[n] = 0;
    }
    /** call this regularly to check for updates */
    void poll() {
        if (!_alive) {
            if (!in.empty()) {
                auto resp = in.pop();
                char str[64];
                terminate(resp, str);
                double volt = strtod(str, nullptr);
                if (23. < volt && volt < 25.) {
                    _alive = true;
                }
            } else if (k.time % 500 == 0) {
                const char rq[] = "r vbus_voltage\n";
                out.trypush({(const uint8_t*)rq, sizeof rq - 1}); // without '\0'
            }
            return;
        }
        while (!in.empty()) {
            auto resp = in.pop();
            // can't receive things without requesting them first
            if (q.empty()) continue;
            auto req = q.pop();
            req.m->callback(req, resp);
        }
        while (!q.empty() && q.front().out.buf) { // command has not been sent
            if (out.full()) break; // try again on next poll
            // marks the command as sent, no matter what the Sink does with it
            Buffer<uint8_t> cmd = std::move(q.front().out);
            out.push(std::move(cmd));
            if (!answered(q.front().cmd)) { // only wait for answered commands
                q.pop();
            }
        }
    }
};
