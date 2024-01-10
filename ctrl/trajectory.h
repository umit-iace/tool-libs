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
struct Curve {
public:
    virtual void setData(Buffer<double> &&b) {
        assert(false); // ups, virtual call didn't work?
    }
    /** virtual destructor */
    virtual ~Curve() {}

    double operator()(double dx) {
        return getValue(dx);
    }

    virtual double getValue(double) {
        assert(false); // ups, virtual call didn't work?
        return 0;
    }
};

class StepTrajectory : public Curve {
    struct Vec {
        double x, y;
    };
    Buffer<Vec> P;
public:
    void setData(Buffer<double> &&b) override {
        size_t i{0}, off{b.size/2};
        P = Buffer<Vec>(off);
        P.len = off;
        for (auto &v : P) {
            v = {b[i], b[i+off]};
            ++i;
        }
    }
    double getValue(double dx) override {
        if (!P.size) return 0;
        size_t i = (P.size - 1) / 2;
        size_t left = 0, right = P.size - 1;
        while (true) {
            if (left == right) {
                return P[right].y;
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
            return P[i].y;
        }
    }
};

/** Class implementing linear interpolation */
class LinearTrajectory : public Curve {
    struct Vec {
        double x, y, m;
    };
    Buffer<Vec> P;
public:
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
    double getValue(double dx) override {
        if (!P.size) return 0;
        size_t i = (P.size - 1) / 2;
        size_t left = 0, right = P.size - 1;
        while (true) {
            if (left == right) {
                return P[right].y;
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
            return P[i].m * (dx - P[i].x) + P[i].y;
        }
    }
};

class SmoothTrajectory : public Curve {
    size_t n, len;
    double *coeffs;
    double *diffs;
    struct {
        double x0, x1;
        double y0, y1;
    } cfg;
public:
    SmoothTrajectory(std::initializer_list<double> coeffs, size_t n) : coeffs{new double [coeffs.size()]},
                                                                       diffs{new double [n - coeffs.size()]},
                                                                       len(0), n(n){
        for (auto v: coeffs)
            this->coeffs[len++] = v;
    }
    void setData(Buffer<double> &&b) override {
        assert(b.size <= 4);
        cfg = {b[0], b[1], b[2], b[3]};
    }
    double getValue(double x) override {
        if (x < cfg.x0) {
            return cfg.y0;
        } else if (x > cfg.x1) {
            return cfg.y1;
        } else {
            double dy = cfg.y1 - cfg.y0;
            double tau = (x - cfg.x0) / (cfg.x1 - cfg.x0);
            polyVal(tau);

            return cfg.y0 + dy * diffs[0];
        }
    }

    double diff(size_t order){
        return diffs[order];
    }

    void polyVal(double dx) {
        for (int l = 0; l < n - len + 1; ++l) {
            diffs[l] = 0;
        }

        for (int i = n; i >= 0; --i) {
            for (int j = n - len; j > 0; --j) {
                diffs[j] = diffs[j] * dx + diffs[j - 1];
            }
            diffs[0] = diffs[0] * dx + coeffs[i - len];
        }
    }
};
