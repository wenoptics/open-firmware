

## Original firmware behavior

If you are starting fresh in 2025, especially after shutdown of the product - here are some good to know things.

- The left part of the strip LED light is a button. It can be used to reset the device
- The default wifi password for original firmware is `ScribItAP314`
- After connection to AP, `http://192.168.240.1:8888/` should be available.

- LED light status: [docs/support-scribit-design/led-status.md]()
- More support doc archive in [docs/support-scribit-design]()


## HowTo

### Compile the Firmware

#### Using Docker

Refer to [docker/README.md](docker/README.md) for more details.

#### Using Arduino IDE

<details>
<summary>Click to expand</summary>

- Copy the file `ExtraFile/SIConfig.hpp.example` to `Firmware/ScribitESP/SIConfig.hpp`
- Copy the file `ExtraFile/Mk4duoVersion.h.example` to `Firmware/MK4Duo/Mk4duoVersion.h`
- Copy the file `ExtraFile/ScribitVersion.hpp.example` to `Firmware/ScribitESP/ScribitVersion.hpp`
- Make the necessary configurations and compile

### SDK Installation

- Install the Arduino Legacy IDE (1.8.19).
    - Add board URLs to Arduino IDE in `File > Preferences > Additional Boards Manager URLs`:
        ```
        https://www.briki.org/download/resources/package_briki_index.json
        https://dl.espressif.com/dl/package_esp32_dev_index.json
        ```
    - Go to `Tools > Board > Board Manager` and install the `Briki MBC-WB` board definition.
    - Use the **v2.0.0** version of the `Briki MBC-WB` board (v2.1.7 doesn't compile SAMD board)

- Add additional hardware overrides:
    - Copy the files `8MB_ffat.csv` and `8MB_spiffs.csv` from `ExtraFile/` to `Arduino15/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions`, overwriting the existing files.
    - Copy `ExtraFile/SERCOM.cpp` to `Arduino15/packages/briki/hardware/mbc-wb/2.0.0/cores/samd21`, overwriting the existing file.

- Copy libraries:
    - Copy `ExtraFile/arduino-mqtt` folder to `Firmware/ScribitESP`
    - Copy `ExtraFile/StepperDriver` folder to `Firmware/ScribitESP`

You may also refer to the [MBC-WB User Manual](docs/MBC-WB-UserManual_v-2-1-min-1.pdf) for more details.

</details>

### New Wi-Fi Configuration
- Connect to the `ScribIt-AP` AP.
- Send a POST request to `http://192.168.240.1:8888`. The body must contain a JSON formatted as follows: `{ "ssid": "networkSSID", "password": "networkPsk" }`.
- The device blinks faster and responds:
  - **200**: The request is correct. The body contains a JSON formatted as follows: `{"ID":"id_device"}`.
  - **400**: Error in the request. The body contains details about the error in the format: `{"error":"error", "ID":"id_device"}`.
- If the connection is successful, the device turns the LEDs green and reboots; otherwise, it turns them red for 2 seconds and waits for a new configuration packet.

### Delete Saved Wi-Fi
To reset the Wi-Fi configuration, press the button for at least 2 seconds. The device will reboot.

### OTA Firmware
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

## Acknowledgments

- [@kris-sum](https://github.com/kris-sum)
- [scribit-open/open-firmware](https://github.com/scribit-open/open-firmware)
