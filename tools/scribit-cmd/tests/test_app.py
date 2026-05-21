from __future__ import annotations

from scribit_cmd.app import App
from scribit_cmd.http_server import DynamicGcodeStore
from scribit_cmd.settings import CurrentSettings, SharedSettings


class FakePublisher:
    def __init__(self) -> None:
        self.calls: list[tuple[str, int, str, str, str, str]] = []

    def __call__(self, host: str, port: int, user: str, pw: str, topic: str, msg: str) -> None:
        self.calls.append((host, port, user, pw, topic, msg))


def make_app(
    *,
    settings: SharedSettings | None = None,
    dynamic_gcode: DynamicGcodeStore | None = None,
    publisher: FakePublisher | None = None,
) -> tuple[App, FakePublisher, DynamicGcodeStore]:
    fake_publisher = publisher or FakePublisher()
    store = dynamic_gcode or DynamicGcodeStore()
    app = App(
        robot_id="robot-1",
        mqtt_host="mqtt.local",
        mqtt_port=1883,
        mqtt_user="user",
        mqtt_pass="pass",
        host_ip="192.0.2.10",
        http_port=80,
        suffix="G4 P0",
        settings=settings or SharedSettings(CurrentSettings(step=2.0, feed=900)),
        dynamic_gcode=store,
        publisher=fake_publisher,
    )
    fake_publisher.calls.clear()
    return app, fake_publisher, store


def test_publish_static_command_sends_status_reset_and_print() -> None:
    app, publisher, _store = make_app()

    app.publish_cmd("BOTH_IN")

    assert [call[4:] for call in publisher.calls] == [
        ("tin/robot-1/status", "{}"),
        ("tin/robot-1/reset", "N"),
        ("tin/robot-1/print", "http://192.0.2.10/g/BOTH_IN.gcode;G4 P0"),
    ]
    assert app.last == "BOTH_IN  step=2  feed=900  payload='http://192.0.2.10/g/BOTH_IN.gcode;G4 P0'"


def test_publish_pen_command_stores_dynamic_gcode() -> None:
    app, publisher, store = make_app()

    app.publish_cmd("P3_DOWN")

    assert store.get("P3_DOWN") == (
        "G21\n"
        "G90\n"
        "M17\n"
        "G1 Z233.000 F2000\n"
        "G101\n"
        "G101\n"
        "G101\n"
    )
    assert publisher.calls[-1][4:] == ("tin/robot-1/print", "http://192.0.2.10/g/P3_DOWN.gcode;G4 P0")


def test_carousel_publish_updates_known_z_position() -> None:
    settings = SharedSettings(CurrentSettings(step=10.0, feed=900))
    app, _publisher, _store = make_app(settings=settings)
    app.z_tracker.set(100.0)

    app.publish_cmd("CAR_CCW")
    app.publish_cmd("CAR_CW")

    assert app.z_tracker.get() == 100.0

