/** @file odometry.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once

#include <cmath>

/** pose of a unicycle (differential drive) in the plane */
struct Pose {
    double x = 0;     ///< [m]
    double y = 0;     ///< [m]
    double theta = 0; ///< [rad] heading, continuous (not wrapped)
};

/** dead reckoning of a unicycle over one step with constant v and w
 *
 * exact for the arc driven with constant speed v [m/s] and turn rate w
 * [rad/s] over dt [s]: the chord of length v·dt·sinc(w·dt/2) in the mean
 * heading. No case split for straight driving, the series of sinc covers
 * small turns.
 */
inline Pose odometryStep(const Pose &p, double v, double w, double dt) {
    double h = w * dt / 2;
    double sinc = std::fabs(h) < 1e-4 ? 1 - h * h / 6 : std::sin(h) / h;
    double s = v * dt * sinc;
    double mean = p.theta + h;
    return {p.x + s * std::cos(mean), p.y + s * std::sin(mean), p.theta + 2 * h};
}
