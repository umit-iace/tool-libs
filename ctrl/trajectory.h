/** @file trajectory.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once

#include <cmath>
#include <utility>
#include <utils/buffer.h>


/** Parametrized Curve
 *
 * useful for trajectory generation
 **/
class Curve {
protected:
    Buffer<double> diffs;
public:
    virtual void setData(Buffer<double> &&b) {
        assert(false); // ups, virtual call didn't work?
    }

    /** virtual destructor */
    virtual ~Curve() {}

    Curve(size_t n=2) : diffs(n) {
        for (size_t i = 0; i < diffs.size; ++i) diffs.append(0);
    }

    /** value & derivatives at dx, valid until the next call */
    const Buffer<double> &operator()(double dx) {
        return getValue(dx);
    }

    virtual const Buffer<double> &getValue(double) {
        assert(false); // ups, virtual call didn't work?
        return diffs;
    }
};

/** Class implementing linear interpolation */
class LinearTrajectory : public Curve {
    struct Vec {
        double x, y, m;
    };
    Buffer<Vec> P;
public:
    LinearTrajectory(size_t diffs=2) : Curve(diffs) {}

    void setData(Buffer<double> &&b) override {
        size_t off{b.size / 2};
        P = Buffer<Vec>(off);
        P.len = off;
        double slope;
        for (size_t i = 0; i < off; ++i) {
            if (i + 1 < off) {
                slope = (b[i + 1 + off] - b[i + off]) / (b[i + 1] - b[i]);
            } else {
                slope = 0;
            }
            P[i] = {b[i], b[i + off], slope};
        }
    }

    const Buffer<double> &getValue(double dx) override {
        if (!P.size) {
            diffs[0] = 0;
            diffs[1] = 0;
            return diffs;
        };
        size_t i = (P.size - 1) / 2;
        size_t left = 0, right = P.size - 1;
        while (true) {
            if (left == right) {
                diffs[0] = P[right].y;
                diffs[1] = 0;
                break;
            }
            if (dx < P[i].x) {
                right = i;
                i = (left + right) / 2;
                continue;
            }
            if (dx >= P[i + 1].x) {
                left = i + 1;
                i = (left + right) / 2;
                continue;
            }
            diffs[0] = P[i].m * (dx - P[i].x) + P[i].y;
            diffs[1] = P[i].m;
            break;
        }
        return diffs;
    }
};

/** Class implementing polynom interpolation
 *
 * between two points the polynom p(tau) of degree `n` is used, tau in [0, 1].
 * `coeffs` are its highest coefficients in ascending order, i.e. for the
 * powers n-coeffs.len+1 .. n, the lower ones are 0.
 * e.g. smoothstep 3tau^2 - 2tau^3: n = 3, coeffs = {3, -2}
 *
 * getValue returns the value and its first coeffs.len-1 derivatives
 */
class SmoothTrajectory : public Curve {
    size_t n;
    Buffer<double> coeffs;
    struct Vec {
        double x, y;
    };
    Buffer<Vec> P;
public:
    SmoothTrajectory(Buffer<double> coeffs, size_t n)
        : Curve(coeffs.len), n(n + 1 < coeffs.len ? coeffs.len - 1 : n),
          coeffs(std::move(coeffs)) {
        assert(n + 1 >= this->coeffs.len); // more coefficients than the degree allows
    }

    void setData(Buffer<double> &&b) override {
        size_t off{b.size / 2};
        P = Buffer<Vec>(off);
        P.len = off;
        for (size_t i = 0; i < off; ++i) {
            P[i] = {b[i], b[i + off]};
        }
    }

    const Buffer<double> &getValue(double x) override {
        for (auto &d: diffs) d = 0;
        if (!P.size) return diffs;

        size_t i = (P.size - 1) / 2;
        size_t left = 0, right = P.size - 1;
        while (true) {
            if (left == right) {
                // outside the points: hold the value, derivatives are 0
                diffs[0] = P[right].y;
                break;
            }
            if (x < P[i].x) {
                right = i;
                i = (left + right) / 2;
                continue;
            }
            if (x >= P[i + 1].x) {
                left = i + 1;
                i = (left + right) / 2;
                continue;
            }
            double dy = P[i + 1].y - P[i].y;
            double dx = P[i + 1].x - P[i].x;
            double tau = (x - P[i].x) / dx;

            // Horner, also dividing out the derivatives:
            // afterwards diffs[l] = p^(l)(tau) / l!
            const size_t lowest = n + 1 - coeffs.len; // power of coeffs[0]
            for (size_t k = n + 1; k-- > 0; ) {
                for (size_t j = diffs.len - 1; j > 0; --j) {
                    diffs[j] = diffs[j] * tau + diffs[j - 1];
                }
                diffs[0] = diffs[0] * tau + (k >= lowest ? coeffs[k - lowest] : 0);
            }

            // scale from tau to x: d^l/dx^l = l! * diffs[l] * dy / dx^l
            double factorial = 1;
            for (size_t l = 0; l < diffs.len; ++l) {
                if (l > 0) {
                    factorial *= l;
                    diffs[l] *= factorial * dy / pow(dx, l);
                } else {
                    diffs[l] = P[i].y + diffs[l] * dy;
                }
            }
            break;
        }

        return diffs;
    }
};

/** Class implementing reference interpolation */
template<int n>
struct Reference {
    double y[n]{};

    size_t size(){
        return n;
    }

    operator double *() {
        return y;
    }

    Reference &operator+=(const Reference &other) {
        for (int i = 0; i < n; ++i) {
            this->y[i] += other.y[i];
        }
        return *this;
    }

    Reference &operator-=(const Reference &other) {
        for (int i = 0; i < n; ++i) {
            this->y[i] -= other.y[i];
        }
        return *this;
    }

    friend Reference operator+(Reference a, const Reference &b) { return a += b; }
    friend Reference operator-(Reference a, const Reference &b) { return a -= b; }
    Reference() { }
    Reference(std::initializer_list<double> list) {
        assert(list.size() == n);
        int i = 0;
        for (auto l = list.begin(); l != list.end(); ++l, ++i) {
            y[i] = *l;
        }
    }
    Reference(const Buffer<double> &b) {
        assert(b.len == n);
        int i = 0;
        for (auto l = b.begin(); l != b.end(); ++l, ++i) {
            y[i] = *l;
        }
    }
};
