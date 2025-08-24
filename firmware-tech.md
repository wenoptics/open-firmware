## Research starting point

- This is the open source firmware for the Scribit device.

- The hardware board is called "MBC-WB", which is a board that has a SAMD21 chip and an ESP32 chip. MBC-WB doc is [./docs/MBC-WB-UserManual_v-2-1-min-1.pdf]()

- There are two parts of the firmware:
  - Firmware/MK4duo - for the SAMD21 chip
  - Firmware/Scribit - for the ESP32 chip


- Firmware/MK4duo/ seems like a copy of the Marlin firmware. Maybe it's good to diff it with the Marlin firmware to see what's different.
  - Make sure to identify the presumable Marlin fork version first
  

  