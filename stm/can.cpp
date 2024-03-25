#include "can.h"

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
        _start(handle, can->tx.q.pop());
    }
}
template<int N>
void _rx_complete(CAN_HandleTypeDef *handle) {
    CAN_RxHeaderTypeDef header{};
    Message msg{};
    HAL_CAN_GetRxMessage(handle, N?CAN_RX_FIFO1:CAN_RX_FIFO0, &header, (uint8_t*)&msg.data);
    msg.id = header.IDE ? header.ExtId : header.StdId;
    msg.opts = {
        .rtr = (bool)header.RTR,
        .ide = (bool)header.IDE,
        .dlc = (uint8_t)header.DLC,
    };
    HW::reg.from(handle)->rx.push(std::move(msg));
}

HW::HW(const Config &c) {
    HW::reg.reg(this, &handle);

    /* Can speed configuration. */
    /* Based on the values obtained from http://bittiming.can-wiki.info */
    /* Assuming CAN clock is 108 MHz */

    uint32_t Prescaler = 27;
    uint32_t ts1 = CAN_BS1_13TQ;
    uint32_t ts2 = CAN_BS2_2TQ;

    switch (c.kbaud) {
        case 1000:
            Prescaler = 3;
            ts1 = CAN_BS1_15TQ;
            ts2 = CAN_BS2_2TQ;
            break;
        case 500:
            Prescaler = 6;
            ts1 = CAN_BS1_15TQ;
            ts2 = CAN_BS2_2TQ;
            break;
        case 250:
            Prescaler = 12;
            ts1 = CAN_BS1_15TQ;
            ts2 = CAN_BS2_2TQ;
            break;
        case 125:
            Prescaler = 27;
            ts1 = CAN_BS1_13TQ;
            ts2 = CAN_BS2_2TQ;
            break;
        default :
            assert(false); // don't know that baud
    }
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
}
void HW::push(Message &&msg) {
    if (HAL_CAN_GetTxMailboxesFreeLevel(&handle)) {
        _start(&handle, std::move(msg));
    } else {
        tx.q.push(std::move(msg));
    }
}
