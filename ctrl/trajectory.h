/** @file trajectory.h
 *
 * Copyright (c) 2023 IACE
 */
#pragma once
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
    Curve() : diffs(2) {
        for (int i = 0; i < diffs.size; ++i) diffs.append(0);
    }
    Curve(size_t n) : diffs(n) {
        for (int i = 0; i < diffs.size; ++i) diffs.append(0);
    }

    Buffer<double>  operator()(double dx) {
        return getValue(dx);
    }

    virtual Buffer<double> getValue(double) {
        assert(false); // ups, virtual call didn't work?
        return 0;
    }
};

/** Class implementing linear interpolation */
class LinearTrajectory : public Curve {
    struct Vec {
        double x, y, m;
    };
    Buffer<Vec> P;
public:
    LinearTrajectory() : Curve(2) {}
    LinearTrajectory(size_t diffs) : Curve(diffs) {}

    void setData(Buffer<double> &&b) override {
        size_t off{b.size/2};
        P = Buffer<Vec>(off);
        P.len = off;
        double slope;
        for (size_t i = 0; i < off; ++i) {
            if (i+1 < off) {
                slope = (b[i+1+off] - b[i+off]) / (b[i+1] - b[i]);
            } else {
                slope = 0;
            }
            P[i] = {b[i], b[i+off], slope};
        }
    }
    Buffer<double> getValue(double dx) override {
        if (!P.size) {
            diffs[0] = 0;
            diffs[1] = 0;
            return diffs;
        };
        size_t i = (P.size - 1) / 2;
        size_t left = 0, right = P.size - 1;
        while (true) {
            if (left == right) {
                diffs[0] =  P[right].y;
                diffs[1] = 0;
                break;
            }
            if (dx < P[i].x) {
                right = i;
                i = (left + right) / 2;
                continue;
            }
            if (dx >= P[i+1].x) {
                left = i+1;
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

/** Class implementing polynom interpolation */
class SmoothTrajectory : public Curve {
    size_t n;
    Buffer<double> coeffs;
    struct Vec {
        double x, y;
    };
    Buffer<Vec> P;
public:
    SmoothTrajectory(Buffer<double> coeffs, size_t n) : coeffs{coeffs}, n(n), Curve(coeffs.len) {}
    void setData(Buffer<double> &&b) override {
        size_t off{b.size/2};
        P = Buffer<Vec>(off);
        P.len = off;
        double slope;
        for (size_t i = 0; i < off; ++i) {
            P[i] = {b[i], b[i+off]};
        }
    }
    Buffer<double> getValue(double x) override {
        for (int l = 0; l < diffs.len; ++l)  diffs[l] = 0;

        size_t i = (P.size - 1) / 2;
        size_t left = 0, right = P.size - 1;
        while (true) {
            if (left == right) {
                diffs[0] =  P[right].y;
                diffs[1] = 0;
                break;
            }
            if (x < P[i].x) {
                right = i;
                i = (left + right) / 2;
                continue;
            }
            if (x >= P[i+1].x) {
                left = i+1;
                i = (left + right) / 2;
                continue;
            }
            double dy = P[i+1].y - P[i].y;
            double dx = P[i+1].x - P[i].x;
            double tau = (x - P[i].x) / dx;

            for (int k = n; k >= 0; --k) {
                for (int j = n - diffs.len; j > 0; --j) {
                    diffs[j] = diffs[j] * tau + diffs[j - 1];
                }
                if (n - k >= coeffs.len)
                    diffs[0] *= tau;
                else
                    diffs[0] = diffs[0] * tau + coeffs[k - coeffs.len];
            }

            for (int l = 0; l < diffs.len; ++l) {
                if (l > 0)
                    diffs[l] *= dy * l / pow(dx, l);
                else
                    diffs[l] = P[i].y + diffs[l] * dy;
            }
            break;
        }

        return diffs;
    }
};
