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
    } cmds {
        .velocity = "v %d %.3f %.f\n", // Motor Velocity FeedForward
        .current = "c %d %.3f\n", // Motor Current
        .get = "f %d\n", // Motor
        .clr = "sc\n", // clear errors
    };
    enum CMDS { VELO, CURR, GET, CLR };
    struct Motor; //< forward declaration, needed for REQ
    struct REQ {
        enum CMDS cmd;
        Motor *m; // pointer to requesting motor, used for callback with data
        Buffer<uint8_t> out; // data to send out
    };
    /* queue of pending requests */
    Queue<REQ> q{8};
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
        /** set speed in rot/s */
        void setSpeed(double speed) {
            if (!drive->alive()) {
                pos = vel = 0;
                return;
            }
            Buffer<uint8_t> cmd = 32;
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.velocity, side, speed, 0);
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
            Buffer<uint8_t> cmd = 32;
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.get, side);
            drive->q.trypush({
                    .cmd = GET,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        void callback(REQ req, const Buffer<uint8_t> &resp) {
            assert(req.m == this);
            if (req.cmd != GET) return; // ignore responses we didn't request
            char *next = nullptr;
            pos = strtod((char*)resp.buf, &next);
            next++; // white space
            vel = strtod(next, nullptr);
        }
    };
    /** call this regularly to check for updates */
    void poll() {
        if (!_alive) {
            if (!in.empty()) {
                auto resp = in.pop();
                double volt = strtod((char*)resp.buf, nullptr);
                if (23. < volt && volt < 25.) {
                    _alive = true;
                }
            } else if (k.time % 500 == 0) {
                out.trypush({(const uint8_t*)"r vbus_voltage\n", 16});
            }
            return;
        }
        while (!in.empty()) {
            assert(!q.empty()); // can't receive things without requesting them first
            auto resp = in.pop();
            auto req = q.pop();
            req.m->callback(req, resp);
        }
        while (!q.empty() && q.front().out.buf) { // command has not been sent
            out.trypush(std::move(q.front().out));
            if (q.front().cmd != GET) { // only expect response from GET
                q.pop();
            }
        }
    }
};
