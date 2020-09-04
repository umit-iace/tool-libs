/** @file Interpolation.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef INTERPOLATION_H
#define INTERPOLATION_H

/**
 * Base class for an interpolator
 */
class Interpolator {
public:
    /**
     * Constructor for a general interpolator with sorted arrays
     * @param dx array of x values
     * @param dy array of y values
     * @param count of values in array
     */
    Interpolator(double *dx, double *dy, const unsigned int iCount) : iCount(iCount) {
        this->dx = new double[iCount];
        this->dy = new double[iCount];

        for (unsigned int i = 0; i < iCount; ++i) {
            this->dx[i] = dx[i];
            this->dy[i] = dy[i];
        }
    }

    ~Interpolator() {
        delete dx;
        delete dy;
    }

    /**
     * Operator overload to get interpolated value.
     * @param dx x position to interpolate
     */
    double operator()(double dx) {
        return this->interpolate(dx);
    }

protected:
    virtual double interpolate(double dx) = 0;

    double *dx;             ///< array of x positions
    double *dy;             ///< array of y values
    unsigned int iCount;    ///< number of points in array
};

/**
 * Specific linear interpolator
 */
class LinearInterpolator : public Interpolator {
public:
    /**
     * Constructor for a linear interpolator
     * @param dx array of x values
     * @param dy array of y values
     * @param count of values in array
     */
    LinearInterpolator(double *dx, double *dy, const unsigned int iCount) : Interpolator(dx, dy, iCount) {
    }

protected:
    double interpolate(double dx) {
        if (dx <= this->dx[0]) {
            return this->dy[0];
        } else if (dx >= this->dx[this->iCount - 1]) {
            return this->dy[iCount - 1];
        }

        for (unsigned int i = 0; i < iCount - 1; ++i) {
            if (dx > this->dx[i] && dx < this->dx[i + 1]) {
                return ((this->dy[i + 1] - this->dy[i]) / (this->dx[i + 1] - this->dx[i])) * dx +
                       (this->dy[i] * this->dx[i + 1] - this->dy[i + 1] * this->dx[i]) /
                       (this->dx[i + 1] - this->dx[i]);
            }
        }
        return 0;
    }
};

#endif //INTERPOLATION_H
