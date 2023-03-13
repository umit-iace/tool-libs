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
    Buffer<Vec> P{0};
public:
    void setData(Buffer<double> &&b) override {
        size_t i{0}, off{b.size/2};
        P = Buffer<Vec>{off};
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
    Buffer<Vec> P{0};
public:
    void setData(Buffer<double> &&b) override {
        size_t off{b.size/2};
        P = Buffer<Vec>{off};
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

/* class SmoothTrajectory : public Curve { */
/*     struct Vec { */
/*         double x, y; */
/*     }; */
/*     Buffer<Vec> P{0}; */
/* public: */
/*     void setData(Buffer<double> &&b) override { */
/*         size_t off{b.size/2}; */
/*         P = Buffer<Vec>{off}; */
/*         P.len = off; */
/*         double slope; */
/*         for (size_t i = 0; i < off; ++i) { */
/*             if (i+1 < off) { */
/*                 slope = (b[i+1+off] - b[i+off]) / (b[i+1] - b[i]); */
/*             } else { */
/*                 slope = 0; */
/*             } */
/*             P[i] = {b[i], b[i+off], slope}; */
/*         } */
/*     } */
/*     double getValue(double dx) override { */
/*         if (!P.size) return 0; */
/*         size_t i = (P.size - 1) / 2; */
/*         size_t left = 0, right = P.size - 1; */
/*         while (true) { */
/*             if (left == right) { */
/*                 return P[right].y; */
/*             } */
/*             if (dx < P[i].x) { */
/*                 right = i; */
/*                 i = (left + right) / 2; */
/*                 continue; */
/*             } */
/*             if (dx >= P[i+1].x) { */
/*                 left = i+1; */
/*                 i = (left + right) / 2; */
/*                 continue; */
/*             } */
/*             return P[i].m * (dx - P[i].x) + P[i].y; */
/*         } */
/*     } */
/* }; */
