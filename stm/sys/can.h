#pragma once
#include <utils/queue.h>
#include <comm/can.h>

#include "gpio.h"
#include "registry.h"

namespace CAN {
/** CAN peripheral driver */
struct HW : public CAN {
    inline static Registry<HW, CAN_HandleTypeDef, 2> reg;
    struct Config {
        CAN_TypeDef *can; ///< CAN peripheral
        AFIO rx, tx; ///< alternate function initialized pins
        uint32_t kbaud; ///< baudrate
    };
    HW(const Config &);
    ~HW();
    bool full() override;
    void push(Message &&) override;
    using Sink::push;
    Message pop() override { return rx.pop(); };
    bool empty() override { return rx.empty(); };

    Queue<Message> rx;

    struct TX {
        Queue<Message> q;
        uint8_t active;
    } tx{};

    void irqHandler() { HAL_CAN_IRQHandler(&handle); };
    CAN_HandleTypeDef handle{};
};
}
