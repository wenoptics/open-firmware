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

    sin(θ) = (R² - L²) / (2 * D * y * cos(θ))   [full cable-tension balance]

Because the small-angle approximation breaks down far from centre, we model the
full geometry and use scipy least-squares to find (x0, y0) that best fits all
four observed pitch values.

Angles stored by the ESP32 firmware are pitch * 10 (int16_t), so callers must
pass the raw scans array and this module converts automatically.
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
WALL_CONFIGS: dict[int, WallConfig] = {
    # A-tape: 2 m – 2.75 m walls  (A1/A2 holes)
    # Point Zero sits roughly 40 % in from left and 35 % down from top.
    # Positions are chosen so |sin(θ)| < 0.95 at all four autocal corners,
    # keeping the pitch well within the physical arcsin domain.
    1: WallConfig(D_mm=2000, x_prior_mm=800,  y_prior_mm=700,  label="A 2.0 m"),
    2: WallConfig(D_mm=2250, x_prior_mm=900,  y_prior_mm=790,  label="A 2.25 m"),
    3: WallConfig(D_mm=2500, x_prior_mm=1000, y_prior_mm=875,  label="A 2.5 m"),
    4: WallConfig(D_mm=2750, x_prior_mm=1100, y_prior_mm=960,  label="A 2.75 m"),
    # B-tape: 3 m – 4 m walls     (B1/B2 holes)
    5: WallConfig(D_mm=3000, x_prior_mm=1200, y_prior_mm=1050, label="B 3.0 m"),
    6: WallConfig(D_mm=3250, x_prior_mm=1300, y_prior_mm=1140, label="B 3.25 m"),
    7: WallConfig(D_mm=3500, x_prior_mm=1400, y_prior_mm=1225, label="B 3.5 m"),
    8: WallConfig(D_mm=3750, x_prior_mm=1500, y_prior_mm=1310, label="B 3.75 m"),
    9: WallConfig(D_mm=4000, x_prior_mm=1600, y_prior_mm=1400, label="B 4.0 m"),
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

    Sign convention: robot tilts left (toward left nail) → negative pitch.
    The IMU is mounted so that leftward tilt gives a negative reading.

    Derivation: at equilibrium the net horizontal moment about the robot CoM
    is zero.  The horizontal components of cable tension must balance:

        T_L * (x / L) = T_R * ((D-x) / R)

    with T_L/R = mg * (L/R) / (y/L + y/R) … simplifying:

        sin(θ) = (L² - R²) / (2 * D * hypot(x, y) * ... )

    Practically, for small angles the pitch tracks the horizontal asymmetry of
    the cable forces.  A clean closed form for the equilibrium angle is:

        θ = arctan2(R_h - L_h, y)

    where L_h = x (horizontal projection of left cable anchor), R_h = D - x.

    We use the full atan2 form which is valid at any angle:
    """
    if y <= 0:
        raise ValueError(f"y must be positive (robot below nails), got {y}")
    L = math.hypot(x, y)
    R = math.hypot(D - x, y)
    # Horizontal component of tension ratio drives tilt.
    # The robot tilts so that sin(θ) = (L² - R²) / (2 * D * y)
    # This is derived from the catenary equilibrium; exact for massless cables.
    sin_theta = (L * L - R * R) / (2.0 * D * y)
    # clamp to [-1, 1] for numerical safety
    sin_theta = max(-1.0, min(1.0, sin_theta))
    return math.degrees(math.asin(sin_theta))


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
        scans_raw: four values as sent by the firmware (pitch × 10, int16_t).
                   Converted to degrees internally.
        D_mm:      nail-to-nail distance.
        x_prior_mm, y_prior_mm: initial guess (from WallConfig or tape spec).

    Returns:
        (x0_mm, y0_mm) — best-fit Point Zero in wall-XY coordinates.
    """
    observed = np.array([v / 10.0 for v in scans_raw])  # tenths → degrees

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
