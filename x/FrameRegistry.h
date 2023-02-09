#pragma once
#include "utils/Min.h"

struct FrameRegistry {
    using Arg = void *;
    using Handler = void (*)(Frame &f, Arg);
    struct Container {
        Handler h;
        Arg a;
    };
    Container list[64]{};
    void setHandler(uint8_t id, Handler h, Arg a=nullptr) {
        list[id] = {h, a};
    }

    void handle(Frame f) {
        Container tmp = list[f.id];
        if (tmp.h) {
            tmp.h(f, tmp.a);
        }
    }
};
