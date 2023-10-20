#pragma once

//cf. https://web2.qatar.cmu.edu/~gdicaro/16311-Fall17/slides/16311-8-Kinematics-DeadReckoning.pdf
namespace Odometry {

template<typename FLOAT>
struct State {
    FLOAT x, y, theta;
};

template<typename FLOAT>
struct Euler {
    State<FLOAT> s;
    State<FLOAT> step(FLOAT v, FLOAT w, FLOAT dt) {
        s.x += dt * v * std::cos(s.theta);
        s.y += dt * v * std::sin(s.theta);
        s.theta += dt * w;
        return s;
    }
    void reset(FLOAT x, FLOAT y, FLOAT theta) {
        s = {x, y, theta};
    }
};

template<typename FLOAT>
struct RungeKutta {
    State<FLOAT> s;
    State<FLOAT> step(FLOAT v, FLOAT w, FLOAT dt) {
        FLOAT Dth = dt * w;
        s.x += dt * v * std::cos(s.theta + Dth/2);
        s.y += dt * v * std::sin(s.theta + Dth/2);
        s.theta += Dth;
        return s;
    }
    void reset(FLOAT x, FLOAT y, FLOAT theta) {
        s = {x, y, theta};
    }
};

template<typename FLOAT>
struct Exact {
    State<FLOAT> s;
    FLOAT sk{0}, ck{1};
    State<FLOAT> step(FLOAT v, FLOAT w, FLOAT dt) {
        if (std::abs(w) < 0.001) {
            s.x += dt * v;
            s.y += dt * v;
        } else {
            s.theta += w * dt;
            FLOAT sk1 = std::sin(s.theta);
            FLOAT ck1 = std::cos(s.theta);
            s.x += v / w * (sk1 - sk);
            s.y -= v / w * (ck1 - ck);
            ck = ck1;
            sk = sk1;
        }
        return s;
    }
    void reset(FLOAT x, FLOAT y, FLOAT theta) {
        s = {x, y, theta};
        sk = std::sin(theta);
        ck = std::cos(theta);
    }
};

}