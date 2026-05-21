# Scribit Command Tools

Python CLI utilities for commanding a Scribit robot over MQTT while serving
GCODE over HTTP.

The firmware expects print payloads in this form:

```text
http://<host-ip>/<path>;<suffix>
```

Current firmware also rejects URLs that include `:port`, so the HTTP server must
bind to port `80`. On most systems that means running the command with `sudo`.

## Setup

From this directory:

```bash
uv sync
```

Run commands with:

```bash
uv run python main.py --help
```

## Commands

### Interactive jog UI

Starts a curses UI for motor-space jogs, carousel movement, pen selection, and
stop/reset commands. It also serves dynamic GCODE endpoints at `/g/<CMD>.gcode`.

```bash
sudo uv run python main.py interactive \
  --robot-id 30aea4da06f4 \
  --mqtt-host 192.168.1.200 \
  --mqtt-user scribit \
  --mqtt-pass scribit \
  --host-ip 192.168.1.202 \
  --http-port 80 \
  --feed 900 \
  --step 2.0
```

Common options:

- `--robot-id`: robot id used in `tin/<robot-id>/...` MQTT topics
- `--mqtt-host`: MQTT broker host or IP
- `--host-ip`: this computer's IP address as reachable by the robot
- `--mqtt-port`: MQTT broker port, default `1883`
- `--mqtt-user` / `--mqtt-pass`: MQTT credentials, default `scribit`
- `--suffix`: print payload suffix, default `G4 P0`
- `--step`: initial cable jog distance / carousel degrees, default `2.0`
- `--feed`: initial feed rate, default `900`

Interactive controls:

```text
Arrow keys / WASD : Up=both in, Down=both out, Left=leftish, Right=rightish
Q / E             : left in/out
Z / C             : right in/out
J / K             : carousel CCW/CW by step
1 / 2 / 3 / 4     : pen 1/2/3/4 down
! / @ / # / $     : pen 1/2/3/4 up
H                 : G77 home Z-cylinder / carousel
[ / ]             : step down/up
- / =             : feed down/up
x                 : reset N / stop
ESC / Ctrl+G      : quit
```

### Draw a GCODE file

Serves one local GCODE file and publishes a print command to the robot.

```bash
sudo uv run python main.py draw path/to/file.gcode \
  --robot-id 30aea4d9fc5c \
  --mqtt-host 192.168.240.2 \
  --host-ip 192.168.240.2 \
  --http-port 80
```

The default suffix for `draw` is `M18`, so motors are released when the job
stops. Use `--suffix` to override that behavior.

Useful draw options:

- `--wait-download`: seconds to wait for the robot to request the file, default `60`
- `--keep-alive`: seconds to keep serving after the first download, default `5`

## Development

The executable entry point is intentionally small:

- `main.py`: invokes the Typer app
- `scribit_cmd/cli.py`: Typer command definitions
- `scribit_cmd/app.py`: command orchestration and MQTT publish flow
- `scribit_cmd/gcode.py`: GCODE generation and carousel Z tracking
- `scribit_cmd/http_server.py`: HTTP handlers for dynamic commands and files
- `scribit_cmd/mqtt_client.py`: MQTT publish wrapper
- `scribit_cmd/settings.py`: shared step/feed state
- `scribit_cmd/tui.py`: curses UI

Run tests from this directory:

```bash
uv run pytest
```
