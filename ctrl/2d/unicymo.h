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
    Later<Input> in;

    Wheels measurement; ///< m/s

    static constexpr Input inputFromWheels(Wheels w) { return {
        (w.r + w.l) / 2,
        (w.r - w.l) / bodyWidth,
    };}
    static constexpr Wheels wheelsFromInput(Input i) { return {
        (i.v - bodyWidth / 2 * i.w),
        (i.v + bodyWidth / 2 * i.w),
    };}
};
}
