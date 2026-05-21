from __future__ import annotations

import threading
from dataclasses import dataclass

from .gcode import Key


@dataclass
class CurrentSettings:
    step: float = 2.0
    feed: int = 900


class SharedSettings:
    def __init__(self, initial: CurrentSettings | None = None) -> None:
        self._lock = threading.Lock()
        self._current = initial or CurrentSettings()

    def set(self, *, step: float | None = None, feed: int | None = None) -> None:
        with self._lock:
            if step is not None:
                self._current.step = step
            if feed is not None:
                self._current.feed = feed

    def snapshot(self) -> CurrentSettings:
        with self._lock:
            return CurrentSettings(step=self._current.step, feed=self._current.feed)

    def key(self) -> Key:
        current = self.snapshot()
        return Key(step=float(current.step), feed=int(current.feed))


SETTINGS = SharedSettings()

