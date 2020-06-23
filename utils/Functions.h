/** @file Functions.h
 *
 * Copyright (c) 2020 IACE
 */
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

class Functions {
public:
    static double linearInterpolation(double dx, double dx1, double dy1, double dx2, double dy2) {
        if (dx <= dx1) {
            return dy1;
        }
        if (dx >= dx2) {
            return dy2;
        }

        if ((dy2 - dy1) == 0) {
            return dy1;
        }
        if ((dx2 - dx1) == 0) {
            return dy1;
        }

        return ((dy2 - dy1) / (dx2 - dx1)) * dx + (dy1 * dx2 - dy2 * dx1) / (dx2 - dx1);
    }
};

#endif //FUNCTIONS_H
