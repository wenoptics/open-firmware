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

## `calibration`

Use `calibration` to run the full IMU-based wall-calibration sequence. The
device must be `SI_IDLE`; otherwise it publishes `SIMQTT_ERROR_STATUS_INCORRECT`
and does nothing.

### How calibration works

Scribit is a cable-suspended wall-drawing robot. Its two drive cables attach at
the top of the wall; the robot hangs from the cables and draws by reeling them
in or out. Because the cable anchor points are never perfectly known and cables
stretch slightly under load, the robot does not inherently know its absolute
position on the wall — it only knows relative distances moved. Calibration
establishes the true starting position before a print job.

**The core idea: tilt encodes position.**
When the robot hangs in equilibrium the cables pull it diagonally upward at
angles determined by the anchor geometry and the robot's horizontal distance
from each anchor. This produces a measurable forward/backward tilt (pitch). The
further the robot is to one side, the steeper the tilt toward that side. By
sampling the pitch at several known *relative* positions the system has enough
information to reconstruct the *absolute* position on the wall.

**The autocal G-code sequence** (`autocal.GCODE`):

```
M17                 ; energise motors
M92 X29.6 Y-29.6   ; set steps/mm ratio
G91                 ; relative positioning
M777               ; tilt sample 1 — starting corner
G1 Y-150 / M777    ; move 150 mm down,   sample 2
G1 X-150 / M777    ; move 150 mm left,   sample 3
G1 Y+150 / M777    ; move 150 mm up,     sample 4
G1 X+150           ; return to start (no sample needed)
G90 / M18          ; absolute mode, de-energise
```

`M777` is a custom G-code command (`Firmware/MK4duo/…/scribit/m777.h`) that
tells the SAMD21 motion controller to:
1. Finish all pending moves.
2. Wait 500 ms for the IMU (LSM6DSL accelerometer/gyroscope) to settle.
3. Average 10 pitch readings and reply `ok I:<angle>` over serial.

The ESP32 parses each `ok I:` response and appends the angle (in tenths of a
degree) to the `m_imuData` circular buffer.

**Why four points?**
The remote calibration service receives the 4 pitch angles plus the wall ID and
device serial number. Because the relative displacements between sample points
are known (the 150 mm moves), the service can fit the tilt measurements to the
cable-geometry model for that wall and calculate the robot's absolute (X, Y)
coordinates.

**What the service returns:**
- A `G92 X… Y…` command — *set position*. This tells the motion controller
  "you are currently at these coordinates," correcting accumulated cable-length
  error without moving the robot.
- Optionally a `G1 X… Y…` command — *move to start point*. Included when the
  robot needs a physical nudge to reach the correct origin before printing. If
  this is present the firmware executes both commands and retries the whole
  tilt-sampling loop (up to 2 attempts) to verify convergence.

After successful calibration the firmware sends the `<G1-command>` from the
original MQTT payload, which the caller uses to position the robot at the
intended print origin.

### Payload format

```text
<G1-command>;<wall-id>
```

- **`<G1-command>`** — a G-code movement starting with `G1` (e.g.
  `G1 X0 Y0 F1000`). The firmware sends this command to the SAMD21 after a
  successful calibration completes ("send-on-stop" position).
- **`<wall-id>`** — integer wall identifier (e.g. `3`). Stored as the active
  wall; ignored if it matches the already-stored wall ID.

If parsing fails (no `G1` found, or the payload is empty) the firmware publishes
`SIMQTT_ERROR_BAD_CALIBRATION_PAYLOAD` and aborts.

Example:

```text
topic:   tin/30aea4da06f4/calibration
payload: G1 X0 Y0 F1000;3
```

### Execution sequence

1. **IMU check** — if IMU hardware is not responding, publish
   `SIMQTT_ERROR_CANNOT_CALIBRATE` and abort.
2. **Download calibration G-code** — firmware fetches the calibration G-code
   file from `SI_CALIBRATION_GCODE_URL` (configured in `SIConfig.hpp`) and
   writes it to the SPIFFS temp path. Failure → `SIMQTT_ERROR_CANNOT_CALIBRATE`.
3. **Stream G-code to SAMD21** — firmware streams the file to the motion
   controller and transitions to `SI_CALIBRATING` (LEDs: cyan blink).
4. **Collect IMU data** — while in `SI_CALIBRATING`, the main loop accumulates
   IMU readings into a circular buffer. Four data points are expected
   (`SI_CALIBRATION_POINT_NUMBER = 4`).
5. **POST to calibration service** — when the G-code stream ends, firmware
   sends an HTTP POST to `SI_CALIBRATION_URL`:

   ```json
   {
     "sn":     "<6-byte-hex-MAC>",
     "wallId": <integer>,
     "scans":  [<float>, <float>, <float>, <float>]
   }
   ```

   The service responds with `G92` (set-position) and `G1` (move) commands.
   If the response body contains `"err"`, firmware publishes
   `SIMQTT_ERROR_CANNOT_CALIBRATE`.

6. **Apply position correction** — firmware sends the `G92` and `G1` commands
   from the service response to the SAMD21, then sends the `<G1-command>` from
   the original payload.
7. **Retry** — if the service response still contains a `G1` after the
   correction (position not yet converged), the firmware re-runs the calibration
   G-code sequence up to `CALIBRATION_ATTEMPTS_LIMIT = 2` total attempts.

### Responses

| Topic | Payload | Meaning |
|---|---|---|
| `calibrating` | `{"status":0}` | Calibration succeeded |
| `calibrating` | `{"status":1}` | Calibration failed after all retries |
| `error` | `{"Code":0x12,"Description":"..."}` | Device was not idle |
| `error` | `{"Code":0x15,"Description":"..."}` | Payload could not be parsed |
| `error` | `{"Code":0x14,"Description":"..."}` | IMU failure, download failure, or service error |

Outbound topics follow the standard `tout/<robot-id>/<topic>` format.
