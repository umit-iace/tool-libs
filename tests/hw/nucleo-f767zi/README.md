# Hardware tests: NUCLEO-F767ZI

On-target tests for the STM32 backend of tool-libs (`stm/`) and for the
parts of `core/` & `comm/` that depend on real interrupts. These complement
the host tests in `tests/`, which can't see races between interrupt handlers
and the main loop, the real HAL, or peripheral timing.

## Requirements

- NUCLEO-F767ZI connected via USB (on-board ST-Link). **No wiring needed.**
- `arm-none-eabi-gcc` (with newlib-nano), CMake ≥ 3.25, optionally Ninja
- `openocd` ≥ 0.11
- Python 3 with `pyserial`
- network access on the first build: CMake fetches the pinned ST sources
  (`stm32f7xx_hal_driver` v1.3.3, `cmsis_device_f7` v1.2.10,
  `cmsis_core` v5.9.0). Local copies can be used with
  `-DFETCHCONTENT_SOURCE_DIR_STM32F7_HAL=...` etc.
- permissions for the ST-Link USB device (openocd udev rules) and the
  serial port (`uucp` group on Arch, `dialout` on Debian/Ubuntu)

## Usage

```sh
cd tests/hw/nucleo-f767zi
./run.py                 # build, flash, run all tests
./run.py --repeat 20     # rerun 20 times (board reset in between), for races
./run.py --no-build --no-flash
./run.py --baud 921600   # rebuilds with a faster VCP
```

Exit code: `0` everything passed (skips allowed), `1` test failures,
`2` setup problem or hang. Each run writes a log to `build/hwtest-*.log`.

When the firmware stops talking (hang, HardFault, failed `assert`), `run.py`
attaches gdb through openocd and prints the backtrace, registers and fault
status registers.

Without `run.py`: `cmake -S . -B build && cmake --build build --target flash`,
then open the VCP at 115200 baud and send `g`.

## Pins

| peripheral | pins | notes |
|---|---|---|
| USART3 | PD8 TX, PD9 RX | ST-Link virtual COM port, test reports |
| SPI1 | PA5 SCK (D13), PA6 MISO (D12), PB5 MOSI (D22) | CS PD14 (D10), PD15 (D9) |
| I2C1 | PB8 SCL (D15), PB9 SDA (D14) | internal pull-ups, 400kHz |
| CAN1 | PD0 RX, PD1 TX | silent loopback, no transceiver needed |
| TIM3 CH1 | PB4 (D25) | PWM, duty cycle measured on the pin |
| TIM6, TIM7 | – | interrupt load for the stress tests |
| ADC1, DMA2 stream 0 | PA3 (A0) analog | measures the internal VREFINT, A0 is only configured |

Optional:
- jumper **PB5 ↔ PA6** (MOSI ↔ MISO) enables `spi_loopback_data`
- sensors on I2C1 are listed by `i2c_scan`

## Tests

| test | checks |
|---|---|
| `critical_nesting`, `critical_blocks_irq` | `CriticalSection` nesting & PRIMASK restore, interrupts really blocked |
| `kernel_recurring` | `Kernel` time vs. HAL tick, `every()` period from SysTick |
| `kernel_early` | a global registering with the `Kernel` before it is constructed (`test_early.cpp` is linked before `main.cpp`) |
| `kernel_overrun` | more calls due in one ms than the kernel queue holds: no exit, the queue grows in the main loop |
| `scheduler_isr_stress` | `Scheduler` fed from a 20kHz interrupt while `Registry::add` grows the list |
| `malloc_isr_stress` | heap used from the main loop and a 50kHz interrupt (`__malloc_lock`), no corruption, no leak |
| `logger`, `printf_float` | `Logger`/`BufferedLogger` truncation with newlib-nano, float formatting linked in |
| `logger_blocking` | logging into a full sink from an interrupt / with interrupts disabled returns right away, bounded wait otherwise |
| `pwm_registers`, `pwm_output` | `TIMER::PWM` register values, period 0, measured duty cycle |
| `spi_until` | `SPI::until` deadline calculation |
| `spi_transfers` | MOSI/MISO/BOTH, chip selects, mode switching, long transfer vs. deadline |
| `spi_loopback_data` | received data (needs the jumper) |
| `spi_timeout` | a transfer whose interrupt never comes is dropped quickly (interrupts are off meanwhile), bus continues |
| `spi_queue_stress` | 2000 requests pushed while completions run in the interrupt, the device callbacks run in the main loop, none lost |
| `i2c_nack` | NACKed requests are dropped, queued requests keep going without another push |
| `i2c_scan` | lists devices, async driver still works afterwards |
| `i2c_timeout` | a transfer whose interrupt never comes is given up, the HAL accepts the next one right away |
| `can_bittiming` | BTR prescaler/segments give 500kbit/s, sample point 80–90% |
| `can_loopback` | std/ext ids, all DLCs (no stale bytes beyond DLC), more messages than TX mailboxes |
| `can_rx_overflow` | full RX queue drops in the interrupt, recovers |
| `uart_retry` | the HAL refusing to start a transmission is counted and retried, no breakpoint |
| `adc_dcache` | `ADC::HW` with circular DMA into cached SRAM1: sequence length, cache maintenance, VREFINT vs. factory calibration |
| `uart_min_echo` | host driven: `UART::HW` + `Min::In`/`Min::Out` echo incl. worst case byte stuffing, then a burst at full line rate; fails if `stm/uart.cpp` had to abort a transfer (counted by wrapping `HAL_UART_AbortReceive/Transmit`) or allocated in the receive interrupt |
| `uart_overrun` | host driven: 6000 bytes while interrupts are blocked 3ms every 10ms, only what arrives while blocked may get lost |

## Adding tests

Write `void test_xyz()` in one of the `test_*.cpp` files (or a new one,
added to `CMakeLists.txt`), using `CHECK`, `CHECK_EQ`, `REQUIRE`,
`hwt::info`, `hwt::warn` and `hwt::skip` from `hwtest.h`, and list it in
the `tests[]` table in `main.cpp`. Peripherals that interrupt handlers need
are published through the pointers in `board.h`.

RAM below 0x20020000 is DTCM, which the D-cache doesn't cover. Buffers
that have to exercise cache maintenance go into SRAM1 with
`__attribute__((section(".sram1")))`, see `test_adc.cpp`.

The firmware reports over the VCP with lines starting with `HWT `, see
`hwtest.h` for the format.
