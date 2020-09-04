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
     * Constructor for a general interpolator
     * @param dx array of x values
     * @param dy array of y values
     * @param iCount of values in array
     * @param bSort flag is arrays must sorted
     */
    Interpolator(double *dx, double *dy, const unsigned int iCount, bool bSort) : iCount(iCount) {
        this->P = new Vec[iCount];

        for (unsigned int i = 0; i < iCount; ++i) {
            this->P[i].x = dx[i];
            this->P[i].y = dy[i];
        }

        if (bSort)
            sort();
    }

    ~Interpolator() {
        delete this->P;
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

    unsigned int iCount;                ///< number of points in array
    struct Vec { double x, y;} *P;      ///< points with x and y values

private:
    void sort() {
        Vec vTemp;
        for(unsigned int i = 0; i < iCount - 1; i++) {
            for(unsigned int j = 0; j < iCount - i - 1; j++) {
                if (this->P[j].x > this->P[j + 1].x){
                    vTemp = this->P[j];
                    this->P[j] = this->P[j + 1];
                    this->P[j + 1] = vTemp;
                }
            }
        }
    }
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
    LinearInterpolator(double *dx, double *dy, const unsigned int iCount, bool bSort=false) : Interpolator(dx, dy, iCount, bSort) {}

protected:
    double interpolate(double dx) {
        if (dx <= this->P[0].x) {
            return this->P[0].y;
        } else if (dx >= this->P[this->iCount - 1].x) {
            return this->P[iCount - 1].y;
        }

        for (unsigned int i = 0; i < iCount - 1; ++i) {
            if (dx > this->P[i].x && dx < this->P[i + 1].x) {
                double dm = ((this->P[i + 1].y - this->P[i].y) / (this->P[i + 1].x - this->P[i].x));
                return  dm * (dx - this->P[i].x) + this->P[i].y;
            }
        }
        return 0;
    }
};

#endif //INTERPOLATION_H
