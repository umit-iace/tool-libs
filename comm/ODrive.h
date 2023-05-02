/** @file ODrive.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <utils/queue.h>


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
        .clr = "sc\n",
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
    struct Motor {
        ODrive *drive;
        uint8_t side;
        double pos{0}, vel{0};
        void setSpeed(double speed) {
            Buffer<uint8_t> cmd{32};
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.velocity, side, speed, 0);
            drive->q.push({
                    .cmd = VELO,
                    .m = this,
                    .out = std::move(cmd),
                    });
            drive->poll();
        }
        void measure() {
            Buffer<uint8_t> cmd{32};
            cmd.len = snprintf((char*)cmd.buf, cmd.size, cmds.get, side);
            drive->q.push({
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

    void poll() {
        while (!in.empty()) {
            assert(!q.empty()); // can't receive things without requesting them first
            auto resp = in.pop();
            auto req = q.pop();
            req.m->callback(req, resp);
        }
        while (!q.empty() && q.front().out.buf) { // command has not been sent
            out.push(std::move(q.front().out));
            if (q.front().cmd != GET) { // only expect response from GET
                q.pop();
            }
        }
    }
};
