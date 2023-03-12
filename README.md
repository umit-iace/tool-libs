# Overview

This is a framework including a collection of useful libraries for building
testing rigs developed and used at the UMIT-TIROL Institute of Automation and
Control Engineering. It is designed to be used with
[pyWisp](https://github.com/umit-iace/tool-pywisp), a tool for visualising and
controlling testing rigs.

# Framework
This framework is designed to simplify the implementation of a simple experiment
process. Provided are following building blocks:

## Kernel
\todo fix description

The Kernel provides scheduling for regularly recurring tasks. It provides an
interface for the application to register itself, and has only two invariants:
its `@tick` method must be called every millisecond, and the `idle`
method should be implemented by the platform to not burn through unnecessary
cycles.
After setting up the application, call the non-returning `run`.

## Experiment
The Experiment implements a simple state machine, and provides the entry point
for [pyWisp](https://github.com/umit-iace/tool-pywisp) frame based experiment
control.
On top of the scheduling provided by the Kernel, the Experiment provides
scheduling for tasks during its states, or on the events of state-changes.

## Minimal example
A minimal example for this framework can be found in the `examples` directory.
It simulates a leaky bucket system including a controller and an observer, and
can run both on a connected microcontroller, or on the host system. 

# Sensors
Drivers for the following sensors are implemented:
 - ADS1115 - external analog to digital voltage converter
 - AS5145 - hall effect based angular position measurement
 - BMI160 - 6axis Inertial Measurement Unit
 - MAX31855 - temperature measurement using thermocouples
 - MAX31865 - temperature measurement using temperature dependent resistors like
 the PT100
 
# STM
The Hardware Abstraction Layer from STMicroelectronics are unified and wrapped
in (hopefully) easier to use classes.
Currently only the F4 and F7 families of microcontrollers are supported. The
following classes are implemented:
 - Encoder - quadrature encoder support
 - gpio.h - DIO & AFIO classes encapsulating digital pins
 - HardwareI2C - I2C interface support
 - HardwarePWM - support for outputting Pulse Width Modulated signals on a pin
 - HardwareSPI - SPI interface support
 - HardwareTimer - support for the timer peripherals

# Utils
Some other utilities are provided by this collection:
 - Interpolator - building block for e.g. trajectories
 - MovingAverage - simple sliding window filter
 - SeriesUnpacker - unpack a series of data sent from pyWisp

# Usage
This package provides two CMake library targets:
 - `tool-libs` - providing the framework and utilities
 - `tool-libs-stm` - providing microcontroller support

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
