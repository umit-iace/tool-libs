#pragma once

#include <cmath>

namespace CTRL {
/** implementation of the flatness-based quasi-static state-feedback controller
 * cf. Rudolph 2021
 */
template <typename FLOAT, typename OUT>
struct QSFB {
    /** controller gains */
    struct Gains {
        FLOAT tau;  ///< gain in tangential direction
        FLOAT nu[2];///< gain in normal direction
    } k;

    OUT out{};          ///< output values v, w

    void reset() { out = {}; }

    /**
     * Sets the gains for the nu and tau error of the controller
     * @param tau gain value for the tau error
     * @param nu0,nu1 gain values for the nu error
     */
    void setGains(const FLOAT tau, const FLOAT nu0, const FLOAT nu1) {
        k.tau = tau;
        k.nu[0] = nu0;
        k.nu[1] = nu1;
    }

    struct Ref {
        FLOAT tau[3], nu[3], theta[3];
    };

    OUT step(const FLOAT ds, const Ref r,
                    const FLOAT x, const FLOAT y, const FLOAT theta) {
        FLOAT delta = theta - r.theta[0];
        FLOAT xtau = x*cos(r.theta[0])+y*sin(r.theta[0]);
        FLOAT xnu = -x*sin(r.theta[0])+y*cos(r.theta[0]);
        FLOAT omegatau = r.tau[1] - k.tau*(xtau - r.tau[0]);
        FLOAT omegatau1 = r.tau[2] - k.tau*(omegatau - r.tau[1]);
        FLOAT u = (omegatau - r.theta[1]*xnu)/cos(delta);
        FLOAT xnu1 = u*sin(delta) - r.theta[1]*xtau;
        FLOAT enu[2] = {
            xnu - r.nu[0],
            xnu1 - r.nu[1],
        };
        FLOAT omeganu = r.nu[2] - k.nu[1]*enu[1] - k.nu[0]*enu[0];

        FLOAT upper = (-xnu*r.theta[2] - xnu1  *  r.theta[1]) + omegatau1;
        FLOAT lower = (xtau*r.theta[2] + omegatau*r.theta[1]) + omeganu;
        /* FLOAT du = cos(delta)*upper + sin(delta)*lower; */
        FLOAT w = -sin(delta)*upper + cos(delta)*lower;

        out = {
            ds * u,
            ds * (w / u + r.theta[1]),
        };
        return out;
    }
};
}
