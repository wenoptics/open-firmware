(https://github.com/scribit-open/open-firmware/issues/1#issuecomment-3344387860)

MQTT Remote Commands

> Send to topic: `tin/[MAC_ADDRESS]/[COMMAND]`

> - `print` - Download and execute G-code from URL
> - `erase` - Download and execute erasing G-code
> - `status` - Request device status
> - `reset` - Reset the device
> - `pause` - Pause/resume current job
> - `manualMove` - Execute manual G-code movements
> - `calibration` - Start calibration sequence
> - `update` - Initiate firmware update
> - `wifiConfig` - Update WiFi settings

These are defined here: https://github.com/scribit-open/open-firmware/blob/main/Firmware/ScribitESP/SIMQTT.hpp#L9 which adds:

- `resetConfig` - does a `wifiDisconnect` and `ESP.restart`
