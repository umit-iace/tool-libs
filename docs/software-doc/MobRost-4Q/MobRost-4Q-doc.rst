STM - 4Q
--------

Firmware für den 4-Quadranten-Motor-Treiber

.. toctree::
    Battery_h
    CanUtils_h
    Car_h
    define_h
    Logger_h
    mcDSA-B60_h
    MotorController_h
    SdCard_h
    Transport_h
    Utils_h

Interrupt Priortäten
~~~~~~~~~~~~~~~~~~~~

.. list-table::
    :header-rows: 1

    * - File
      - Name
      - IRQn
      - PreemptP / SubP
      - Usage
    * - Battery.h
      - Volt DMA
      - DMA2_Stream1_IRQn
      - 3 / 0
      - filters measured voltage on callback
    * -
      - Temp DMA
      - DMA2_Stream0_IRQn
      - 3 / 1
      - | finishes spi read
        | filters measured temperature
    * - Car.h
      - Timer
      - TIM6_DAC_IRQn
      - 0 / 3
      - constant tick server
    * - Min.h
      - DMA
      - DMA2_Stream2_IRQn
      - 2 / 2
      - incoming data transmission
    * -
      - Uart
      - USART6_IRQn
      - 2 / 2
      - incoming data transmission
    * - SdCard.h
      - RX DMA
      - DMA2_Stream3_IRQn
      - 1 / 1
      - Rx SD data
    * -
      - TX DMA
      - DMA2_Stream6_IRQn
      - 1 / 1
      - Tx SD data
    * -
      - SDMMC
      - SDMMC1_IRQn
      - 1 / 1
      - SD comm
    * - CO_driver.c
      - CAN TX
      - CAN1_TX_IRQn
      - 0 / 0
      - CAN Tx data
    * -
      - CAN RX
      - CAN1_RX0_IRQn
      - 0 / 0
      - CAN Rx data
    * - Experiment.h
      - Experiment Timer
      - TIM7_IRQn
      - 4 / 4
      - main loop
