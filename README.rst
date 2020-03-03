=========
tool-libs
=========

This is a collection of libraries. The library is structured in different parts.

* STM - Hardware specific classes for the STM controller familiy (Support for f4 and f7)

    - :code:`servo` - Class for hardware based PWM derivations

    - :code:`spi` - Classes to use Hardware based SPI functionality

        - :code:`ChipSelect`

        - :code:`SPIRequest`

        - :code:`HardwareSPI`

* Utils - General classes

    - :code:`ExperimentModule` - General class for a experiment module used to describe different functionality of a test rig.

    - :code:`RequestQueue` - Template based class for a request queue


Documentation
-------------

The documentation for all libs can be found in the **docs** directory. To build the documentation Python 3 and doxygen
must be installed. To install the needed Python packages do

.. code-block:: bash

    $ pip install -r requirements.txt

Tu generate the documentation run

.. code-block:: bash

    $ make html

in the **docs** directory.