=========
tool-libs
=========

This is a collection of libraries used at the UMIT-Tirol Institute of
Automation and Control Engineering. It includes the following components:

* Sensors - Classes implementing specific sensors used in our test rigs

  - ``ADS1115`` - external analog to digital voltage converter

  - ``AS1545`` - hall effect based angular position measurement

  - ``IMU3000`` - inertial measurement unit 3 ax Gyro + 3 ax Accelerometer

  - ``MAX31855`` - temperature measurement using the MAX31855 with thermocouples

  - ``MAX31865`` - temperature measurement using the MAX31865 with temperature
    dependent resistors like the PT100

* STM - Hardware specific classes for the STM controller familiy (Support for f4 and f7)

  - ``encoder`` - Class for hardware based quadratic encoder support

  - ``gpio`` - Classes encapsulating digital pins

  - ``hal`` - points to the correct HAL include for the used family

  - ``i2c`` - Classes to use hardware based I2C functionality using ``RequestQueue``

    - ``I2CRequest``

    - ``HardwareI2C``

  - ``pwm`` - Class for hardware based pwm outputs

  - ``spi`` - Classes to use hardware based SPI functionality using ``RequestQueue``

    - ``ChipSelect``

    - ``SPIRequest``

    - ``HardwareSPI``

  - ``timer`` - Template class for hardware based Timers

  - ``uart`` - Template class for hardware based UART

* Utils - General classes

  - ``Experiment`` - General class of an experiment used to control the experiment flow at a test rig.

  - ``ExperimentModule`` - General class of an experiment module used to describe different functionality of a test rig.

  - ``MovingAverage`` - Template class that implements a moving average filter

  - ``RequestQueue`` - Template based class for a request queue

  - ``RingBuffer`` - Template class that implements a ring buffer

  - ``Tick`` - Classes to realise a general time server at that clients can connect.

    - ``TickClient`` - Class describing a client to the TickServer.

    - ``TickServer`` - Class describing a tick server to a TickClient.

  - ``Transport`` - Layer class for the min protocol communication between the host and the microcontroller


Documentation
-------------

The documentation for all libs can be found in the **docs** directory. To build the documentation Python 3 and doxygen
must be installed. To install the needed Python packages run

.. code-block:: bash

    $ pip install -r requirements.txt

and to generate the documentation run

.. code-block:: bash

    $ make html

in the **docs** directory.

Contributing
------------

Every bit of help is appreciated! Whether it's just fixing typos, or adding
support for different controllers or sensors, we try keeping things in order.

So please keep lists in the documentation (e.g. this file) sorted alphabetically,
and help keeping the commit history as clean as possible. Consider working in
feature branches, and make sure to rebase your commits on top of master before
merging.

Thank you for helping out!

