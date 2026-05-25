"""
Polargraph IMU-based position solver for Scribit.

The autocal sequence (ExtraFile/autocal.GCODE) samples the IMU pitch at four
positions forming a 150 mm square, starting at Point Zero (x0, y0):

    P0 = (x0,       y0)         sample 1  (start)
    P1 = (x0,       y0 + 150)   sample 2  G1 Y-150 → dR=+150 → robot moves down
    P2 = (x0 - 150, y0 + 150)   sample 3  G1 X-150 → dL=-150 → robot moves left
    P3 = (x0 - 150, y0)         sample 4  G1 Y+150 → dR=-150 → robot moves up

The IMU reports the body pitch angle θ (degrees, averaged over 10 readings).
For a robot hanging freely from two cables of lengths L and R, with nail
separation D, the equilibrium tilt is:

    L = hypot(x, y)
    R = hypot(D - x, y)

    θ = arctan2(D - 2x, 2y)

Positive θ means robot tilts left (x < D/2, left cable shorter). This matches
the sign reported by the SAMD21 IMU ("ok I:<angle>").

Because the small-angle approximation breaks down far from centre, we model the
full geometry and use scipy least-squares to find (x0, y0) that best fits all
four observed pitch values.

Angles arrive already in degrees: the firmware stores pitch×10 as int16_t
internally but sends the divided value over serial (SIFileDownloader.cpp:272).
"""

import math
from dataclasses import dataclass

import numpy as np
from scipy.optimize import least_squares


# ---------------------------------------------------------------------------
# Wall configuration table
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class WallConfig:
    """Physical parameters for a given wallId."""
    D_mm: float          # nail-to-nail horizontal distance
    x_prior_mm: float    # expected Point Zero X (from tape spec)
    y_prior_mm: float    # expected Point Zero Y (from tape spec)
    label: str


# wallId → WallConfig
# The Scribit measuring tape defines two size classes (A and B).
# Point Zero for each class sits at a fixed offset from the left nail,
# as determined by the tape-hole layout.  These values are approximate;
# the solver uses them as an initial guess and prior, not a hard constraint.
#
# (0, 0) ──────────────────────────────→ X (right)
#   │
#   │   Wall surface
#   │
#   ↓ Y (down)
#
# Origin is the **top-left corner** of the wall. X increases to the right; Y
# increases downward (same convention as SVG). Scribit's full wall spans from
# `(0, 0)` to approximately `(D, D)` where `D` is the nail-to-nail distance

WALL_CONFIGS: dict[int, WallConfig] = {
    # A-tape: 2 m – 2.75 m walls  (A1/A2 holes)
    # Point Zero sits roughly 25 % in from left and 65 % down from top.
    1: WallConfig(D_mm=2000, x_prior_mm=500,  y_prior_mm=1300, label="A 2.0 m"),
    2: WallConfig(D_mm=2250, x_prior_mm=562,  y_prior_mm=1463, label="A 2.25 m"),
    3: WallConfig(D_mm=2500, x_prior_mm=625,  y_prior_mm=1625, label="A 2.5 m"),
    4: WallConfig(D_mm=2750, x_prior_mm=688,  y_prior_mm=1788, label="A 2.75 m"),
    # B-tape: 3 m – 4 m walls     (B1/B2 holes)
    5: WallConfig(D_mm=3000, x_prior_mm=750,  y_prior_mm=1950, label="B 3.0 m"),
    6: WallConfig(D_mm=3250, x_prior_mm=812,  y_prior_mm=2113, label="B 3.25 m"),
    7: WallConfig(D_mm=3500, x_prior_mm=875,  y_prior_mm=2275, label="B 3.5 m"),
    8: WallConfig(D_mm=3750, x_prior_mm=938,  y_prior_mm=2438, label="B 3.75 m"),
    9: WallConfig(D_mm=4000, x_prior_mm=1000, y_prior_mm=2600, label="B 4.0 m"),
}

# Offset step used by autocal.GCODE (mm)
AUTOCAL_STEP_MM = 150.0

# How far from the prior (mm) triggers a nudge response instead of direct G92
NUDGE_THRESHOLD_MM = 80.0


# ---------------------------------------------------------------------------
# Core geometry
# ---------------------------------------------------------------------------

def predicted_pitch(x: float, y: float, D: float) -> float:
    """
    Return the equilibrium IMU pitch angle (degrees) for a polargraph robot at
    wall position (x, y) with nail separation D.

    Sign convention: robot tilts left (toward left nail) → positive pitch.
    This matches the SAMD21 IMU reports: firmware replies "ok I:<angle>" where
    angle is positive when the robot is left of centre (shorter left cable).

    Derivation: the robot hangs so its body aligns with the bisector of the two
    cable directions. The net horizontal cable force is proportional to (D - 2x),
    and the vertical component to 2y, giving:

        θ = arctan2(D - 2x, 2y)

    When x < D/2 (left of centre): D - 2x > 0 → positive θ.
    When x > D/2 (right of centre): D - 2x < 0 → negative θ.
    """
    if y <= 0:
        raise ValueError(f"y must be positive (robot below nails), got {y}")
    return math.degrees(math.atan2(D - 2.0 * x, 2.0 * y))


def sample_positions(x0: float, y0: float) -> list[tuple[float, float]]:
    """Return the four (x, y) positions visited during the autocal sequence."""
    s = AUTOCAL_STEP_MM
    return [
        (x0,       y0),        # P0: start (Point Zero)
        (x0,       y0 + s),    # P1: G1 Y-150 → down
        (x0 - s,   y0 + s),    # P2: G1 X-150 → left
        (x0 - s,   y0),        # P3: G1 Y+150 → up
    ]


def _residuals(params: np.ndarray, D: float, observed: np.ndarray) -> np.ndarray:
    x0, y0 = params
    positions = sample_positions(x0, y0)
    res = []
    for i, (px, py) in enumerate(positions):
        try:
            pred = predicted_pitch(px, py, D)
        except ValueError:
            pred = 0.0
        res.append(pred - observed[i])
    return np.array(res)


def solve_position(
    scans_raw: list[int | float],
    D_mm: float,
    x_prior_mm: float,
    y_prior_mm: float,
) -> tuple[float, float]:
    """
    Solve for (x0, y0) given four raw IMU scan values and wall geometry.

    Args:
        scans_raw: four pitch values in degrees as sent by the firmware.
                   The firmware stores pitch×10 internally but divides by 10
                   before sending (SIFileDownloader.cpp line 272), so the values
                   arrive here already in degrees.
        D_mm:      nail-to-nail distance.
        x_prior_mm, y_prior_mm: initial guess (from WallConfig or tape spec).

    Returns:
        (x0_mm, y0_mm) — best-fit Point Zero in wall-XY coordinates.
    """
    observed = np.array([float(v) for v in scans_raw])  # already degrees

    result = least_squares(
        _residuals,
        x0=np.array([x_prior_mm, y_prior_mm]),
        args=(D_mm, observed),
        method="lm",          # Levenberg-Marquardt, no bounds needed
        max_nfev=500,
    )
    x0, y0 = result.x
    return float(x0), float(y0)


def xy_to_lr(x_mm: float, y_mm: float, D_mm: float) -> tuple[float, float]:
    """Convert wall-XY to cable lengths (L, R)."""
    L = math.hypot(x_mm, y_mm)
    R = math.hypot(D_mm - x_mm, y_mm)
    return L, R


def format_g92(x_mm: float, y_mm: float, D_mm: float) -> str:
    """
    Return the G92 command string the firmware expects.

    The SAMD21 firmware (Style B) tracks position in cable-length space:
      G92 X<L> Y<R>
    where L and R are the left and right cable lengths at the robot's current
    position.
    """
    L, R = xy_to_lr(x_mm, y_mm, D_mm)
    return f"G92 X{L:.2f} Y{R:.2f}"
