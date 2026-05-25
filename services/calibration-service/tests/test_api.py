"""Integration tests for the FastAPI calibration service."""

import math
import re

import pytest
from fastapi.testclient import TestClient

from geometry import (
    WALL_CONFIGS,
    NUDGE_THRESHOLD_MM,
    predicted_pitch,
    sample_positions,
    xy_to_lr,
)
from main import app

client = TestClient(app)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_scans(x0: float, y0: float, D: float) -> list[float]:
    positions = sample_positions(x0, y0)
    return [predicted_pitch(px, py, D) * 10.0 for px, py in positions]


def _parse_g92_from_body(body: str) -> tuple[float, float]:
    """Extract X and Y values from a G92 body line like '"G92 X860.23 Y1562.05"'."""
    m = re.search(r"G92 X([\d.]+) Y([\d.]+)", body)
    assert m, f"No G92 X... Y... found in: {body!r}"
    return float(m.group(1)), float(m.group(2))


# ---------------------------------------------------------------------------
# /health
# ---------------------------------------------------------------------------

def test_health():
    r = client.get("/health")
    assert r.status_code == 200
    assert r.json()["status"] == "ok"


# ---------------------------------------------------------------------------
# /autocal.GCODE
# ---------------------------------------------------------------------------

def test_autocal_gcode_served():
    r = client.get("/autocal.GCODE")
    assert r.status_code == 200
    assert "M777" in r.text
    assert "G91" in r.text


def test_autocal_gcode_has_four_m777():
    r = client.get("/autocal.GCODE")
    assert r.text.count("M777") == 4


# ---------------------------------------------------------------------------
# POST /calibrate — happy path
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("wall_id", list(WALL_CONFIGS.keys()))
def test_calibrate_all_wall_ids(wall_id: int):
    cfg = WALL_CONFIGS[wall_id]
    scans = _make_scans(cfg.x_prior_mm, cfg.y_prior_mm, cfg.D_mm)
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": wall_id, "scans": scans})
    assert r.status_code == 200
    assert "G92" in r.text


def test_calibrate_response_format_matches_firmware_parser():
    """
    The firmware scans: indexOf("G92") … indexOf('"', cmdStart)
    So the body must contain the pattern: "G92 X... Y..."
    """
    cfg = WALL_CONFIGS[1]
    scans = _make_scans(cfg.x_prior_mm, cfg.y_prior_mm, cfg.D_mm)
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 1, "scans": scans})
    assert r.status_code == 200
    body = r.text
    # Must open with a quote, contain G92, and have a closing quote after the command
    assert '"' in body
    g92_pos = body.index("G92")
    closing_quote = body.index('"', g92_pos)
    assert closing_quote > g92_pos


def test_calibrate_round_trip_cable_lengths():
    """
    Solved position → cable lengths should match the perfect forward-model values.
    """
    cfg = WALL_CONFIGS[5]
    true_x, true_y = cfg.x_prior_mm, cfg.y_prior_mm
    scans = _make_scans(true_x, true_y, cfg.D_mm)
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 5, "scans": scans})
    assert r.status_code == 200

    L_solved, R_solved = _parse_g92_from_body(r.text)
    L_true, R_true = xy_to_lr(true_x, true_y, cfg.D_mm)

    assert L_solved == pytest.approx(L_true, abs=5.0)
    assert R_solved == pytest.approx(R_true, abs=5.0)


def test_calibrate_no_nudge_when_at_prior():
    """Response should NOT contain G1 when robot is at the expected prior."""
    cfg = WALL_CONFIGS[3]
    scans = _make_scans(cfg.x_prior_mm, cfg.y_prior_mm, cfg.D_mm)
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 3, "scans": scans})
    assert r.status_code == 200
    assert "G1" not in r.text


def test_calibrate_nudge_when_far_from_prior():
    """
    If the robot is placed far from Point Zero, the service should include G1
    in the response so the firmware retries.
    """
    cfg = WALL_CONFIGS[3]
    # Place robot 150 mm away from prior (well beyond NUDGE_THRESHOLD_MM)
    far_x = cfg.x_prior_mm + NUDGE_THRESHOLD_MM + 50
    far_y = cfg.y_prior_mm + NUDGE_THRESHOLD_MM + 50
    scans = _make_scans(far_x, far_y, cfg.D_mm)
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 3, "scans": scans})
    assert r.status_code == 200
    assert "G1" in r.text


# ---------------------------------------------------------------------------
# POST /calibrate — validation errors
# ---------------------------------------------------------------------------

def test_calibrate_unknown_wall_id():
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 99, "scans": [0, 0, 0, 0]})
    assert r.status_code == 422


def test_calibrate_wrong_scan_count():
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 1, "scans": [0, 0, 0]})
    assert r.status_code == 422


def test_calibrate_too_many_scans():
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "wallId": 1, "scans": [0, 0, 0, 0, 0]})
    assert r.status_code == 422


def test_calibrate_missing_sn():
    cfg = WALL_CONFIGS[1]
    scans = _make_scans(cfg.x_prior_mm, cfg.y_prior_mm, cfg.D_mm)
    r = client.post("/calibrate", json={"wallId": 1, "scans": scans})
    assert r.status_code == 422


def test_calibrate_missing_wall_id():
    cfg = WALL_CONFIGS[1]
    scans = _make_scans(cfg.x_prior_mm, cfg.y_prior_mm, cfg.D_mm)
    r = client.post("/calibrate", json={"sn": "aabbccddeeff", "scans": scans})
    assert r.status_code == 422
