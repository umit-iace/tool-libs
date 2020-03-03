Messungen
---------

Im Zuge der Entwicklung wurden einige Messungen dokumentiert.

.. list-table::
        :header-rows: 1

        * - Datei
          - Info
        * - 20191127_odometry
          - | Messung zur Validierung der Odometrie.
            | Gemessene Distanz: 2.46m
            | Odometrie: 2.43m
        * - 20191127_speed
          - | Messung zur Validierung der Geschwindigkeitsvorgabe.
            | Abgesteckte Strecke von 2m wurde mit einer Geschwindigkeitsvorgabe
            | von 0.5m/s durfahren
            | gemessene Zeit: 4.12 s
        * - wifi_with_transport.csv
          - | Data received during a 'FL-IACE-Traj-Regelung'
            | while the Transport Layer of the MIN protocol was **active**
            | it can be seen that many of the frames got dropped due to timeouts
        * - wifi_wout_transport.csv
          - | Data received during a 'FL-IACE-Traj-Regelung'
            | while the Transport Layer of the MIN protocol was **inactive**
            | all frames seem to have arrived successfully
        * - wifi_wout_transport_ap.csv
          - | Data received during a 'FL-IACE-Traj-Regelung'
            | while the Transport Layer of the MIN protocol was **inactive**
            | with the esp8266 connected to the PC being in **AP Mode**
            | all frames seem to have arrived successfully

