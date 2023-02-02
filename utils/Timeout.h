/** @file Timeout.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

/** simple solution for keeping track of timeouts
 *
 * assign new Timeout to reset, assign 0 to disable
 * e.g.
 * ```
 * timeout = Timeout{now + dt};
 * ```
 */
struct Timeout {
    uint32_t when{};
    /// time expired
    bool operator()(uint32_t now) {
        if (when == 0) return false;
        return now >= when;
    }
};
