#pragma once
#include <cstdint>
#include <core/streams.h>
namespace CAN {
struct Message {
    uint64_t data;
    uint32_t id;
    struct {
        uint8_t rtr:1;
        uint8_t ide:1;
        uint8_t dlc:4;
    } opts;
};

struct CAN : public Sink<Message>, public Source<Message> { };
}
