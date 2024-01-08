/** @file odometry.h
 *
 * Copyright (c) 2024 IACE
 */
#pragma once
#include <utils/later.h>

namespace Ctrl2D {
namespace Odometry {
/** Dead-Reckoning using the Euler Method
 * cf. https://web2.qatar.cmu.edu/~gdicaro/16311-Fall17/slides/16311-8-Kinematics-DeadReckoning.pdf
 **/
struct Euler {
    Loc step(Loc s, Input u, double dt) {
        s.x += dt * u.v * std::cos(s.t);
        s.y += dt * u.v * std::sin(s.t);
        s.t+= dt * u.w;
        return s;
    }
    void reset(State) {}
};

/** Dead-Reckoning using the Runge-Kutta Method
 * cf. https://web2.qatar.cmu.edu/~gdicaro/16311-Fall17/slides/16311-8-Kinematics-DeadReckoning.pdf
 **/
struct RungeKutta {
    Loc step(Loc s, Input u, double dt) {
        double Dth = dt * u.w;
        s.x += dt * u.v * std::cos(s.t + Dth/2);
        s.y += dt * u.v * std::sin(s.t + Dth/2);
        s.t += Dth;
        return s;
    }
    void reset(State) {}
};

/** Dead-Reckoning using the Exact Approach
 * cf. https://web2.qatar.cmu.edu/~gdicaro/16311-Fall17/slides/16311-8-Kinematics-DeadReckoning.pdf
 **/
struct Exact {
    double sk{0}, ck{1};
    Loc step(Loc s, Input u, double dt) {
        if (std::abs(w) < 0.001) {
            // l'Hopital
            s.x += dt * u.v * ck;
            s.y += dt * u.v * sk;
        } else {
            s.t += u.w * dt;
            double sk1 = std::sin(s.t);
            double ck1 = std::cos(s.t);
            s.x += u.v / u.w * (sk1 - sk);
            s.y -= u.v / u.w * (ck1 - ck);
            ck = ck1;
            sk = sk1;
        }
        return s;
    }
    void reset(State s) {
        sk = std::sin(s.theta);
        ck = std::cos(s.theta);
    }
};
}
template <typename T> {
struct Odometer {
    Loc loc;
    T odom;
    Later<Input> track;
    void step(uint32_t, uint32_t dt) {
        odom.step(loc, track.get(), dt*0.001);
    }
    void reset(Loc l) {
        loc = l;
        odom.reset(l);
    }
};
}
