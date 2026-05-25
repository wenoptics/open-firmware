"""
Scribit calibration HTTP service.

Endpoints
---------
POST /calibrate
    Receive four IMU pitch scans, solve for wall position, return G92 command.

GET /autocal.GCODE
    Serve the autocal G-code file the firmware fetches from SI_CALIBRATION_GCODE_URL.

GET /health
    Liveness check.

Response format (POST /calibrate)
----------------------------------
The firmware parser (SIFileDownloader.cpp:321-323) reads the first body line
after the HTTP header and scans for the substring "G92" up to the next '"'.
The body must therefore look like:

    "G92 X<L> Y<R>"

(with literal surrounding double-quotes on that line).

If the solution is too far from the prior (robot not at Point Zero), the
service returns a nudge response that the firmware recognises by the presence
of "G1", causing it to retry:

    "G92 X<L> Y<R>; G1 X0 Y0 F1000"
"""

import math
import os
from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.responses import PlainTextResponse, FileResponse
from pydantic import BaseModel, field_validator

from geometry import (
    WALL_CONFIGS,
    NUDGE_THRESHOLD_MM,
    solve_position,
    format_g92,
    sample_positions,
)

AUTOCAL_GCODE_PATH = Path(__file__).parent / "autocal.GCODE"

app = FastAPI(title="Scribit Calibration Service", version="0.1.0")


# ---------------------------------------------------------------------------
# Request / response models
# ---------------------------------------------------------------------------

class CalibrateRequest(BaseModel):
    sn: str                  # device serial (MAC hex string), informational
    wallId: int              # 1-9, selects WallConfig
    scans: list[float]       # four raw IMU readings (pitch × 10, int16_t)

    @field_validator("wallId")
    @classmethod
    def wall_id_must_be_known(cls, v: int) -> int:
        if v not in WALL_CONFIGS:
            raise ValueError(
                f"wallId {v} is not in the known range 1-{max(WALL_CONFIGS)}. "
                "Add it to geometry.WALL_CONFIGS."
            )
        return v

    @field_validator("scans")
    @classmethod
    def scans_must_be_four(cls, v: list[float]) -> list[float]:
        if len(v) != 4:
            raise ValueError(f"Expected exactly 4 scan values, got {len(v)}")
        return v


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.get("/health")
def health() -> dict:
    return {"status": "ok"}


@app.get("/autocal.GCODE", response_class=PlainTextResponse)
def serve_autocal_gcode() -> PlainTextResponse:
    """
    Serve the IMU tilt-sampling G-code sequence.
    The firmware fetches this URL before each calibration run
    (SI_CALIBRATION_GCODE_URL in SIConfig.hpp).
    """
    if not AUTOCAL_GCODE_PATH.exists():
        raise HTTPException(status_code=404, detail="autocal.GCODE not found")
    return PlainTextResponse(AUTOCAL_GCODE_PATH.read_text())


@app.post("/calibrate", response_class=PlainTextResponse)
def calibrate(req: CalibrateRequest) -> PlainTextResponse:
    """
    Solve for the robot's Point Zero position from four IMU pitch readings
    and return a G92 command string for the SAMD21.

    The response body is a single line with the firmware-expected format:
        "G92 X<L> Y<R>"

    If the solved position is far from the wallId prior (suggesting the robot
    was not placed at Point Zero correctly), a G1 nudge suffix is appended so
    the firmware retries after moving to the corrected position:
        "G92 X<L> Y<R>; G1 X0 Y0 F1000"
    """
    cfg = WALL_CONFIGS[req.wallId]

    try:
        x0, y0 = solve_position(
            req.scans,
            D_mm=cfg.D_mm,
            x_prior_mm=cfg.x_prior_mm,
            y_prior_mm=cfg.y_prior_mm,
        )
    except Exception as exc:
        raise HTTPException(status_code=422, detail=f"Solver failed: {exc}") from exc

    # Sanity-check: y0 must be positive (robot below nails)
    if y0 <= 0:
        raise HTTPException(
            status_code=422,
            detail=f"Solver returned non-physical y0={y0:.1f} mm (must be > 0). "
                   "Check IMU readings and wallId.",
        )

    g92 = format_g92(x0, y0, cfg.D_mm)

    # Distance from prior
    dist = math.hypot(x0 - cfg.x_prior_mm, y0 - cfg.y_prior_mm)
    if dist > NUDGE_THRESHOLD_MM:
        # Robot appears to not be at Point Zero — include G1 so firmware retries
        body = f'"{g92}; G1 X0 Y0 F1000"'
    else:
        body = f'"{g92}"'

    # Body is a single line; firmware skips HTTP headers then reads first line.
    return PlainTextResponse(body)
