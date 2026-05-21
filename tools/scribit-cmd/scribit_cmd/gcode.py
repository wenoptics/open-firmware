from __future__ import annotations

import threading
from dataclasses import dataclass


# Scribit open-firmware convention:
#   X = dL
#   Y = -dR
# So for desired right delta +dR, emit Y = -dR.
MOTOR_CMDS: dict[str, tuple[int, int]] = {
    "BOTH_IN": (-1, -1),
    "BOTH_OUT": (+1, +1),
    "L_IN": (-1, 0),
    "L_OUT": (+1, 0),
    "R_IN": (0, -1),
    "R_OUT": (0, +1),
    "LEFTISH": (-1, +1),
    "RIGHTISH": (+1, -1),
}

CAROUSEL_CMDS = {"CAR_CCW", "CAR_CW"}

PEN_SLOT_Z = {
    1: 89,
    2: 161,
    3: 233,
    4: 305,
}

PEN_CMDS = {
    "P1_UP",
    "P2_UP",
    "P3_UP",
    "P4_UP",
    "P1_DOWN",
    "P2_DOWN",
    "P3_DOWN",
    "P4_DOWN",
}

SPECIAL_CMDS = {"G77"} | CAROUSEL_CMDS | PEN_CMDS


@dataclass(frozen=True)
class Key:
    step: float
    feed: int


class Cache:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._cache: dict[tuple[Key, str], str] = {}

    def get_gcode(self, key: Key, cmd: str) -> str:
        cache_key = (key, cmd)
        with self._lock:
            if cache_key not in self._cache:
                self._cache[cache_key] = build_static_gcode(key, cmd)
            return self._cache[cache_key]


def build_static_gcode(key: Key, cmd: str) -> str:
    if cmd in MOTOR_CMDS:
        sx, sy = MOTOR_CMDS[cmd]
        d_l = sx * key.step
        d_r = sy * key.step
        return "\n".join(
            [
                "G21",
                "G91",
                "M17",
                f"G1 X{d_l:.3f} Y{-d_r:.3f} F{int(key.feed)}",
            ]
        ) + "\n"

    if cmd in CAROUSEL_CMDS:
        dz = key.step if cmd == "CAR_CCW" else -key.step
        return "\n".join(
            [
                "G21",
                "G91",
                "M17",
                f"G1 Z{dz:.3f} F{int(key.feed)}",
            ]
        ) + "\n"

    if cmd == "G77":
        return build_g77_gcode()

    raise ValueError(f"unknown cmd {cmd}")


class ZTracker:
    def __init__(self, z: float | None = None) -> None:
        self._lock = threading.Lock()
        self._z = z

    def get(self) -> float | None:
        with self._lock:
            return self._z

    def set(self, z: float | None) -> None:
        with self._lock:
            self._z = z

    def add_if_known(self, dz: float) -> None:
        with self._lock:
            if self._z is not None:
                self._z += dz

    def ccw_only_target(self, slot_z: float) -> float:
        current_z = self.get()
        if current_z is None:
            return float(slot_z)

        target = float(slot_z)
        while target < current_z:
            target += 360.0
        return target


def build_pen_gcode(tracker: ZTracker, pen: int, is_down: bool, fz: int = 2000) -> str:
    slot = PEN_SLOT_Z[pen]
    target = tracker.ccw_only_target(slot)
    lines = [
        "G21",
        "G90",
        "M17",
        f"G1 Z{target:.3f} F{int(fz)}",
    ]
    if is_down:
        lines += ["G101", "G101", "G101"]
    tracker.set(target)
    return "\n".join(lines) + "\n"


def build_g77_gcode(tracker: ZTracker | None = None) -> str:
    if tracker is not None:
        tracker.set(-56.0)
    return "\n".join(
        [
            "G21",
            "G90",
            "M17",
            "G77",
            "G92 Z-56",
        ]
    ) + "\n"

