#pragma once

/** abstraction of a non-holonomic differentially driven two-wheeled agent
 *
 * a.k.a. the UniCycleModel
 * @note see comments for units
 */
template<typename FLOAT>
struct UniCyMo {
    static const FLOAT body_dia_m;
    /** in m/s */
    struct Wheels {
        FLOAT l, r;
        Wheels operator+(Wheels w) { return { l+w.l, r+w.r };}
        Wheels operator-(Wheels w) { return { l-w.l, r-w.r };}
    };
    /** in m/s, rad/s */
    struct Input {
        FLOAT v, w;
        Input operator+(Input o) { return { v+o.v, w+o.w };}
        Input operator-(Input o) { return { v-o.v, w-o.w };}
    };
    /* struct State { */
    /*     FLOAT x, y, t; */
    /*     State operator+(State s) { return { x+s.x, y+s.y, t+s.t };} */
    /*     State operator-(State s) { return { x-s.x, y-s.y, t-s.t };} */
    /* }; */

    Wheels measurement; ///< m/s
    Later<Input> in;

    static constexpr Input inputFromWheels(Wheels w) { return {
        (w.r + w.l) / 2,
        (w.r - w.l) / body_dia_m,
    };}
    static constexpr Wheels wheelsFromInput(Input i) { return {
        (i.v - body_dia_m / 2 * i.w),
        (i.v + body_dia_m / 2 * i.w),
    };}
    void step(uint32_t t, uint32_t dt);
    void reset(uint32_t);
};
