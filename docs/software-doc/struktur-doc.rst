Softwarestruktur
================

Hier ist eine Übersicht über die einzelnen Module und wie sie zusammen agieren aufgelistet.

SdCard - FatFs
--------------

.. graphviz:: ../dot/classSdCard.dot

Logger
------

.. graphviz:: ../dot/classLogger.dot

Battery
-------

Die Batteriespannung wird mittels des :ref:`DC-DC-Converter` auf ein benutzbares Level gebracht. Die Batteriespannung
wird mit einem ADC überwacht, bei Unterschreiten von Grenzwerten (:code:`BATTERY_WARN_LEVEL` und :code:`BATTERY_STOP_LEVEL`)
werden die nötigen Maßnahmen getroffen.

Weiterhin überwacht ein :ref:`Thermoelement<Temperatursensor>` die Temperatur.

defines
-------

.. graphviz:: ../dot/define.dot

Car
---

.. graphviz:: ../dot/classCar.dot

.. graphviz:: ../dot/classMotorController.dot

.. graphviz:: ../dot/classTimeTick.dot


Transport
---------

.. graphviz:: ../dot/classTransport.dot

Utils
-----

.. graphviz:: ../dot/classUtils.dot
