/** @file bspline.h
 *
 * Copyright (c) 2026 IACE
 */
#pragma once

#include <cstddef>
#include <utils/buffer.h>
#include "trajectory.h"

/** B-spline of arbitrary degree with its derivatives, e.g. a flat output
 *
 * The spline is computed elsewhere (e.g. scipy.interpolate.make_interp_spline)
 * and sent as one series:
 * ```
 * [p, m, t_0 .. t_{m-1}, c_0 .. c_{m-p-2}]
 * ```
 * with the degree p, m knots t (non decreasing) and m - p - 1 coefficients c,
 * as `(t, c, k)` of a scipy BSpline. The spline is valid on [t_p, t_{m-p-1}],
 * before and after it holds its end values with all derivatives 0, i.e. the
 * trajectory starts and ends at rest. Invalid data clears the spline, it then
 * returns zeros.
 *
 * getValue returns the value and as many derivatives as given to the
 * constructor, derivatives above the degree are 0.
 */
class BSplineTrajectory : public Curve {
public:
    static constexpr size_t MAX_DEGREE = 15;

    BSplineTrajectory(size_t diffs = 4) : Curve(diffs) {}

    void setData(Buffer<double> &&b) override {
        knots = Buffer<double>{};
        coeffs = Buffer<double>{};
        if (b.len < 2) return;
        double pd = b[0], md = b[1];
        if (!(pd >= 1 && pd <= MAX_DEGREE && pd == (size_t)pd && md == (size_t)md)) return;
        size_t p = pd, m = md;
        if (m < 2 * p + 2 || b.len != 2 + m + (m - p - 1)) return;
        for (size_t i = 0; i + 1 < m; ++i) {
            if (!(b[2 + i] <= b[3 + i])) return;  // also rejects NaN
        }
        if (!(b[2 + p] < b[2 + m - p - 1])) return;
        knots = Buffer<double>(m);
        coeffs = Buffer<double>(m - p - 1);
        for (size_t i = 0; i < m; ++i) knots.append(b[2 + i]);
        for (size_t i = 0; i < m - p - 1; ++i) coeffs.append(b[2 + m + i]);
        deg = p;
    }

    /// time [s] the spline starts and ends at, 0 without data
    double begin() const { return knots.len ? knots[deg] : 0; }
    double end() const { return knots.len ? knots[knots.len - deg - 1] : 0; }

    const Buffer<double> &getValue(double x) override {
        for (size_t j = 0; j < diffs.size; ++j) diffs[j] = 0;
        if (!knots.len) return diffs;

        bool hold = x < begin() || x > end();
        if (x < begin()) x = begin();
        if (x > end()) x = end();

        // span k with t_k <= x < t_k+1, the last span includes the end
        size_t n = coeffs.len, p = deg, k = p;
        while (k + 1 < n && knots[k + 1] <= x) ++k;

        // local coefficients of the derivative splines, q[r] belongs to c_{k-p+r}
        double q[MAX_DEGREE + 1];
        for (size_t r = 0; r <= p; ++r) q[r] = coeffs[k - p + r];
        size_t orders = hold ? 1 : diffs.size;
        for (size_t j = 0; j < orders && j <= p; ++j) {
            if (j > 0) {
                // derivative of degree p-j+1 -> p-j, from the back to keep q[r-1]
                for (size_t r = p; r >= j; --r) {
                    size_t i = k - p + r;
                    double h = knots[i + p - j + 1] - knots[i];
                    q[r] = h > 0 ? (p - j + 1) * (q[r] - q[r - 1]) / h : 0;
                }
            }
            diffs[j] = deBoor(q + j, p - j, k, x);
        }
        return diffs;
    }

private:
    Buffer<double> knots, coeffs;
    size_t deg = 0;

    /// value of the degree q spline on span k with the local control points d[0..q]
    double deBoor(const double *d, size_t q, size_t k, double x) const {
        double w[MAX_DEGREE + 1];
        for (size_t s = 0; s <= q; ++s) w[s] = d[s];
        for (size_t r = 1; r <= q; ++r) {
            for (size_t s = q; s >= r; --s) {
                size_t i = s + k - q;
                double h = knots[i + 1 + q - r] - knots[i];
                double a = h > 0 ? (x - knots[i]) / h : 0;
                w[s] = (1 - a) * w[s - 1] + a * w[s];
            }
        }
        return w[q];
    }
};
