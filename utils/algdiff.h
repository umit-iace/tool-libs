/** @file algdiff.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once

#include <array>
#include <cstddef>

/** algebraic differentiator: FIR filter on the last N equidistant samples
 *
 * The weights estimate one derivative (order 0 smooths the signal) and belong to one sampling
 * time, they are computed offline e.g. with AlgDiff (https://github.com/aothmane-control/Algebraic-differentiators),
 * cf. Mboup, Join, Fliess 2009. w[0] weights the newest sample.
 *
 * T is the sample type, anything with T + T and double * T (double, Eigen vectors, ...)
 *
 * e.g.
 * ```
 * // N = 0, alpha = beta = 2, T = 0.06 s, ts = 0.01 s, mid-point: delay 30 ms
 * AlgDiff<6> speed{{9.9457504521, 14.6473779385, 6.3291139241,
 *                   -6.3291139241, -14.6473779385, -9.9457504521}};
 * double v = speed(angle);  // every 10 ms
 * ```
 */
template<std::size_t N, typename T = double>
struct AlgDiff {
    static_assert(N > 0);

    std::array<double, N> w;  ///< weights, w[0] for the newest sample

    explicit AlgDiff(const std::array<double, N> &w) : w(w) {}

    /** forget the history, the next sample fills all of it (estimate 0 for derivatives) */
    void reset() { empty = true; }

    /** add a sample, returns the estimate delayed by the filter */
    T operator()(const T &sample) {
        if (empty) {
            x.fill(sample);
            empty = false;
        }
        head = (head + 1) % N;
        x[head] = sample;
        T est = w[0] * x[head];
        for (std::size_t i = 1; i < N; ++i) est = est + w[i] * x[(head + N - i) % N];
        return est;
    }

private:
    std::array<T, N> x{};
    std::size_t head = 0;
    bool empty = true;
};
