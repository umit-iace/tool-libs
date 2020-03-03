.. _dc-dc-converter:

DC-DC-Converter
---------------

Die Hauptversorgungsspannung von ~24VDC wird mit einem DC-DC Converter aus der Batteriespannung (42..32V) erzeugt.

Hierfür wird der `i6A <https://www.mouser.at/ProductDetail/TDK-Lambda/I6A4W010A033V-001-R?qs=sGAEpiMZZMsc0tfZmXiUnRtXTJWqek%2FHvmxREa8jon3pMXNg81GgeQ%3D%3D>`_
von TDK-Lambda verwendet. Eine Platine wurde designed die den Anforderungen der Roboter entspricht.
Die Ausgagnsspannung lässt sich durch einen Widerstand zwischen Pin 6 und Masse einstellen.
Der genaue Zusammenhang für den Widerstand ist laut Datenblatt:

.. math::

  R=\frac{0.6\cdot36500}{V_\text{out}-0.6}-511

Für die gewünschte Ausgangsspannung von 24V ergibt sich R also zu 424.9 Ohm. Letztendlich wurde ein Widerstand von 390 Ohm
verwendet, da dieser schon vorhanden war, und die Spannung sich nur auf 24.9V erhöht.
Mit Pin 2 kann man den DC/DC Wandler an, bzw. aus schalten. An den Pin wurde ein Mosfet gelegt, der mit einer
Selbsthalteschaltung gesteuert wird. Der spannungsüberwachende Mikrocontroller kann diese Schaltung unterbrechen, um so
einen simplen Unterspannungsschutz zu realisieren. Darüber hinaus besteht die Möglichkeit einen Notausschalter vor
den Spannungswandler zu schalten.

.. figure:: ../resources/powersupplyschematic.png
   :align: center

   Schaltung des Spannungswandlers

.. figure:: ../resources/powersupplypcb.png
   :align: center

   PCB des Spannungswandlers


