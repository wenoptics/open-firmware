# Scribit Calibration Service

HTTP service that implements the remote calibration backend for the Scribit
drawing robot. The firmware (`Firmware/ScribitESP/`) talks to two URLs at
calibration time — both are served here.

---

## What it does

During calibration the Scribit firmware:

1. **Downloads** `autocal.GCODE` (`SI_CALIBRATION_GCODE_URL`) — the
   tilt-sampling G-code sequence that drives the robot through a 150 mm square
   and samples the IMU pitch at each corner via `M777`.
2. **POSTs** the four IMU readings plus a `wallId` to `SI_CALIBRATION_URL`.
   This service solves for the robot's absolute wall position using polargraph
   geometry (scipy least-squares) and returns a `G92 X<L> Y<R>` command the
   SAMD21 uses to set its cable-length position registers.

See `docs/dev/gcode-robot-setup.md §4` for the full theory.

---

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `GET`  | `/autocal.GCODE` | Serves the IMU tilt-sampling G-code file |
| `POST` | `/calibrate`     | Solves position; returns G92 command |
| `GET`  | `/health`        | Liveness check |
| `GET`  | `/docs`          | Auto-generated OpenAPI UI (FastAPI) |

### POST /calibrate

**Request body (JSON):**

```json
{
  "sn":     "aabbccddeeff",
  "wallId": 3,
  "scans":  [-166, -141, -230, -272]
}
```

| Field    | Type       | Description |
|----------|------------|-------------|
| `sn`     | string     | Device MAC hex (informational, not used in computation) |
| `wallId` | int 1–9    | Tape-hole configuration (see table below) |
| `scans`  | 4× number  | Raw IMU pitch values (pitch × 10, as `int16_t`) |

**Response body (plain text):**

```
"G92 X860.23 Y1907.41"
```

The firmware parser (`SIFileDownloader.cpp:321-323`) scans the first body line
for the substring `G92` up to the next `"`. The response is deliberately
formatted to match this expectation.

If the solved position is more than 80 mm from the expected Point Zero for the
given `wallId`, a `G1` nudge suffix is appended. This signals the firmware to
re-run the tilt loop (up to 2 attempts):

```
"G92 X860.23 Y1907.41; G1 X0 Y0 F1000"
```

### Wall ID table

| `wallId` | Tape config | Wall width | Point Zero prior (X, Y) |
|----------|-------------|------------|--------------------------|
| 1 | A 2.0 m | 2000 mm | (800, 700) mm |
| 2 | A 2.25 m | 2250 mm | (900, 790) mm |
| 3 | A 2.5 m | 2500 mm | (1000, 875) mm |
| 4 | A 2.75 m | 2750 mm | (1100, 960) mm |
| 5 | B 3.0 m | 3000 mm | (1200, 1050) mm |
| 6 | B 3.25 m | 3250 mm | (1300, 1140) mm |
| 7 | B 3.5 m | 3500 mm | (1400, 1225) mm |
| 8 | B 3.75 m | 3750 mm | (1500, 1310) mm |
| 9 | B 4.0 m | 4000 mm | (1600, 1400) mm |

Priors are used as the solver initial guess. The solver can recover the true
position even with moderate placement error (tested to ±50 mm). Adjust
`geometry.WALL_CONFIGS` if your tape-hole offsets differ.

---

## Firmware configuration

In `Firmware/ScribitESP/SIConfig.hpp`, set both URLs to point at this service:

```cpp
const char SI_CALIBRATION_URL[]      = "http://<host>:8000/calibrate";
const char SI_CALIBRATION_GCODE_URL[]= "http://<host>:8000/autocal.GCODE";
```

The firmware does not accept URLs with an explicit port for the `print` command,
but the calibration URLs are used differently (direct WiFiClient) and do
support ports.

---

## Running locally

```bash
# Install dependencies
uv sync

# Start the server (hot-reload)
uv run uvicorn main:app --reload --port 8000

# Verify
curl http://localhost:8000/health
curl http://localhost:8000/autocal.GCODE
curl -X POST http://localhost:8000/calibrate \
     -H "Content-Type: application/json" \
     -d '{"sn":"aabbccddeeff","wallId":3,"scans":[-166,-141,-230,-272]}'
```

### Running tests

```bash
uv run pytest
uv run pytest -v          # verbose
uv run pytest -v -x       # stop on first failure
```

---

## Running with Docker

```bash
# Build and start
docker-compose up --build

# Or in background
docker-compose up -d --build

# View logs
docker-compose logs -f

# Stop
docker-compose down
```

The service runs on port **8000** by default. To change it, edit the `ports`
mapping in `docker-compose.yml`.

---

## Project layout

```
calibration-service/
├── main.py           # FastAPI app (routes, request/response models)
├── geometry.py       # Polargraph physics, solver, wall configs
├── autocal.GCODE     # IMU tilt-sampling G-code (copied from ExtraFile/)
├── pyproject.toml
├── Dockerfile
├── docker-compose.yml
└── tests/
    ├── test_geometry.py   # Unit tests: pitch model, solver, round-trips
    └── test_api.py        # Integration tests: all endpoints
```

---

## Tuning the solver

The solver (`geometry.solve_position`) uses `scipy.optimize.least_squares`
(Levenberg-Marquardt) to fit the four observed pitch angles to the polargraph
forward model (`geometry.predicted_pitch`). It is initialised at the `wallId`
prior and converges reliably within ±150 mm of that prior.

**If you observe wrong G92 values:**

1. Check that `wallId` in the MQTT payload matches the tape holes actually used.
2. Verify the robot was hanging freely (not touching the wall) during M777 sampling.
3. Run `uv run pytest -v` to confirm the geometry model is intact.
4. Add a custom `WallConfig` entry in `geometry.WALL_CONFIGS` with better priors
   for your specific installation.
