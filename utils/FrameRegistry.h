#pragma once
#include "Min.h"

struct FrameRegistry {
    using Handler = void (*)(Frame &f);
    Handler list[64]{};
    void setHandler(uint8_t id, Handler h) {
        list[id] = h;
    }

    void handle(Frame &f) {
        Handler tmp = list[f.id];
        if (tmp) {
            tmp(f);
        }
    }
};
