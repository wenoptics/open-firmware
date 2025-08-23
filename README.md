

## Original firmware behavior

If you are starting fresh in 2025, especially after shutdown of the product - here are some good to know things.

- The left part of the strip LED light is a button. It maybe pressed to reset the device
- The default wifi password for original firmware is `ScribItAP314`. Source: [web-archive: How to connect your Scribit to the WiFi network](https://web.archive.org/web/20240530214903/https://support.scribit.design/hc/en-us/articles/360038467812-How-to-connect-your-Scribit-to-the-WiFi-network)
- After connection to AP, `http://192.168.240.1:8888/` should be available.

- LED light status: [docs/support-scribit-design/led-status.md]()

--- (Original README.md) ---

## HowTo

### Compile the Firmware
- Copy the file `ExtraFile/SIConfig.hpp.example` to `Firmware/ScribitESP/SIConfig.hpp`
- Copy the file `ExtraFile/Mk4duoVersion.h.example` to `Firmware/MK4Duo/Mk4duoVersion.h`
- Copy the file `ExtraFile/ScribitVersion.hpp.example` to `Firmware/ScribitESP/ScribitVersion.hpp`
- Make the necessary configurations and compile

### SDK Installation
Follow the instruction on the document [MBC-W](docs/MBC-WB-UserManual_v-2-1-min-1.pdf) page 38 (Arduino platform manual installation)
To compile correctly, the following steps are required (All files are located in the `Extrafile` folder):
- Copy the files `8MB_ffat.csv` and `8MB_spiffs.csv` to `Arduino/hardware/METECA/mbc-wb/tools/partitions`
- Copy the `arduino-mqtt` folder to `Arduino/libraries`
- Copy the `StepperDriver` folder to `Arduino/libraries`
- Copy the `SERCOM.cpp` file to `Arduino/hardware/METECA/mbc-wb/cores/samd21`


#### New Wi-Fi Configuration
- Connect to the `ScribIt-AP` AP.
- Send a POST request to `http://192.168.240.1:8888`. The body must contain a JSON formatted as follows: `{ "ssid": "networkSSID", "password": "networkPsk" }`.
- The device blinks faster and responds:
  - **200**: The request is correct. The body contains a JSON formatted as follows: `{"ID":"id_device"}`.
  - **400**: Error in the request. The body contains details about the error in the format: `{"error":"error", "ID":"id_device"}`.
- If the connection is successful, the device turns the LEDs green and reboots; otherwise, it turns them red for 2 seconds and waits for a new configuration packet.

#### Delete Saved Wi-Fi
To reset the Wi-Fi configuration, press the button for at least 2 seconds. The device will reboot.

#### OTA Firmware
- Compile the firmware.
- Connect to the AP and open the OTA tool.
- Connect to `192.168.240.1` on port `3232` without a password.
- Follow the document [MBC-W](docs/MBC-WB-UserManual_v-2-1-min-1.pdf)

## Known Bugs
- If you perform an update from a link on SAMD with the serial monitor open, the port may become inaccessible until the first reboot.

## Troubleshooting
- The device reports insufficient space even if the GCODE is much smaller than 5MB:
  - Follow **SDK Installation** and try flashing again.
  - Follow the procedure for flashing the partition table, ensuring that this firmware for the ESP does not coexist with the one for the SAMD partition table.
- After downloading, the robot does not move, and the debug shows many serial errors:
  - Follow **SDK Installation** and verify that the `SERCOM.cpp` file has been correctly overwritten.
