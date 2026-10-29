/** @file board.cpp
 *
 * NUCLEO-F767ZI board support for the hardware tests
 *
 * Copyright (c) 2026 IACE
 */
#include "board.h"
#include <sys/uart.h>
#include <sys/spi.h>
#include <sys/i2c.h>
#include <sys/can.h>

namespace board {
UART::HW *vcp;
SPI::HW *spi1;
I2C::HW *i2c1;
CAN::HW *can1;

static void fatal() {
    asm("bkpt");
    while (true);
}

/** 216MHz from the HSI, APB1 54MHz, APB2 108MHz, I2C1 from SYSCLK */
static void clock() {
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 216;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 9;
    osc.PLL.PLLR = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) fatal();
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) fatal();

    RCC_ClkInitTypeDef clk{};
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_7) != HAL_OK) fatal();

    // the I2C timing in stm/i2c.cpp is hardcoded for a 216MHz I2C clock
    RCC_PeriphCLKInitTypeDef periph{};
    periph.PeriphClockSelection = RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_USART3;
    periph.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
    periph.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) fatal();
}

void init() {
    SCB_EnableICache();
    SCB_EnableDCache();
    clock();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void busy_us(uint32_t us) {
    uint32_t start = cycles();
    uint32_t n = us * (SystemCoreClock / 1000000);
    while (cycles() - start < n) ;
}

static void (*hook6)();
static void (*hook7)();

static void stress(TIM_TypeDef *tim, IRQn_Type irq, uint32_t hz) {
    // APB1 timers run at twice PCLK1, count in us
    uint32_t clk = 2 * HAL_RCC_GetPCLK1Freq();
    tim->CR1 = 0;
    tim->PSC = clk / 1000000 - 1;
    tim->ARR = 1000000 / hz - 1;
    tim->CNT = 0;
    tim->EGR = TIM_EGR_UG;
    tim->SR = 0;
    tim->DIER = TIM_DIER_UIE;
    HAL_NVIC_SetPriority(irq, STRESS_PRIO, 0);
    HAL_NVIC_EnableIRQ(irq);
    tim->CR1 = TIM_CR1_CEN;
}
void stress6(uint32_t hz, void (*hook)()) {
    __HAL_RCC_TIM6_CLK_ENABLE();
    hook6 = hook;
    stress(TIM6, TIM6_DAC_IRQn, hz);
}
void stress7(uint32_t hz, void (*hook)()) {
    __HAL_RCC_TIM7_CLK_ENABLE();
    hook7 = hook;
    stress(TIM7, TIM7_IRQn, hz);
}
void stress6_stop() {
    TIM6->CR1 = 0;
    HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    hook6 = nullptr;
}
void stress7_stop() {
    TIM7->CR1 = 0;
    HAL_NVIC_DisableIRQ(TIM7_IRQn);
    hook7 = nullptr;
}
}

using namespace board;

static void enable(IRQn_Type irq) {
    HAL_NVIC_SetPriority(irq, IRQ_PRIO, 0);
    HAL_NVIC_EnableIRQ(irq);
}

extern "C" {
void HAL_UART_MspInit(UART_HandleTypeDef *h) {
    if (h->Instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        enable(USART3_IRQn);
    }
}
void HAL_SPI_MspInit(SPI_HandleTypeDef *h) {
    if (h->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        enable(SPI1_IRQn);
    }
}
void HAL_I2C_MspInit(I2C_HandleTypeDef *h) {
    if (h->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        enable(I2C1_EV_IRQn);
        enable(I2C1_ER_IRQn);
    }
}
void HAL_CAN_MspInit(CAN_HandleTypeDef *h) {
    if (h->Instance == CAN1) {
        __HAL_RCC_CAN1_CLK_ENABLE();
        enable(CAN1_TX_IRQn);
        enable(CAN1_RX0_IRQn);
        enable(CAN1_RX1_IRQn);
        enable(CAN1_SCE_IRQn);
    }
}
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *h) {
    if (h->Instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
}

void USART3_IRQHandler() { if (vcp) vcp->irqHandler(); }
void SPI1_IRQHandler() { if (spi1) spi1->irqHandler(); }
void I2C1_EV_IRQHandler() { if (i2c1) i2c1->irqEvHandler(); }
void I2C1_ER_IRQHandler() { if (i2c1) i2c1->irqErHandler(); }
void CAN1_TX_IRQHandler() { if (can1) can1->irqHandler(); }
void CAN1_RX0_IRQHandler() { if (can1) can1->irqHandler(); }
void CAN1_RX1_IRQHandler() { if (can1) can1->irqHandler(); }
void CAN1_SCE_IRQHandler() { if (can1) can1->irqHandler(); }
void TIM6_DAC_IRQHandler() {
    TIM6->SR = 0;
    if (hook6) hook6();
}
void TIM7_IRQHandler() {
    TIM7->SR = 0;
    if (hook7) hook7();
}
}
