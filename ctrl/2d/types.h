/** @file types.h
 *
 * Copyright (c) 2024 IACE
 */
#pragma once

namespace Ctrl2D {

/** Position and Orientation of UniCycleModel in 2D space */
struct Loc {
    double x, y, t;
    friend Loc operator*(float v, Loc a) { return { v*a.x, v*a.y, v*a.t };}
    friend Loc operator+(Loc a, Loc b) { return { a.x+b.x, a.y+b.y, a.t+b.t }; }
    friend Loc operator-(Loc a, Loc b) { return { a.x-b.x, a.y-b.y, a.t-b.t }; }
};

/** reference position + derivatives */
struct Reference {
    double x[4], y[4];
    friend Reference operator+(Reference a, Reference b) { return {
        .x = {
            a.x[0] + b.x[0],
            a.x[1] + b.x[1],
            a.x[2] + b.x[2],
            a.x[3] + b.x[3],
        }, .y = {
            a.y[0] + b.y[0],
            a.y[1] + b.y[1],
            a.y[2] + b.y[2],
            a.y[3] + b.y[3],
        }};
    }
    friend Reference operator-(Reference a, Reference b) { return {
        .x = {
            a.x[0] - b.x[0],
            a.x[1] - b.x[1],
            a.x[2] - b.x[2],
            a.x[3] - b.x[3],
        }, .y = {
            a.y[0] - b.y[0],
            a.y[1] - b.y[1],
            a.y[2] - b.y[2],
            a.y[3] - b.y[3],
        }};
    }
};

/** reference in Invariant coordinates */
struct Invariant {
    double tau[3], nu[3], theta[3];
};

/** in m/s */
struct Wheels {
    double l, r;
    friend Wheels operator*(float v, Wheels w) { return { v*w.l, v*w.r };}
    friend Wheels operator+(Wheels a, Wheels b) { return { a.l+b.l, a.r+b.r };}
    friend Wheels operator-(Wheels a, Wheels b) { return { a.l-b.l, a.r-b.r };}
};

/** in m/s, rad/s */
struct Input {
    double v, w;
    friend Input operator*(float v, Input o) { return { v*o.v, v*o.w };}
    friend Input operator+(Input a, Input b) { return { a.v+b.v, a.w+b.w };}
    friend Input operator-(Input a, Input b) { return { a.v-b.v, a.w-b.w };}
};

/** generate invariant coordinates from cartesian reference */
Invariant invFromRef(const Reference &ref) {
    Invariant inv = {};
    const auto &x = ref.x;
    const auto &y = ref.y;
    auto tau = inv.tau;
    auto nu = inv.nu;
    auto theta = inv.theta;

    theta[0] = atan2(y[1], x[1]);
    float c = cos(theta[0]), s = sin(theta[0]);
    float vsqr = pow(x[1], 2) + pow(y[1], 2);

    theta[1] = (x[1] * y[2] - x[2] * y[1]) / vsqr;
    theta[2] = (-2 * theta[1] * (x[1] * x[2] + y[1] * y[2]) +
                        x[1] * y[3] - x[3] * y[1] ) / vsqr;

    tau[0] = x[0] * c + y[0] * s;
    nu[0] = -x[0] * s + y[0] * c;

    tau[1] = x[1] * c + y[1] * s
                   + theta[1] * nu[0];
    nu[1] = -x[1] * s + y[1] * c
                   - theta[1] * tau[0];

    tau[2] = x[2] * c + y[2] * s
                   + theta[1] * (2 * nu[1] + theta[1] * tau[0])
                   + theta[2] * nu[0];

    nu[2] = -x[2] * s + y[2] * c
                    -theta[1] * (2 * tau[1] - theta[1] * nu[0])
                    -theta[2] * tau[0];
    return inv;
}
}
