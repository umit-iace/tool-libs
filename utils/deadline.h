/** @file deadline.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
#include <cstdint>
/** simple solution for keeping track of timeouts
 *
 * assign new Deadline to reset, assign 0 to disable
 * e.g.
 * ```
 * deadline = Deadline::after(now, dt);
 * ```
 * works across the wrap-around of the uint32_t ms counter (~49.7 days),
 * as long as deadlines are less than ~24.8 days in the future
 */
struct Deadline {
    uint32_t when{};
    /** deadline `dt` after `now`. avoids `when == 0`, which means disabled */
    static Deadline after(uint32_t now, uint32_t dt) {
        uint32_t w = now + dt;
        return {w ? w : 1};
    }
    /// deadline has passed
    bool operator()(uint32_t now) {
        if (when == 0 || (int32_t)(now - when) < 0) return false;
        when = 0;
        return true;
    }
};
