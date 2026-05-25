# Scribit G-code Reference

---

## Coordinate System

### Wall XY (logical)

```
(0, 0) ──────────────────────────────→ X (right)
  │
  │   Wall surface
  │
  ↓ Y (down)
```

Origin is the **top-left corner** of the wall. X increases to the right; Y
increases downward (same convention as SVG). Scribit's full wall spans from
`(0, 0)` to approximately `(D, D)` where `D` is the nail-to-nail distance
(default **1860 mm**).

### Cable kinematics (LR space)

The firmware does not work in XY directly. It works in **cable lengths**:

- **L** = length of the left cable (distance from left nail to robot)
- **R** = length of the right cable (distance from right nail to robot)

```python
# .reference/lqu/src/scribit_svg_to_gcode.py:105-109
def xy_to_lr(x_mm, y_mm, D_mm):
    L = math.hypot(x_mm, y_mm)
    R = math.hypot(D_mm - x_mm, y_mm)
    return L, R
```

G-code moves are expressed as **incremental delta-L / delta-R**:

```
G91                        ; incremental mode
G1 X<dL> Y<-dR> F<feed>   ; note: Y axis is negated dR
```

The X G-code axis controls the left cable; the Y axis controls the negative of
the right cable.

### Steps per mm

```
M92 X30.5 Y-30.5 Z22.2222
```

`Firmware/ScribitESP/data/mariM.GCODE:7`  
The Z steps/mm (`22.2222`) governs carousel rotation (degrees → steps).

### Wall center

The "home" drawing position that calibrated G-code uses as its origin is the
**physical center of the wall** at `(D/2, D/2)`:

```python
# .reference/lqu/src/scribit_svg_to_gcode.py:436-437
wall_cx = D_mm / 2.0   # 930 mm for D=1860
wall_cy = D_mm / 2.0   # 930 mm for D=1860
```

---

## G-code Dialect Reference

### Standard G-code used by Scribit

| Code | Purpose | Notes |
|------|---------|-------|
| `G1 X Y F` | Linear move | In G91: `X=dL`, `Y=-dR` (cable deltas) |
| `G4 Pn` | Dwell n milliseconds | Used between moves in old-style files |
| `G4 Sn` | Dwell n seconds | Used by SVG converter for dots |
| `G21` | Millimeter units | Always include at file start |
| `G90` | Absolute positioning | Required for pen Z selection |
| `G91` | Incremental positioning | Default for XY cable moves |
| `G92 X Y Z` | Set current position | Used post-calibration and post-G77 |
| `M17` | Enable motors | Required before any move |
| `M18` | Disable motors | Used at end of calibration G-code |
| `M92 X Y Z` | Set steps/mm | X29.6 Y-29.6 for cables; Z22.2222 for carousel |
| `M400` | Wait for moves to finish | Used in original files, not required by SVG converter |
| `M201/203/204/205` | Acceleration settings | Present in original files |

### Scribit custom G-code (SAMD21)

| Code | Purpose | File |
|------|---------|------|
| `G77` | Home carousel using Hall sensor | `Firmware/MK4duo/src/core/commands/gcode/scribit/g77.h` |
| `G100` | Calibrate pen engagement angle via IMU gyroscope | `…/scribit/g100.h` |
| `G101` | Engage pen (Z down by calibrated angle) | `…/scribit/g101.h` |
| `M777` | Sample IMU pitch; replies `ok I:<angle>` | `…/scribit/m777.h` |

### ESP32 G-code interception (SISerialManager)

When `m_penSensitivity` is enabled, the ESP32 intercepts certain `G1 Z`
commands before forwarding to the SAMD21 and substitutes `G101`:

```cpp
// Firmware/ScribitESP/SISerialManager.hpp:66-76
if(m_penSensitivity &&
  (strstr(line, "G1 Z54")  != nullptr ||   // pen 1 "safe approach"
   strstr(line, "G1 Z126") != nullptr ||   // pen 2
   strstr(line, "G1 Z198") != nullptr ||   // pen 3
   strstr(line, "G1 Z270") != nullptr ||   // pen 4
   strstr(line, "G1 Z49")  != nullptr ||   // pen 1 safety move
   strstr(line, "G1 Z121") != nullptr ||   // pen 2 safety move
   strstr(line, "G1 Z193") != nullptr ||   // pen 3 safety move
   strstr(line, "G1 Z265") != nullptr))    // pen 4 safety move
{
    sprintf(l_actualSentLine, "%s", "G101");
}
```

This interception is for old-style G-code files that used `G1 Z54` etc. for
pen down. The SVG converter bypasses this by using `G101` directly.

When `m_smartCylinder` is **disabled**, the ESP32 replaces any `G77` command
with a no-op `G` (preventing carousel homing). Keep `m_smartCylinder = true`
unless you are debugging.

---

## Two G-code Styles

There are two incompatible G-code styles used historically. Understanding both
prevents confusion:

### Style A — Old absolute style (original Scribit app output)

Used in `Firmware/ScribitESP/data/mariM.GCODE`, `smallTest.gcode`.

```gcode
M17
M92 X30.5 Y-30.5 Z22.2222
G92 X910.75 Y1900.19 Z30    ; establish absolute position from calibration
G1 Z30                       ; pen up (absolute Z position)
G1 X1210.11 Y1734.42         ; move to start (absolute wall XY, NOT LR delta!)
G1 Z0                        ; pen down
G1 X… Y…                     ; draw (absolute wall XY)
G1 Z30                       ; pen up
```

Observations:
- Uses **absolute mode** throughout.
- XY moves are direct **wall XY coordinates** (not cable deltas).
- A single pen at Z=0 (down) and Z=30 (up); no carousel rotation.
- No G77, no G101.
- Initial `G92 X… Y… Z30` establishes absolute position from a prior
  calibration result.

### Style B — SVG converter style (reference script output)

Used by `.reference/lqu/src/scribit_svg_to_gcode.py`.

```gcode
G21              ; mm units
G91              ; incremental mode
M17              ; enable motors
G77              ; home carousel
G92 Z-56         ; set Z reference
; --- select pen (absolute Z, then back to incremental) ---
G90
G1 Z89.000 F600  ; rotate carousel to pen 1
G91
; --- reposition pen-up (segmented LR deltas) ---
G1 X2.341 Y-1.823 F600
…
; --- pen down ---
G101
G101
G101
; --- draw (segmented LR deltas) ---
G1 X0.987 Y-0.543 F300
…
; --- pen up (return to slot Z, CCW) ---
G90
G1 Z89.000 F600
G91
```

Observations:
- Uses **incremental mode (G91)** for XY moves.
- XY moves are **cable length deltas** (from `xy_to_lr`), not wall XY.
- Uses G77 + `G92 Z-56` for carousel homing.
- Uses `G101 × 3` for pen down.
- Long moves are split into segments ≤ `step_mm` (default 1 mm wall distance)
  to keep cable-delta arithmetic accurate.

---

## Minimal G-code File Template

This template covers a single-pen drawing. For multi-pen see
`docs/dev/pen-carousel.md` (CCW wrap bug section).

```gcode
; ===== SCRIBIT SINGLE-PEN DRAWING TEMPLATE =====
; Preconditions: robot is idle, hanging freely at intended start position,
;                calibration has been run (SAMD21 knows its XY position).

G21          ; mm units
G91          ; incremental mode
M17          ; enable motors

; --- Home carousel ---
G77          ; detect Hall sensor magnet reference
G92 Z-56     ; set Z to known post-home reference

; --- Select pen 1 ---
G90
G1 Z89.000 F600
G91

; --- Travel to drawing start (pen up, segmented LR deltas) ---
; (replace with actual computed deltas from xy_to_lr)
G1 X<dL> Y<-dR> F600

; --- Pen down ---
G101
G101
G101

; --- Draw (pen down, segmented LR deltas at slower feed) ---
G1 X<dL> Y<-dR> F300
G1 X<dL> Y<-dR> F300
; ...

; --- Pen up (return to slot Z) ---
G90
G1 Z89.000 F600
G91

; ===== END =====
```

---

## Serial Protocol (ESP32 → SAMD21)

All G-code lines are encapsulated with a line number and XOR checksum before
being sent over UART at 115200 baud:

```
N<line_number> <gcode>*<checksum>\n
```

Example:

```
N0 M17*17
N1 G77*42
```

The SAMD21 replies `ok` per line. On error it replies `Resend N#` and the
ESP32 re-sends from that line.

`Firmware/ScribitESP/SISerialManager.hpp:88-106`

---

## See Also

- `docs/dev/pen-carousel.md` — pen carousel mechanics, homing, engagement, and known CCW wrap bug.
- `docs/dev/autocal.md` — IMU-based wall calibration process and service protocol.
- `docs/dev/mqtt-commands.md` — full MQTT command reference including calibration flow details.
- `.reference/lqu/src/scribit_svg_to_gcode.py` — SVG to G-code converter reference implementation.
- `Firmware/ScribitESP/data/` — example G-code files from original Scribit app (Style A format).
