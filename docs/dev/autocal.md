# Scribit Autocal (IMU Wall Calibration) Reference

This document covers the IMU tilt-based wall calibration triggered by the
`calibration` MQTT command. For the full MQTT flow see `docs/dev/mqtt-commands.md`.

---

## Physical Overview

Scribit is a cable-suspended wall-drawing robot (polargraph). Two cables attach
at fixed anchor nails at the top of the wall; the robot hangs from both cables
and moves by reeling each cable in or out independently.

**Chips:**

- **ESP32** (`Firmware/ScribitESP/`): Wi-Fi, MQTT, file download, OTA, and
  streaming G-code to the SAMD21 over UART.
- **SAMD21** (`Firmware/MK4duo/`): Marlin-derived motion controller.
  Interprets G-code, drives the stepper motors (cables X/Y, carousel Z), and
  reads the IMU and Hall sensor.

---

## Boot / Power-On State

After a fresh boot (before any calibration), the SAMD21 does **not** home the
XY axes and does not know its absolute position on the wall. It starts with
whatever position is in EEPROM or the Marlin default (typically `0, 0`).

The ESP32 firmware streams a calibration G-code sequence that:

1. Commands known relative moves (150 mm square pattern).
2. Samples IMU tilt at each corner via `M777`.
3. Posts the tilt data to a remote calibration service.
4. The service returns a `G92 X… Y…` command that sets the current absolute
   position — correcting the firmware's accumulated error.

Until `G92` is received, **all XY position knowledge is relative only**.

The Z (carousel) axis also starts at whatever angle happens to be in memory.
Before any carousel move can work reliably, G77 must be run to home it via the
Hall sensor.

---

## Physical Placement (Before Sending the Calibration Command)

1. **Place the robot at "Point Zero"** — the intended drawing origin on the
   wall. For a standard Scribit installation this is the top-left corner of
   the intended drawing area, as marked by the Scribit Measuring Tape:
   - 2 m – 2.75 m walls: use the A1/A2 tape holes.
   - 3 m – 4 m walls: use the B1/B2 tape holes.
2. The robot must be hanging freely on its cables (not resting against a
   surface) so the IMU tilt reading is accurate.
3. The robot must be in the `SI_IDLE` state (not printing).

---

## What the Calibration MQTT Command Does

```
topic:   tin/<robot-id>/calibration
payload: G1 X0 Y0 F1000;3
         │                └─ wall ID (integer)
         └─ move command to execute after calibration (sent-on-stop)
```

The firmware runs this autocal G-code sequence on the SAMD21:

```gcode
M17               ; energise motors
M92 X29.6 Y-29.6 ; set steps/mm
G91               ; relative positioning
M777              ; sample tilt at Point Zero (sample 1)
G1 Y-150          ; move 150 mm down
M777              ; sample tilt (sample 2)
G1 X-150          ; move 150 mm left
M777              ; sample tilt (sample 3)
G1 Y+150          ; move 150 mm up
M777              ; sample tilt (sample 4)
G1 X+150          ; return to start
G90               ; absolute mode
M18               ; de-energise motors
```

`docs/dev/mqtt-commands.md:104-116`

`M777` tells the SAMD21 to average 10 IMU pitch readings and reply
`ok I:<angle>`. The ESP32 collects the four angles.

After the sequence, the ESP32 POSTs the 4 angles plus the wall ID to the
remote calibration service. The service returns:

- `G92 X… Y…` — set the robot's current absolute position.
- Optionally `G1 X… Y…` — nudge the robot to the true print origin.

If a `G1` is returned, the firmware retries the tilt loop (up to
`CALIBRATION_ATTEMPTS_LIMIT = 2` total attempts). On success, it sends the
original `<G1-command>` from the MQTT payload, positioning the robot at the
print start.

`docs/dev/mqtt-commands.md:134-145`, `Firmware/ScribitESP/ScribIt.cpp:735-791`

---

## Is the G92 Value Hardcoded?

**No.** The G92 X Y value comes entirely from the remote calibration service
response and is forwarded as-is to the SAMD21.

**Full data flow:**

1. The firmware downloads the calibration G-code file from
   `SI_CALIBRATION_GCODE_URL` at runtime — the tilt-sampling sequence is
   **not** embedded in the firmware binary (a commented-out
   `SICalibrationGcodes[]` array in `ScribIt.cpp:698` shows the old
   hardcoded approach that was replaced).

2. After streaming the M777 sequence, `completeCalibration()`
   (`ScribIt.cpp:735`) calls `SIFileDownloader::getStartingPosition()`
   (`SIFileDownloader.cpp:247`), which POSTs a JSON payload to
   `SI_CALIBRATION_URL`:

   ```json
   {
     "sn": "<device-MAC-hex>",
     "wallId": <integer from MQTT payload suffix>,
     "scans": [<angle1>, <angle2>, <angle3>, <angle4>]
   }
   ```

3. The remote service returns a JSON body containing a G92 command string.
   The firmware extracts the substring from `"G92"` to the closing `"` and
   forwards it verbatim to the SAMD21 via `forceLineToSAMD()`
   (`SIFileDownloader.cpp:321-323`, `ScribIt.cpp:756-769`).

**No fallback:** if `SI_CALIBRATION_URL` is unreachable or returns a non-200
response, `getStartingPosition()` returns `false` and calibration fails
immediately (`ScribIt.cpp:744-745`). There is no hardcoded fallback G92
position in any code path.

---

## How the Remote Calibration Service Works

The service solves one problem: given four IMU pitch readings collected at
four known *relative* positions on the wall, determine the robot's absolute
wall-XY position at Point Zero, and return it as a `G92 X Y` command.

### What IMU Pitch Encodes

The SAMD21 IMU (`M777`, `m777.h:50-56`) reports **pitch** — the robot body's
forward/back tilt angle averaged over 10 readings, in tenths of a degree
(`angle * 10` stored as `int16_t`, `SISerialManager.cpp:505`). The firmware
sends the value divided by 10, so the calibration service receives degrees.

**Sign convention:** positive pitch = robot tilts left (toward left nail).
Verified empirically: with the robot left of centre (x < D/2, shorter left
cable), `M777` returns a positive angle.

For a polargraph hanging freely on two cables:

```
left nail                    right nail
    ●────────────────────────────●
    │ L                        R │
    │                            │
    │          robot             │
    └──────────────●─────────────┘
```

The robot body hangs with its centre of mass directly below the resultant
cable tension. When `L ≠ R` the body tilts — the shorter cable side pulls the
top of the robot toward it. The resulting **pitch angle θ** at any wall
position `(x, y)` with nail separation `D` is:

```
L = hypot(x, y)          # left cable length
R = hypot(D - x, y)      # right cable length

θ = arctan2(D − 2x, 2y)
  ≈ arcsin((L² − R²) / (2 · D · y))   # small-angle approximation near centre
```

When `x < D/2`: `D − 2x > 0` → `θ > 0` (tilts left, positive). This exact
formula (no negation) is what `geometry.py:predicted_pitch()` implements.

### How the Four Measurements Overdetermine the Position

The calibration G-code (`ExtraFile/autocal.GCODE`) samples tilt at four
corners of a 150 mm square (all moves relative/incremental, starting at Point
Zero):

| Sample | Move from previous | Wall position relative to P0 |
|--------|--------------------|------------------------------|
| P0     | —                  | `(x₀, y₀)`                  |
| P1     | `G1 Y-150` → dR = +150 → robot moves **down** 150 mm | `(x₀, y₀ + 150)` |
| P2     | `G1 X-150` → dL = −150 → robot moves **left** 150 mm | `(x₀ − 150, y₀ + 150)` |
| P3     | `G1 Y+150` → dR = −150 → robot moves **up** 150 mm   | `(x₀ − 150, y₀)` |

> **Note on axis sign convention:** In incremental G91 mode the X G-code
> axis is `dL` and the Y G-code axis is `−dR`. A `G1 Y-150` command means
> `−dR = −150`, so `dR = +150` — the right cable pays out 150 mm and the
> robot descends.

Each pitch reading is a noisy, nonlinear function of the robot's true
`(x, y)`. Having four readings at offsets that are known exactly (150 mm
steps, controlled by the stepper motors) lets the service:

1. **Estimate absolute X:** The pitch changes between P0 and P3 (same row,
   150 mm apart horizontally) are primarily driven by `Δx`. Their difference
   cancels common biases.
2. **Estimate absolute Y:** The pitch changes between P0 and P1 (same column,
   150 mm apart vertically) encode `y₀` because cable-length tilt sensitivity
   varies with height.
3. **Cross-check / reduce noise:** All four readings are consistent with a
   single `(x₀, y₀)` hypothesis; the service can use least-squares or an
   analytic solve over the overdetermined system to reject outliers.

### Role of `wallId`

`wallId` tells the service which Scribit measuring-tape configuration was used:

| Tape holes | Approximate wall width | Known approximate Point Zero |
|------------|------------------------|------------------------------|
| A1 / A2    | 2 m – 2.75 m           | Known XY from tape spec      |
| B1 / B2    | 3 m – 4 m              | Known XY from tape spec      |

This gives the service a **prior**: Point Zero cannot be anywhere on the wall;
it must be near the tape-hole position for the given `wallId`. The prior
dramatically shrinks the search space and disambiguates the nonlinear
tilt-to-position inversion. `D` (nail-to-nail separation) is also derivable
from `wallId`, removing the only other unknown from the cable-length formulas.

### Service Response Format

The service returns a plain-text HTTP body (no JSON envelope for the command
itself — the firmware reads the first non-header line and scans for `"G92"`
to the next `"` character, `SIFileDownloader.cpp:321-323`):

```
"G92 X<x0_mm> Y<y0_mm>"
```

If the computed position is far from the prior (robot not placed correctly),
the service may instead return:

```
"G92 X<corrected_x> Y<corrected_y>; G1 X<nudge_x> Y<nudge_y> F<feed>"
```

The firmware treats the presence of a `G1` in the response as a signal that
the robot needs a physical nudge before position is settled, and retries the
full tilt-sampling loop (up to `CALIBRATION_ATTEMPTS_LIMIT = 2`,
`ScribIt.hpp:10`, `ScribIt.cpp:292-298`).

---

## Tape-Free / Offline Calibration

If the remote service is unavailable, compute `G92 X Y` manually:

1. Measure the nail-to-nail distance `D` in mm with a tape measure.
2. Measure the robot's physical position on the wall: distance from the left
   nail horizontally (`x`) and vertically (`y`).
3. Compute:
   ```python
   import math
   L = math.hypot(x, y)
   R = math.hypot(D - x, y)
   # G92 sets the SAMD21's internal position in LR-equivalent mm.
   # In Style B G-code the firmware works in cable-length space,
   # so G92 X Y here means "left cable = L mm, right cable = R mm".
   print(f"G92 X{L:.2f} Y{R:.2f}")
   ```
   Send this command to the SAMD21 via `manualMove` or include it at the
   head of your G-code file before any moves.
4. Alternatively, use `G92 X0 Y0` at your chosen starting position and
   generate all subsequent moves as incremental deltas from that origin —
   this avoids needing absolute wall coordinates entirely (the Style B SVG
   converter does this when you fix the robot's start point manually).

---

## See Also

- `docs/dev/gcode-reference.md` — G-code dialect, coordinate system, and cable kinematics.
- `docs/dev/pen-carousel.md` — pen carousel mechanics and homing.
- `docs/dev/mqtt-commands.md` — full MQTT command reference including calibration flow details.
- `Firmware/ScribitESP/ScribIt.cpp` — `completeCalibration()`, `ScribIt.cpp:735`.
- `services/calibration-service/geometry.py` — `predicted_pitch()` implementation.
