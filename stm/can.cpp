#include "sys/can.h"

using namespace CAN;

void _start(CAN_HandleTypeDef *handle, const Message &msg) {
    uint32_t mbox;
    CAN_TxHeaderTypeDef header = {
        .StdId = msg.opts.ide ? 0 : msg.id & 0x7ff,
        .ExtId = msg.opts.ide ? msg.id : 0,
        .IDE = msg.opts.ide ? CAN_ID_EXT : CAN_ID_STD,
        .RTR = msg.opts.rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA,
        .DLC = msg.opts.dlc,
    };
    HAL_CAN_AddTxMessage(handle, &header, (uint8_t*)&msg.data, &mbox);
    HW::reg.from(handle)->tx.active |= mbox;
}
template<int N>
void _tx_complete(CAN_HandleTypeDef *handle) {
    auto can = HW::reg.from(handle);
    assert(can->tx.active & 1<<N);
    can->tx.active &= ~(1<<N);
    while (can->tx.q.size() && HAL_CAN_GetTxMailboxesFreeLevel(handle)) {
        _start(handle, can->tx.q.front());
        can->tx.q.drop();
    }
}
template<int N>
void _rx_complete(CAN_HandleTypeDef *handle) {
    CAN_RxHeaderTypeDef header{};
    Message msg{};
    HAL_CAN_GetRxMessage(handle, N?CAN_RX_FIFO1:CAN_RX_FIFO0, &header, (uint8_t*)&msg.data);
    // the HAL copies all 8 bytes, the ones beyond dlc are stale register content
    if (header.RTR) msg.data = 0;
    else if (header.DLC < 8) msg.data &= (1ull << (8 * header.DLC)) - 1;
    msg.id = header.IDE ? header.ExtId : header.StdId;
    msg.opts = {
        .rtr = (bool)header.RTR,
        .ide = (bool)header.IDE,
        .dlc = (uint8_t)header.DLC,
    };
    auto &rx = HW::reg.from(handle)->rx;
    if (!rx.full()) {
        rx.push(std::move(msg));
    } else {
        // dropping Message, reader should call more often...
    }
}

HW::HW(const Config &c) {
    HW::reg.reg(this, &handle);

    /* Can speed configuration, derived from the CAN (APB1) clock. */
    /* use the first segment layout that divides the clock evenly. */
    /* sample points between 80% & 89%, cf. http://bittiming.can-wiki.info */
    /* e.g. 54MHz (F7 @ 216MHz) -> 18tq, 42MHz (F4 @ 168MHz) -> 14tq */
    static constexpr struct { uint8_t bs1, bs2; } segments[] = {
        {15, 2}, {13, 2}, {12, 2}, {11, 2}, {10, 2}, {9, 2}, {7, 2},
    };
    const uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    const uint32_t baud = c.kbaud * 1000;
    uint32_t Prescaler = 0, ts1 = 0, ts2 = 0;
    for (auto seg : segments) {
        uint32_t tq = 1 + seg.bs1 + seg.bs2;
        if (baud == 0 || pclk % (baud * tq)) continue;
        Prescaler = pclk / (baud * tq);
        if (Prescaler > 1024) continue;
        ts1 = (uint32_t)(seg.bs1 - 1) << CAN_BTR_TS1_Pos;
        ts2 = (uint32_t)(seg.bs2 - 1) << CAN_BTR_TS2_Pos;
        break;
    }
    assert(ts1); // don't know how to generate that baud from the CAN clock

    /* Configure CAN module registers */
    /* Configuration is handled by CubeMX HAL*/
    handle = {
        .Instance = c.can,
        .Init = {
            .Prescaler = Prescaler,
			.Mode = CAN_MODE_NORMAL,
			.SyncJumpWidth = CAN_SJW_1TQ,
            .TimeSeg1 = ts1,
            .TimeSeg2 = ts2,
			.TimeTriggeredMode = DISABLE,
			.AutoBusOff = DISABLE,
			.AutoWakeUp = DISABLE,
			.AutoRetransmission = ENABLE,
			.ReceiveFifoLocked = DISABLE,
			.TransmitFifoPriority = DISABLE,
        },
    };

    /* CLEAR_BIT(c.can->MCR, 1 << 16); // freeze CAN during debug */
    while (HAL_CAN_Init(&handle) != HAL_OK);

    /* accept _all_ received messages i.e. activate empty filters */
    CAN_FilterTypeDef FilterConfig = {
        .FilterActivation = true,
    };
    while (HAL_CAN_ConfigFilter(&handle, &FilterConfig) != HAL_OK) ;


    HAL_CAN_RegisterCallback(&handle, HAL_CAN_TX_MAILBOX0_COMPLETE_CB_ID, _tx_complete<0>);
    HAL_CAN_RegisterCallback(&handle, HAL_CAN_TX_MAILBOX1_COMPLETE_CB_ID, _tx_complete<1>);
    HAL_CAN_RegisterCallback(&handle, HAL_CAN_TX_MAILBOX2_COMPLETE_CB_ID, _tx_complete<2>);
    HAL_CAN_RegisterCallback(&handle, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, _rx_complete<0>);
    HAL_CAN_RegisterCallback(&handle, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, _rx_complete<1>);

    while (HAL_CAN_ActivateNotification(&handle,
                                     CAN_IT_RX_FIFO0_MSG_PENDING |
                                     CAN_IT_RX_FIFO1_MSG_PENDING |
                                     CAN_IT_TX_MAILBOX_EMPTY)) ;

    while (HAL_CAN_Start(&handle) != HAL_OK) ;
}

HW::~HW() {
    HAL_CAN_DeactivateNotification(&handle,
                                   CAN_IT_RX_FIFO0_MSG_PENDING |
                                   CAN_IT_RX_FIFO1_MSG_PENDING |
                                   CAN_IT_TX_MAILBOX_EMPTY);
    HAL_CAN_Stop(&handle);
    HW::reg.unreg(this);
}
bool HW::full() {
    return tx.q.full();
}
void HW::push(Message &&msg) {
    // otherwise a tx complete interrupt in between could leave msg stuck in q
    CriticalSection cs;
    if (HAL_CAN_GetTxMailboxesFreeLevel(&handle)) {
        _start(&handle, std::move(msg));
    } else {
        tx.q.push(std::move(msg));
    }
}
