from __future__ import annotations

import pytest

from scribit_cmd.gcode import Key, ZTracker, build_g77_gcode, build_manual_move_payload, build_pen_gcode, build_static_gcode


def test_motor_jog_uses_firmware_axis_mapping() -> None:
    assert build_static_gcode(Key(step=2.0, feed=900), "LEFTISH") == (
        "G21\n"
        "G91\n"
        "M17\n"
        "G1 X-2.000 Y-2.000 F900\n"
    )


def test_carousel_commands_are_relative_z_moves() -> None:
    assert build_static_gcode(Key(step=5.0, feed=1200), "CAR_CW") == (
        "G21\n"
        "G91\n"
        "M17\n"
        "G1 Z-5.000 F1200\n"
    )


def test_unknown_static_command_raises_value_error() -> None:
    with pytest.raises(ValueError, match="unknown cmd NOPE"):
        build_static_gcode(Key(step=1.0, feed=900), "NOPE")


def test_manual_move_payload_uses_semicolon_line_delimiters() -> None:
    assert build_manual_move_payload("G21\n\nG91\nM17\n") == "G21;G91;M17"


def test_pen_gcode_tracks_ccw_only_absolute_targets() -> None:
    tracker = ZTracker(z=305.0)

    gcode = build_pen_gcode(tracker, pen=1, is_down=True, fz=2000)

    assert gcode == (
        "G21\n"
        "G90\n"
        "M17\n"
        "G1 Z449.000 F2000\n"
        "G101\n"
        "G101\n"
        "G101\n"
    )
    assert tracker.get() == 449.0


def test_pen_up_does_not_emit_g101() -> None:
    tracker = ZTracker()

    gcode = build_pen_gcode(tracker, pen=2, is_down=False)

    assert gcode == "G21\nG90\nM17\nG1 Z161.000 F2000\n"
    assert tracker.get() == 161.0


def test_g77_sets_tracker_to_home_offset() -> None:
    tracker = ZTracker(z=120.0)

    assert build_g77_gcode(tracker) == "G21\nG90\nM17\nG77\nG92 Z-56\n"
    assert tracker.get() == -56.0
