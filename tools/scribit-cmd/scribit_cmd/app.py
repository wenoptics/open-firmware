from __future__ import annotations

import threading
import time
from collections.abc import Callable

from .gcode import CAROUSEL_CMDS, PEN_CMDS, ZTracker, build_g77_gcode, build_manual_move_payload, build_pen_gcode
from .http_server import CACHE, DYNAMIC_GCODE, DynamicGcodeStore
from .mqtt_client import mqtt_pub
from .settings import SETTINGS, SharedSettings

MqttPublisher = Callable[[str, int, str, str, str, str], None]


class App:
    def __init__(
        self,
        robot_id: str,
        mqtt_host: str,
        mqtt_port: int,
        mqtt_user: str,
        mqtt_pass: str,
        host_ip: str,
        http_port: int,
        suffix: str,
        *,
        settings: SharedSettings = SETTINGS,
        dynamic_gcode: DynamicGcodeStore = DYNAMIC_GCODE,
        publisher: MqttPublisher = mqtt_pub,
    ) -> None:
        self.robot_id = robot_id
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.mqtt_user = mqtt_user
        self.mqtt_pass = mqtt_pass
        self.host_ip = host_ip
        self.http_port = http_port
        self.suffix = suffix
        self.settings = settings
        self.dynamic_gcode = dynamic_gcode
        self.publisher = publisher

        self.last = ""
        self.last_lock = threading.Lock()
        self.z_tracker = ZTracker()

        try:
            self.ensure_idle()
            time.sleep(0.02)
        except Exception:
            pass

    def topic(self, suffix: str) -> str:
        return f"tin/{self.robot_id}/{suffix}"

    def publish_mqtt(self, suffix: str, payload: str) -> None:
        self.publisher(
            self.mqtt_host,
            self.mqtt_port,
            self.mqtt_user,
            self.mqtt_pass,
            self.topic(suffix),
            payload,
        )

    def ensure_idle(self) -> None:
        self.publish_mqtt("status", "{}")

    def reset_n(self) -> None:
        self.publish_mqtt("reset", "N")
        with self.last_lock:
            self.last = "RESET N"

    def publish_calibration(self, wall_id: int, g1_cmd: str) -> None:
        payload = f"{g1_cmd};{wall_id}"
        self.publish_mqtt("calibration", payload)
        with self.last_lock:
            self.last = f"calibration  wall={wall_id}  payload='{payload}'"

    def build_pen_gcode(self, pen: int, is_down: bool, fz: int = 2000) -> str:
        return build_pen_gcode(self.z_tracker, pen=pen, is_down=is_down, fz=fz)

    def build_g77_gcode(self) -> str:
        return build_g77_gcode(self.z_tracker)

    def build_cmd_gcode(self, cmd: str) -> str:
        key = self.settings.key()

        if cmd in PEN_CMDS:
            pen = int(cmd[1])
            is_down = cmd.endswith("DOWN")
            return self.build_pen_gcode(pen=pen, is_down=is_down, fz=2000)
        if cmd == "G77":
            return self.build_g77_gcode()

        gcode = CACHE.get_gcode(key, cmd)
        if cmd in CAROUSEL_CMDS:
            dz = key.step if cmd == "CAR_CCW" else -key.step
            self.z_tracker.add_if_known(dz)
        return gcode

    def publish_manual_cmd(self, cmd: str) -> None:
        key = self.settings.key()
        gcode = self.build_cmd_gcode(cmd)
        payload = build_manual_move_payload(gcode)

        self.ensure_idle()
        time.sleep(0.02)
        self.publish_mqtt("manualMove", payload)

        with self.last_lock:
            self.last = f"{cmd}  step={key.step:g}  feed={key.feed}  manualMove='{payload}'"

    def publish_cmd(self, cmd: str) -> None:
        key = self.settings.key()

        if cmd in PEN_CMDS:
            pen = int(cmd[1])
            is_down = cmd.endswith("DOWN")
            self.dynamic_gcode.set(cmd, self.build_pen_gcode(pen=pen, is_down=is_down, fz=2000))
        elif cmd == "G77":
            self.dynamic_gcode.set(cmd, self.build_g77_gcode())
        else:
            CACHE.get_gcode(key, cmd)
            if cmd in CAROUSEL_CMDS:
                dz = key.step if cmd == "CAR_CCW" else -key.step
                self.z_tracker.add_if_known(dz)

        self.ensure_idle()
        time.sleep(0.02)

        url = f"http://{self.host_ip}/g/{cmd}.gcode"
        payload = f"{url};{self.suffix}"

        self.reset_n()
        time.sleep(0.08)
        self.publish_mqtt("print", payload)

        with self.last_lock:
            self.last = f"{cmd}  step={key.step:g}  feed={key.feed}  payload='{payload}'"
