"""Unit tests for the polargraph geometry and position solver."""

import math
import pytest

from geometry import (
    WALL_CONFIGS,
    AUTOCAL_STEP_MM,
    predicted_pitch,
    sample_positions,
    solve_position,
    xy_to_lr,
    format_g92,
)


# ---------------------------------------------------------------------------
# predicted_pitch
# ---------------------------------------------------------------------------

class TestPredictedPitch:
    def test_centre_is_zero(self):
        # At the horizontal centre of the wall, L == R → pitch == 0
        D = 2000.0
        x = D / 2
        y = 800.0
        assert predicted_pitch(x, y, D) == pytest.approx(0.0, abs=1e-6)

    def test_left_of_centre_negative(self):
        # Robot left of centre → L < R → negative pitch (tilts toward left nail)
        D = 2000.0
        pitch = predicted_pitch(600.0, 800.0, D)
        assert pitch < 0.0

    def test_right_of_centre_positive(self):
        D = 2000.0
        pitch = predicted_pitch(1400.0, 800.0, D)
        assert pitch > 0.0

    def test_antisymmetric(self):
        # pitch(D-x, y, D) == -pitch(x, y, D)
        D = 2500.0
        x, y = 700.0, 900.0
        assert predicted_pitch(D - x, y, D) == pytest.approx(-predicted_pitch(x, y, D), abs=1e-9)

    def test_y_zero_raises(self):
        with pytest.raises(ValueError):
            predicted_pitch(500.0, 0.0, 2000.0)

    def test_y_negative_raises(self):
        with pytest.raises(ValueError):
            predicted_pitch(500.0, -10.0, 2000.0)

    def test_magnitude_increases_with_displacement(self):
        # Further from centre → larger |pitch|
        D = 2000.0
        y = 800.0
        p_near = abs(predicted_pitch(800.0, y, D))
        p_far = abs(predicted_pitch(400.0, y, D))
        assert p_far > p_near


# ---------------------------------------------------------------------------
# sample_positions
# ---------------------------------------------------------------------------

class TestSamplePositions:
    def test_four_positions(self):
        positions = sample_positions(700.0, 500.0)
        assert len(positions) == 4

    def test_p0_is_origin(self):
        x0, y0 = 700.0, 500.0
        positions = sample_positions(x0, y0)
        assert positions[0] == (x0, y0)

    def test_p1_down(self):
        x0, y0 = 700.0, 500.0
        _, p1 = sample_positions(x0, y0)[0], sample_positions(x0, y0)[1]
        assert p1 == (x0, y0 + AUTOCAL_STEP_MM)

    def test_p2_down_left(self):
        x0, y0 = 700.0, 500.0
        p2 = sample_positions(x0, y0)[2]
        assert p2 == (x0 - AUTOCAL_STEP_MM, y0 + AUTOCAL_STEP_MM)

    def test_p3_left(self):
        x0, y0 = 700.0, 500.0
        p3 = sample_positions(x0, y0)[3]
        assert p3 == (x0 - AUTOCAL_STEP_MM, y0)


# ---------------------------------------------------------------------------
# xy_to_lr and format_g92
# ---------------------------------------------------------------------------

class TestXyToLr:
    def test_centre(self):
        D = 2000.0
        L, R = xy_to_lr(D / 2, 800.0, D)
        assert L == pytest.approx(R, rel=1e-9)

    def test_values(self):
        L, R = xy_to_lr(500.0, 800.0, 2000.0)
        assert L == pytest.approx(math.hypot(500, 800), rel=1e-9)
        assert R == pytest.approx(math.hypot(1500, 800), rel=1e-9)

    def test_format_g92_contains_g92(self):
        s = format_g92(700.0, 500.0, 2000.0)
        assert s.startswith("G92 X")
        assert " Y" in s

    def test_format_g92_two_decimal_places(self):
        s = format_g92(700.0, 500.0, 2000.0)
        # e.g. "G92 X860.23 Y1562.05"
        parts = s.split()
        x_part = parts[1][1:]  # strip 'X'
        y_part = parts[2][1:]  # strip 'Y'
        assert len(x_part.split(".")[1]) == 2
        assert len(y_part.split(".")[1]) == 2


# ---------------------------------------------------------------------------
# solve_position — round-trip tests
# ---------------------------------------------------------------------------

def _make_scans(x0: float, y0: float, D: float) -> list[float]:
    """Synthesise perfect (noise-free) raw scan values for a known position."""
    positions = sample_positions(x0, y0)
    return [predicted_pitch(px, py, D) * 10.0 for px, py in positions]


class TestSolvePosition:
    @pytest.mark.parametrize("wall_id", [1, 3, 5, 7, 9])
    def test_round_trip_at_prior(self, wall_id: int):
        cfg = WALL_CONFIGS[wall_id]
        scans = _make_scans(cfg.x_prior_mm, cfg.y_prior_mm, cfg.D_mm)
        x_solved, y_solved = solve_position(
            scans, cfg.D_mm, cfg.x_prior_mm, cfg.y_prior_mm
        )
        assert x_solved == pytest.approx(cfg.x_prior_mm, abs=1.0)
        assert y_solved == pytest.approx(cfg.y_prior_mm, abs=1.0)

    def test_round_trip_offset_from_prior(self):
        """Solver recovers position even when robot is not exactly at prior."""
        cfg = WALL_CONFIGS[3]
        true_x = cfg.x_prior_mm + 40.0
        true_y = cfg.y_prior_mm + 30.0
        scans = _make_scans(true_x, true_y, cfg.D_mm)
        x_solved, y_solved = solve_position(
            scans, cfg.D_mm, cfg.x_prior_mm, cfg.y_prior_mm
        )
        assert x_solved == pytest.approx(true_x, abs=2.0)
        assert y_solved == pytest.approx(true_y, abs=2.0)

    def test_noisy_scans_within_tolerance(self):
        """Adding ±0.5° noise (±5 raw units) still yields <10 mm error."""
        import random
        rng = random.Random(42)
        cfg = WALL_CONFIGS[5]
        true_x, true_y = cfg.x_prior_mm, cfg.y_prior_mm
        perfect = _make_scans(true_x, true_y, cfg.D_mm)
        noisy = [v + rng.uniform(-5, 5) for v in perfect]

        x_solved, y_solved = solve_position(
            noisy, cfg.D_mm, cfg.x_prior_mm, cfg.y_prior_mm
        )
        err = math.hypot(x_solved - true_x, y_solved - true_y)
        assert err < 15.0, f"Error {err:.1f} mm exceeds 15 mm threshold"

    def test_all_wall_configs_have_positive_prior_y(self):
        for wid, cfg in WALL_CONFIGS.items():
            assert cfg.y_prior_mm > 0, f"wallId {wid} has non-positive y_prior"

    def test_all_wall_configs_prior_x_inside_wall(self):
        for wid, cfg in WALL_CONFIGS.items():
            assert 0 < cfg.x_prior_mm < cfg.D_mm, (
                f"wallId {wid} x_prior {cfg.x_prior_mm} outside wall [0, {cfg.D_mm}]"
            )
