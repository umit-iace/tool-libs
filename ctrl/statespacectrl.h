#pragma once
#include <cstdarg>
#include <Eigen/Dense>
using Eigen::Matrix;
using Eigen::Map;

/**
 * generic state space controller template
 *
 * calculates \f$\vec{u} = -K \cdot (\vec{x} - \vec{x}_{des})\f$
 *
 * where
 * \f$\vec{u}\f$ == control inputs
 * \f$\vec{x}\f$ == current state
 * \f$\vec{x}_{des}\f$ == desired state
 * and
 * \f$K\f$ == gain matrix
 */
template<int INPUTS, int STATES>
struct StateSpaceController {
    typedef Matrix<double, INPUTS, 1> OutVector;
    typedef Matrix<double, INPUTS, STATES> GainMatrix;
    typedef Matrix<double, STATES, 1> StateVector;
    OutVector out{};
    GainMatrix k{};

    void setGains(const GainMatrix &gains) {
        k = gains;
    }
    void reset() {
        out.setZero();
    }
    OutVector compute(const StateVector &error) {
        out = - k * error;
        return out;
    }
    OutVector compute(const StateVector &desired, const StateVector &state) {
        return compute(state - desired);
    }
    OutVector operator()() {
        return out;
    }
    inline OutVector operator()(const StateVector &desired, const StateVector &state) {
        return compute(desired, state);
    }
    inline OutVector operator()(const double desired[STATES], const double state[STATES]) {
        return compute(
                (Map<const StateVector>)desired,
                (Map<const StateVector>)state);
    }
    inline OutVector operator()(const double error[STATES]) {
        return compute(
                (Map<const StateVector>)error
                );
    }
};
