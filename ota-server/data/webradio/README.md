Plaats hier de OTA artefacten voor de ESP32 firmware.

Benodigd:
- firmware.bin  : de gecompileerde firmware die OTA geflasht moet worden
- version.txt   : alleen de versie-string, bijv. 1.4.2

De server levert automatisch:
- /webradio/version.json  (met velden version + bin)
- /webradio/firmware.bin
