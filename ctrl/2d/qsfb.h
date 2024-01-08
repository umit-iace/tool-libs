/** @file qsfb.h
 *
 * Copyright (c) 2024 IACE
 */
#pragma once

#include <cmath>
#include "types.h"

namespace Ctrl2D {
/** implementation of the flatness-based quasi-static state-feedback controller
 * cf. Rudolph 2021 */
struct QSFB {
    Later<Invariant> ref;
    Later<double> ds;
    Later<Loc> state;
    /** controller gains */
    struct Gains {
        double tau;  ///< gain in tangential direction
        double nu[2];///< gain in normal direction
    } k;

    Input out{};          ///< output values v, w

    void reset() { out = {}; }

    /**
     * Sets the gains for the nu and tau error of the controller
     * @param tau gain value for the tau error
     * @param nu0,nu1 gain values for the nu error
     */
    void setGains(const double tau, const double nu0, const double nu1) {
        k.tau = tau;
        k.nu[0] = nu0;
        k.nu[1] = nu1;
    }

    Input step(const double ds, const Invariant ref, const Loc l) {
        double delta = l.theta - ref.theta[0];
        double xtau = l.x*cos(ref.theta[0])+l.y*sin(ref.theta[0]);
        double xnu = -l.x*sin(ref.theta[0])+l.y*cos(ref.theta[0]);
        double omegatau = ref.tau[1] - k.tau*(xtau - ref.tau[0]);
        double omegatau1 = ref.tau[2] - k.tau*(omegatau - ref.tau[1]);
        double u = (omegatau - ref.theta[1]*xnu)/cos(delta);
        double xnu1 = u*sin(delta) - ref.theta[1]*xtau;
        double enu[2] = {
            xnu - ref.nu[0],
            xnu1 - ref.nu[1],
        };
        double omeganu = ref.nu[2] - k.nu[1]*enu[1] - k.nu[0]*enu[0];

        double upper = (-xnu*ref.theta[2] - xnu1  *  ref.theta[1]) + omegatau1;
        double lower = (xtau*ref.theta[2] + omegatau*ref.theta[1]) + omeganu;
        /* double du = cos(delta)*upper + sin(delta)*lower; */
        double w = -sin(delta)*upper + cos(delta)*lower;

        out = {
            ds * u,
            ds * (w / u + ref.theta[1]),
        };
        return out;
    }
    void step(uint32_t, uint32_t dt) {
        auto ref = ref.get();
        auto vel = ds.get();
        auto loc = state.get();
        out = step(vel, ref, loc);
    }
};
}
