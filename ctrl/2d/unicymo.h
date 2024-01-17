/** @file unicymo.h
 *
 * Copyright (c) 2024 IACE
 */
#pragma once

#include "types.h"

namespace Ctrl2D {
/** abstraction of a non-holonomic differentially driven two-wheeled agent
 *
 * a.k.a. the UniCycleModel
 */
struct UniCyMo {
    const double wheelRadius, bodyWidth;


    constexpr WheelRad wheelRadFromWheels(Wheels w) {
        auto tmp = 1 / wheelRadius * w;
        return {tmp.l, tmp.r};
    }
    constexpr Wheels wheelsFromWheelRad(WheelRad w) {
        auto tmp =  wheelRadius * w;
        return {tmp.l, tmp.r};
    }
    constexpr Input inputFromWheels(Wheels w) { return {
        .v = (w.r + w.l) / 2,
        .w = (w.r - w.l) / bodyWidth,
    };}
    constexpr Input inputFromWheelRad(WheelRad w) {
        return inputFromWheels(wheelsFromWheelRad(w));
    }
    constexpr Wheels wheelsFromInput(Input i) { return {
        .l = (i.v - bodyWidth / 2 * i.w),
        .r = (i.v + bodyWidth / 2 * i.w),
    };}
    constexpr WheelRad wheelRadFromInput(Input i) {
        return wheelRadFromWheels(wheelsFromInput(i));
    }
};
}
