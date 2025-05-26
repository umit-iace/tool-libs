# Overview

This is a framework including a collection of useful libraries for building
testing rigs developed and used at the UMIT-TIROL Institute of Automation and
Control Engineering. It is designed to be used with
[pyWisp](https://github.com/umit-iace/tool-pywisp), a tool for visualising and
controlling testing rigs.

# Core
This framework is designed to simplify the implementation of a simple experiment
process. The core consists of scheduling capabilities for setting up recurring
and oneshot (Evented) tasks, a batteries-included Experiment structure

## Kernel
The Kernel provides scheduling for regularly recurring tasks. It provides an
interface for the application to register itself, and has only two invariants:
its `tick` method must be called to let it know time passed, and the `idle`
method should be implemented by the platform to not burn through unnecessary
cycles. Two backend implementations are provided: for STM microcontrollers,
actually running the rigs, and for Linux, for simulation purposes.
The Kernel also provides a simple Logger, that can be routed through any Sink
provided by the application, making logging to a file (e.g. simulation), or
through a serial port (on the actual rig) not only possible, but plug-and-play.

## Experiment
The Experiment implements a simple state machine, and provides the entry point
for [pyWisp](https://github.com/umit-iace/tool-pywisp) frame based experiment
control.
On top of the scheduling provided by the Kernel, the Experiment provides
scheduling for tasks during its states, and on the events of state-changes.

## Minimal example
A minimal example for this framework can be found
[on GitHub](https://github.com/umit-iace/labor-rig-templates.git).
It simulates a leaky bucket system including a controller and an observer, and
can run both on a connected microcontroller, or on a linux box.

# Utils
Some other utilities are provided by this collection:
 - Buffer - buffer template for storing things
 - Queue - fifo queue template that is used throughout these libraries
 - Deadline - simple solution for keeping track of timeouts
 - Later - a poor man's additions and subtractions upon access instead of upon
 definition

# Communication
Providing a bunch of helpers for communication with pyWisp, but also for
character streams.
 - Min - main implementation of packing & unpacking data for transfer to / from
 pyWisp
 - FrameRegistry - the place where consumers of Frames can sign up for their
 respective IDs
 - SeriesUnpacker - unpack a series of data sent from pyWisp
 - bufferutils.h & line.h - helpers for manipulating character streams

# Control
Helpers for common problems found in control engineering. Currently only
provides a MovingAverage filter and a simple LinearTrajectory generator, but has
aspirations to grow into a more full-fledged toolbox providing implementations
for PID-, and State feedback controllers, Kalman filter, more sofisticated
trajectories, etc.

# Device Drivers
Drivers for the following sensors are implemented:
 - ADS1115 - external analog to digital voltage converter
 - AS5048B - 14-bit rotary position sensor with digital angle
 - AS5145 - hall effect based angular position measurement
 - BMI160 - 6axis Inertial Measurement Unit
 - BNO085 - 9axis Position/Motion sensor with integrated sensor fusion engine
 - HMC5883L - multi-chip three-axis magnetic sensor
 - MAX31855 - temperature measurement using thermocouples
 - MAX31865 - temperature measurement using temperature dependent resistors like
 the PT100
 - QMC5883L - multi-chip three-axis magnetic sensor
Additionally supported devices include:
 - ODrive - main implementation of communication between ODrive High performance
 motor control to / from  microcontroller

# STM
An implementation for the Kernel backend that makes integration with
microcontrollers a breeze.
The Hardware Abstraction Layers from STMicroelectronics are unified and wrapped
in (hopefully) easier to use classes.
Currently only the F4 and F7 families of microcontrollers are supported. The
following classes are implemented:
 - gpio.h - DIO & AFIO classes encapsulating digital pins
 - ADC::HW - ADC interface support
 - I2C::HW - I2C interface support
 - TIMER::HW - support for the timer peripherals
 - TIMER::PWM - support for outputting Pulse Width Modulated signals on a pin
 - TIMER::Encoder - quadrature encoder support
 - UART::HW - UART interface
 - SPI::HW - SPI interface support

# Linux
An implementation for the Kernel backend for simulation purposes.

# Usage
This package provides two CMake library targets:
 - `tool-libs` - providing the framework and utilities
 - `tool-libs-stm` - providing microcontroller support
 - `tool-libs-linux` - providing linux backend

For no-fuss usage include the following lines in your `CMakeLists.txt`:
``` cmake
    include(FetchContent)
    FetchContent_Declare(tool-libs
            GIT_REPOSITORY https://github.com/umit-iace/tool-libs
            GIT_TAG TOOLLIBVERSION
            )
    FetchContent_MakeAvailable(tool-libs)
    target_link_libraries(YOURTARGETNAME tool-libs)
```

Make sure to set `TOOLLIBVERSION` and `YOURTARGETNAME`

# Documentation
Documentation is provided inline with the implementation, but can also be built
into user-friendly html files. Make sure you have `doxygen` and `dot` (from the
graphviz package) installed, then run
``` shell
doxygen docs/doxyfile && xdg-open html/index.html
```
alternatively building the documentation is also available from a cmake target
so you can build the documentation right alongside your project. The target is
named `tool-libs-docs`

# Contributing

Every bit of help is appreciated! Whether it's just raising issues, fixing
typos, or going as far as to provide support for different controllers or
sensors. The source can be found on
[GitHub](https://github.com/umit-iace/tool-libs).

Thank you for helping out!
