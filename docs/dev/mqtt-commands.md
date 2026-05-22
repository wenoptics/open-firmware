# MQTT Remote Commands

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

Source lead: https://github.com/scribit-open/open-firmware/issues/1#issuecomment-3344387860

## Topic Shape

Inbound commands use:

```text
tin/<robot-id>/<command>
```

Firmware publishes outbound status, success, debug, and error messages under its
outbound topic format.

## `manualMove`

Use `manualMove` for short interactive movements. Unlike `print`, it does not
download a URL. The payload is an inline G-code string where semicolons delimit
G-code lines:

```text
G21;G91;M17;G1 X-2.000 Y2.000 F900
```

Firmware behavior in `Firmware/ScribitESP`:

- `SIMQTT.hpp` defines the inbound command suffix as `manualMove`.
- `SIMQTT.cpp` maps topics ending in `manualMove` to
  `SIMQTTMessage::MANUALMOVE`.
- `Scribit_mqtt.cpp` accepts the command only when the device state is
  `SI_IDLE`; otherwise it emits `SIMQTT_ERROR_STATUS_INCORRECT`.
- `ScribIt.cpp::parseAndSaveGcodeString` writes the payload to the temporary
  G-code file and converts each `;` to a newline before streaming it locally.
- When the stream ends and state returns from `SI_MANUAL` to `SI_IDLE`,
  `setState` publishes success for `manualMove`.

Payload notes:

- The firmware MQTT payload limit is `SI_MQTT_MAX_PAYLOAD_LEN = 512`; keep
  manual movements short.
- Newline characters are not required. Send semicolon-delimited G-code lines.
- A status request can be sent before `manualMove`, but the command itself still
  requires the robot to be idle when evaluated.

Example:

```text
topic:   tin/30aea4da06f4/manualMove
payload: G21;G91;M17;G1 X2.000 Y-2.000 F900
```

## `print`

Use `print` for file-sized jobs. The payload is a URL followed by a semicolon
and a suffix command:

```text
http://<host-ip>/<path>;<suffix>
```

The current firmware download path rejects URLs that include `:port`, so local
HTTP serving for `print` jobs should bind on port `80`.
