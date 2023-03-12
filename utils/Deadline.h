/** @file Deadline.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

/** simple solution for keeping track of timeouts
 *
 * assign new Deadline to reset, assign 0 to disable
 * e.g.
 * ```
 * deadline = Deadline{now + dt};
 * ```
 */
struct Deadline {
    uint32_t when{};
    /// deadline has passed
    bool operator()(uint32_t now) {
        if (when == 0 || now < when) return false;
        when = 0;
        return true;
    }
};
