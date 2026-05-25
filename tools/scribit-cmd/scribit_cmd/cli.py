from __future__ import annotations

import math
import threading
import time
from pathlib import Path
from typing import Annotated
from urllib.parse import quote

import typer

from .app import App
from .http_server import FileHandler, start_http_server
from .mqtt_client import mqtt_pub
from .tui import run_curses

cli = typer.Typer(
    add_completion=False,
    context_settings={"help_option_names": ["-h", "--help"]},
    help="Scribit command line tools. Serves GCODE over HTTP and commands the robot over MQTT.",
)


def validate_firmware_http_port(http_port: int) -> None:
    if http_port != 80:
        typer.secho(
            "ERROR: Your firmware rejects URLs with ':port'. Use --http-port 80 and run with sudo.",
            fg=typer.colors.RED,
            err=True,
        )
        raise typer.Exit(2)


@cli.command()
def interactive(
    robot_id: Annotated[
        str,
        typer.Option("--robot-id", help="Scribit robot id used in tin/<robot-id>/... MQTT topics."),
    ],
    mqtt_host: Annotated[
        str,
        typer.Option("--mqtt-host", help="MQTT broker host or IP address."),
    ],
    host_ip: Annotated[
        str,
        typer.Option("--host-ip", help="Unused by interactive manualMove mode; kept for compatibility."),
    ] = "127.0.0.1",
    mqtt_port: Annotated[int, typer.Option("--mqtt-port", min=1, max=65535, help="MQTT broker port.")] = 1883,
    mqtt_user: Annotated[
        str,
        typer.Option("--mqtt-user", help="MQTT username. Use an empty string to disable auth."),
    ] = "scribit",
    mqtt_pass: Annotated[
        str,
        typer.Option("--mqtt-pass", help="MQTT password. Use an empty string to disable auth."),
    ] = "scribit",
    http_port: Annotated[
        int,
        typer.Option(
            "--http-port",
            min=1,
            max=65535,
            help="Unused by interactive manualMove mode; kept for compatibility.",
        ),
    ] = 80,
    suffix: Annotated[
        str,
        typer.Option("--suffix", help="Unused by interactive manualMove mode; kept for compatibility."),
    ] = "G4 P0",
    step: Annotated[
        float,
        typer.Option("--step", min=0.001, help="Initial jog step in mm for cables and degrees for carousel."),
    ] = 2.0,
    feed: Annotated[int, typer.Option("--feed", min=1, help="Initial GCODE feed rate.")] = 900,
) -> None:
    app = App(
        robot_id=robot_id,
        mqtt_host=mqtt_host,
        mqtt_port=mqtt_port,
        mqtt_user=mqtt_user,
        mqtt_pass=mqtt_pass,
        host_ip=host_ip,
        http_port=http_port,
        suffix=suffix,
    )

    typer.echo(f"[scribit_jog_cli] MQTT broker {mqtt_host}:{mqtt_port}  robot_id={robot_id}  command=manualMove")

    try:
        run_curses(app, step0=step, feed0=feed)
    except KeyboardInterrupt:
        pass


@cli.command()
def draw(
    gcode_path: Annotated[
        Path,
        typer.Argument(
            exists=True,
            file_okay=True,
            dir_okay=False,
            readable=True,
            resolve_path=True,
            help="Path to the GCODE file the robot should download and draw.",
        ),
    ],
    robot_id: Annotated[
        str,
        typer.Option("--robot-id", help="Scribit robot id used in tin/<robot-id>/... MQTT topics."),
    ],
    mqtt_host: Annotated[
        str,
        typer.Option("--mqtt-host", help="MQTT broker host or IP address."),
    ],
    host_ip: Annotated[
        str,
        typer.Option("--host-ip", help="This computer's IP address as reachable by the robot."),
    ],
    mqtt_port: Annotated[int, typer.Option("--mqtt-port", min=1, max=65535, help="MQTT broker port.")] = 1883,
    mqtt_user: Annotated[
        str,
        typer.Option("--mqtt-user", help="MQTT username. Use an empty string to disable auth."),
    ] = "scribit",
    mqtt_pass: Annotated[
        str,
        typer.Option("--mqtt-pass", help="MQTT password. Use an empty string to disable auth."),
    ] = "scribit",
    http_port: Annotated[
        int,
        typer.Option(
            "--http-port",
            min=1,
            max=65535,
            help="HTTP bind port. Current firmware requires 80 because URLs cannot include ':port'.",
        ),
    ] = 80,
    suffix: Annotated[
        str,
        typer.Option("--suffix", help="Required MQTT print payload suffix appended after the URL."),
    ] = "M18",
    wait_download: Annotated[
        float,
        typer.Option("--wait-download", min=0.0, help="Seconds to wait for the robot to request the GCODE file."),
    ] = 60.0,
    keep_alive: Annotated[
        float,
        typer.Option("--keep-alive", min=0.0, help="Seconds to keep serving after the first successful download."),
    ] = 5.0,
) -> None:
    validate_firmware_http_port(http_port)

    filename = quote(gcode_path.name)
    url_path = f"/{filename}"
    url = f"http://{host_ip}{url_path}"
    payload = f"{url};{suffix}"
    downloaded = threading.Event()

    FileHandler.gcode_path = gcode_path
    FileHandler.url_path = f"/{gcode_path.name}"
    FileHandler.downloaded = downloaded
    httpd = start_http_server(http_port, FileHandler)

    # Make Scribit ready (leave BOOT, go IDLE)
    mqtt_pub(mqtt_host, mqtt_port, mqtt_user, mqtt_pass, f"tin/{robot_id}/status", "{}")
    time.sleep(0.02)

    topic = f"tin/{robot_id}/print"
    typer.echo(f"[scribit_cmd] HTTP listening on 0.0.0.0:{http_port} (serving {gcode_path})")
    typer.echo(f"[scribit_cmd] MQTT broker {mqtt_host}:{mqtt_port}  topic={topic}")
    typer.echo(f"[scribit_cmd] print payload: {payload}")

    try:
        mqtt_pub(mqtt_host, mqtt_port, mqtt_user, mqtt_pass, topic, payload)
        if wait_download > 0:
            if downloaded.wait(timeout=wait_download):
                typer.echo("[scribit_cmd] robot downloaded the GCODE file")
                if keep_alive > 0:
                    time.sleep(keep_alive)
            else:
                typer.secho(
                    f"[scribit_cmd] timed out waiting {wait_download:g}s for robot download; stopping HTTP server",
                    fg=typer.colors.YELLOW,
                    err=True,
                )
    finally:
        httpd.shutdown()


@cli.command()
def calibrate(
    left_mm: Annotated[float, typer.Option("--left", help="Left cable length in mm (nail centre to robot centre).")],
    right_mm: Annotated[float, typer.Option("--right", help="Right cable length in mm (nail centre to robot centre).")],
    robot_id: Annotated[str, typer.Option("--robot-id", help="Scribit robot id used in tin/<robot-id>/... MQTT topics.")],
    mqtt_host: Annotated[str, typer.Option("--mqtt-host", help="MQTT broker host or IP address.")],
    nail_spacing_mm: Annotated[float, typer.Option("--nail-spacing", help="Horizontal distance between the two nails in mm.")] = 1860.0,
    mqtt_port: Annotated[int, typer.Option("--mqtt-port", min=1, max=65535, help="MQTT broker port.")] = 1883,
    mqtt_user: Annotated[str, typer.Option("--mqtt-user", help="MQTT username.")] = "scribit",
    mqtt_pass: Annotated[str, typer.Option("--mqtt-pass", help="MQTT password.")] = "scribit",
) -> None:
    """Set robot position from measured cable lengths. No IMU or cloud service required.

    Measure the left and right cable lengths with a tape measure (nail centre to
    robot centre), then run this command. The robot's absolute wall position is
    computed from the polargraph geometry and injected via a G92 command.
    """
    D, L, R = nail_spacing_mm, left_mm, right_mm
    x = (L**2 - R**2 + D**2) / (2 * D)
    y = math.sqrt(max(0.0, L**2 - x**2))
    payload = f"G92 X{x:.2f} Y{y:.2f}"

    typer.echo(f"[scribit_cmd] Computed position: X={x:.1f} mm, Y={y:.1f} mm")
    typer.echo(f"[scribit_cmd] Sending manualMove payload: {payload}")
    mqtt_pub(mqtt_host, mqtt_port, mqtt_user, mqtt_pass, f"tin/{robot_id}/manualMove", payload)
    typer.echo("[scribit_cmd] Done.")


def main() -> None:
    cli()
