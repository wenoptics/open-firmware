from __future__ import annotations

import threading
from datetime import datetime
from typing import ClassVar

import paho.mqtt.client as mqtt
from textual import on
from textual.app import App as TextualApp
from textual.app import ComposeResult
from textual.binding import Binding
from textual.containers import Container, Horizontal, ScrollableContainer, Vertical
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Button, Footer, Header, Input, Label, RichLog, Static

from .app import App
from .settings import SETTINGS

# ─── constants ────────────────────────────────────────────────────────────────
STEP_CHOICES = [0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 50.0, 72.0, 90.0, 100.0, 200.0]
FEED_CHOICES = [600, 900, 1200, 1800, 2400, 3000]

BUTTON_GROUPS: list[tuple[str, list[tuple[str, str, str]]]] = [
    (
        "Cable Drive",
        [
            ("↑ Both IN", "BOTH_IN", "arrow_up"),
            ("↓ Both OUT", "BOTH_OUT", "arrow_down"),
            ("← Leftish", "LEFTISH", "arrow_left"),
            ("→ Rightish", "RIGHTISH", "arrow_right"),
            ("Q  Left IN", "L_IN", "q_key"),
            ("E  Left OUT", "L_OUT", "e_key"),
            ("Z  Right IN", "R_IN", "z_key"),
            ("C  Right OUT", "R_OUT", "c_key"),
        ],
    ),
    (
        "Carousel",
        [
            ("J  CCW", "CAR_CCW", "j_key"),
            ("K  CW", "CAR_CW", "k_key"),
            ("H  Home (G77)", "G77", "h_key"),
        ],
    ),
    (
        "Pen DOWN  (1-4)",
        [
            ("1  Pen 1 ↓", "P1_DOWN", "p1d"),
            ("2  Pen 2 ↓", "P2_DOWN", "p2d"),
            ("3  Pen 3 ↓", "P3_DOWN", "p3d"),
            ("4  Pen 4 ↓", "P4_DOWN", "p4d"),
        ],
    ),
    (
        "Pen UP  (!@#$)",
        [
            ("!  Pen 1 ↑", "P1_UP", "p1u"),
            ("@  Pen 2 ↑", "P2_UP", "p2u"),
            ("#  Pen 3 ↑", "P3_UP", "p3u"),
            ("$  Pen 4 ↑", "P4_UP", "p4u"),
        ],
    ),
]


# ─── MQTT subscriber for the log panel ───────────────────────────────────────

class MqttLogSubscriber:
    """Persistent MQTT client subscribing to tin/<robot_id>/# and tout/<robot_id>/#."""

    def __init__(self, host: str, port: int, user: str, pw: str, robot_id: str, callback) -> None:
        self._host = host
        self._port = port
        self._cb = callback
        self._in_topic = f"tin/{robot_id}/#"
        self._out_topic = f"tout/{robot_id}/#"
        self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        if user or pw:
            self._client.username_pw_set(user, pw)
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message
        self._client.on_disconnect = self._on_disconnect
        self._thread = threading.Thread(target=self._run, daemon=True, name="mqtt-log")
        self._thread.start()

    def _run(self) -> None:
        try:
            self._client.connect(self._host, self._port, keepalive=30)
        except Exception as exc:
            self._cb(f"[red]MQTT subscriber connect error: {exc}[/]")
            return
        self._client.loop_forever()

    def _on_connect(self, client, _userdata, _flags, reason_code, _properties) -> None:
        if reason_code == 0:
            self._cb("[green]Connected to MQTT broker[/]")
            client.subscribe([(self._in_topic, 0), (self._out_topic, 0)])
            self._cb(f"[dim]Subscribed to {self._in_topic} and {self._out_topic}[/]")
        else:
            self._cb(f"[red]MQTT connect refused: {reason_code}[/]")

    def _on_message(self, _client, _userdata, message) -> None:
        topic = message.topic
        payload = message.payload.decode(errors="replace")
        # Highlight calibration-related outbound messages distinctly
        if "/calibrating" in topic:
            self._cb(f"[bold cyan]{topic}[/]  [bold white]{payload}[/]", level="calibrating")
        elif "/error" in topic:
            self._cb(f"[bold red]{topic}[/]  [red]{payload}[/]", level="error")
        else:
            self._cb(f"[cyan]{topic}[/]  [white]{payload}[/]", level="info")

    def _on_disconnect(self, _client, _userdata, _disconnect_flags, reason_code, _properties) -> None:
        self._cb(f"[yellow]MQTT subscriber disconnected (rc={reason_code})[/]", level="warn")

    def stop(self) -> None:
        self._client.disconnect()


# ─── Widgets ──────────────────────────────────────────────────────────────────

class StatusBar(Static):
    """Shows MQTT connection info and current step/feed."""

    step: reactive[float] = reactive(2.0)
    feed: reactive[int] = reactive(900)
    last_cmd: reactive[str] = reactive("")

    def __init__(self, app_ref: App, **kwargs) -> None:
        super().__init__(**kwargs)
        self._app_ref = app_ref

    def render(self) -> str:
        broker = f"{self._app_ref.mqtt_host}:{self._app_ref.mqtt_port}"
        rid = self._app_ref.robot_id
        return (
            f"[bold]{broker}[/]  robot=[cyan]{rid}[/]  "
            f"step=[yellow]{self.step:g}[/]  feed=[yellow]{self.feed}[/]  "
            f"last=[dim]{self.last_cmd or '—'}[/]"
        )


class CommandButton(Button):
    """A jog button that carries the scribit command name."""

    cmd: ClassVar[str]

    def __init__(self, label: str, cmd: str, btn_id: str, **kwargs) -> None:
        super().__init__(label, id=btn_id, **kwargs)
        self._cmd = cmd

    @property
    def scribit_cmd(self) -> str:
        return self._cmd


class CommandGroup(Widget):
    """Labelled group of CommandButtons."""

    DEFAULT_CSS = """
    CommandGroup {
        border: round $accent-darken-2;
        padding: 0 1;
        margin: 0 0 1 0;
        height: auto;
    }
    CommandGroup Label.group-title {
        color: $accent;
        text-style: bold;
        margin-bottom: 0;
    }
    CommandGroup CommandButton {
        width: 1fr;
        margin: 0;
        height: 3;
        background: $surface;
    }
    CommandGroup CommandButton:hover {
        background: $accent-darken-3;
    }
    CommandGroup CommandButton:focus {
        background: $accent-darken-2;
    }
    """

    def __init__(self, title: str, buttons: list[tuple[str, str, str]], **kwargs) -> None:
        super().__init__(**kwargs)
        self._title = title
        self._buttons = buttons

    def compose(self) -> ComposeResult:
        yield Label(self._title, classes="group-title")
        for label, cmd, btn_id in self._buttons:
            yield CommandButton(label, cmd, btn_id)


class StepFeedControl(Widget):
    """[ / ] and - / = controls displayed as a widget."""

    DEFAULT_CSS = """
    StepFeedControl {
        height: auto;
        border: round $accent-darken-2;
        padding: 0 1;
        margin: 0 0 1 0;
    }
    StepFeedControl Label {
        color: $accent;
        text-style: bold;
    }
    StepFeedControl .control-row {
        height: 3;
        layout: horizontal;
    }
    StepFeedControl Button {
        width: 5;
        height: 3;
        min-width: 5;
        background: $surface;
    }
    StepFeedControl Button:hover {
        background: $accent-darken-3;
    }
    StepFeedControl .val-label {
        width: 1fr;
        content-align: center middle;
    }
    """

    step: reactive[float] = reactive(2.0)
    feed: reactive[int] = reactive(900)

    def compose(self) -> ComposeResult:
        yield Label("Step / Feed")
        with Horizontal(classes="control-row"):
            yield Button("[", id="step_down", classes="ctrl-btn")
            yield Label(f"{self.step:g}", id="step_val", classes="val-label")
            yield Button("]", id="step_up", classes="ctrl-btn")
        with Horizontal(classes="control-row"):
            yield Button("-", id="feed_down", classes="ctrl-btn")
            yield Label(str(self.feed), id="feed_val", classes="val-label")
            yield Button("=", id="feed_up", classes="ctrl-btn")

    def watch_step(self, val: float) -> None:
        try:
            self.query_one("#step_val", Label).update(f"{val:g}")
        except Exception:
            pass

    def watch_feed(self, val: int) -> None:
        try:
            self.query_one("#feed_val", Label).update(str(val))
        except Exception:
            pass


# ─── Calibration Panel ────────────────────────────────────────────────────────

class CalibrationPanel(Widget):
    """Modal-style panel for triggering and monitoring calibration."""

    DEFAULT_CSS = """
    CalibrationPanel {
        border: double $warning;
        padding: 0 1 1 1;
        margin: 0 0 1 0;
        height: auto;
    }
    CalibrationPanel Label.cal-title {
        color: $warning;
        text-style: bold;
        margin-bottom: 0;
    }
    CalibrationPanel Label.cal-field {
        color: $text-muted;
        margin-top: 1;
    }
    CalibrationPanel Input {
        margin-bottom: 0;
    }
    CalibrationPanel #cal-status {
        margin-top: 1;
        height: auto;
        min-height: 1;
    }
    CalibrationPanel #cal-go-btn {
        margin-top: 1;
        width: 1fr;
        height: 3;
        background: $warning-darken-2;
        color: $text;
    }
    CalibrationPanel #cal-go-btn:hover {
        background: $warning;
    }
    CalibrationPanel #cal-go-btn:disabled {
        background: $surface-darken-1;
        color: $text-disabled;
    }
    """

    # reactive status label text
    status: reactive[str] = reactive("")

    def compose(self) -> ComposeResult:
        yield Label("Calibration (B)", classes="cal-title")
        yield Label("Wall ID:", classes="cal-field")
        yield Input(placeholder="e.g. 3", id="cal-wall-id", type="integer")
        yield Label("G1 send-on-stop command:", classes="cal-field")
        yield Input(placeholder="e.g. G1 X0 Y0 F1000", id="cal-g1-cmd", value="G1 X0 Y0 F1000")
        yield Button("Start Calibration", id="cal-go-btn")
        yield Label("", id="cal-status")

    def watch_status(self, val: str) -> None:
        try:
            self.query_one("#cal-status", Label).update(val)
        except Exception:
            pass

    def set_running(self, running: bool) -> None:
        try:
            btn = self.query_one("#cal-go-btn", Button)
            btn.disabled = running
            btn.label = "Calibrating…" if running else "Start Calibration"
        except Exception:
            pass

    @property
    def wall_id_value(self) -> str:
        try:
            return self.query_one("#cal-wall-id", Input).value.strip()
        except Exception:
            return ""

    @property
    def g1_cmd_value(self) -> str:
        try:
            return self.query_one("#cal-g1-cmd", Input).value.strip()
        except Exception:
            return ""


# ─── Main TUI App ─────────────────────────────────────────────────────────────

class ScribitTUI(TextualApp):
    """Scribit interactive jog TUI built with Textual."""

    TITLE = "Scribit Jog CLI"
    CSS = """
    Screen {
        layout: horizontal;
    }

    #left-panel {
        width: 36;
        min-width: 30;
        height: 100%;
        border-right: tall $accent-darken-3;
        overflow-y: auto;
        padding: 0 1;
    }

    #right-panel {
        width: 1fr;
        height: 100%;
        layout: vertical;
    }

    #status-bar {
        height: 1;
        background: $surface-darken-2;
        padding: 0 1;
        dock: top;
    }

    #log-panel {
        height: 1fr;
        border: round $accent-darken-2;
        margin: 0 1 0 0;
    }

    #log-panel RichLog {
        height: 1fr;
    }

    #log-title {
        color: $accent;
        text-style: bold;
        padding: 0 1;
    }

    #reset-btn {
        dock: bottom;
        margin: 1 1;
        height: 3;
        background: $error-darken-2;
        color: $text;
        width: 1fr;
    }
    #reset-btn:hover {
        background: $error;
    }

    CommandGroup {
        border: round $accent-darken-2;
    }

    .section-title {
        color: $accent;
        text-style: bold;
        margin-top: 1;
    }
    """

    BINDINGS = [
        # calibration
        Binding("b", "focus_calibration", "B Calibrate", show=True),
        # cable drive
        Binding("up", "jog('BOTH_IN')", "↑ Both IN", show=False),
        Binding("down", "jog('BOTH_OUT')", "↓ Both OUT", show=False),
        Binding("left", "jog('LEFTISH')", "← Leftish", show=False),
        Binding("right", "jog('RIGHTISH')", "→ Rightish", show=False),
        Binding("w", "jog('BOTH_IN')", "W Both IN", show=False),
        Binding("s", "jog('BOTH_OUT')", "S Both OUT", show=False),
        Binding("a", "jog('LEFTISH')", "A Leftish", show=False),
        Binding("d", "jog('RIGHTISH')", "D Rightish", show=False),
        Binding("q", "jog('L_IN')", "Q L-IN", show=False),
        Binding("e", "jog('L_OUT')", "E L-OUT", show=False),
        Binding("z", "jog('R_IN')", "Z R-IN", show=False),
        Binding("c", "jog('R_OUT')", "C R-OUT", show=False),
        # carousel / home
        Binding("j", "jog('CAR_CCW')", "J CAR CCW", show=False),
        Binding("k", "jog('CAR_CW')", "K CAR CW", show=False),
        Binding("h", "jog('G77')", "H Home", show=False),
        # pen down
        Binding("1", "jog('P1_DOWN')", "1 Pen1↓", show=False),
        Binding("2", "jog('P2_DOWN')", "2 Pen2↓", show=False),
        Binding("3", "jog('P3_DOWN')", "3 Pen3↓", show=False),
        Binding("4", "jog('P4_DOWN')", "4 Pen4↓", show=False),
        # pen up (shifted)
        Binding("!", "jog('P1_UP')", "! Pen1↑", show=False),
        Binding("@", "jog('P2_UP')", "@ Pen2↑", show=False),
        Binding("#", "jog('P3_UP')", "# Pen3↑", show=False),
        Binding("$", "jog('P4_UP')", "$ Pen4↑", show=False),
        # step / feed
        Binding("[", "step_down", "[ Step↓", show=True),
        Binding("]", "step_up", "] Step↑", show=True),
        Binding("-", "feed_down", "- Feed↓", show=True),
        Binding("=", "feed_up", "= Feed↑", show=True),
        # misc
        Binding("x", "reset_n", "X Reset", show=True),
        Binding("ctrl+c,escape", "quit", "Quit", show=True),
    ]

    step: reactive[float] = reactive(2.0)
    feed: reactive[int] = reactive(900)

    def __init__(self, app_ref: App, step0: float, feed0: int) -> None:
        super().__init__()
        self._app = app_ref
        self._step_idx = self._nearest_idx(step0, STEP_CHOICES)
        self._feed_idx = self._nearest_idx(feed0, FEED_CHOICES)
        self.step = STEP_CHOICES[self._step_idx]
        self.feed = FEED_CHOICES[self._feed_idx]
        self._mqtt_sub: MqttLogSubscriber | None = None
        self._calibrating = False

    # ── helpers ──────────────────────────────────────────────────────────────

    @staticmethod
    def _nearest_idx(val: float, choices: list) -> int:
        return min(range(len(choices)), key=lambda i: abs(choices[i] - val))

    def _apply_settings(self) -> None:
        SETTINGS.set(step=self.step, feed=self.feed)

    # ── composition ──────────────────────────────────────────────────────────

    def compose(self) -> ComposeResult:
        yield Header()
        with Horizontal():
            with ScrollableContainer(id="left-panel"):
                for title, buttons in BUTTON_GROUPS:
                    yield CommandGroup(title, buttons)
                yield StepFeedControl(id="sf-ctrl")
                yield CalibrationPanel(id="cal-panel")
                yield Button("X  Reset / Stop", id="reset-btn", variant="error")
        with Vertical(id="right-panel"):
            yield StatusBar(self._app, id="status-bar")
            with Container(id="log-panel"):
                yield Label("MQTT Log", id="log-title")
                yield RichLog(highlight=True, markup=True, id="rich-log", wrap=True)
        yield Footer()

    def on_mount(self) -> None:
        self._apply_settings()
        self._update_sf_widget()
        self._start_mqtt_subscriber()

    def _update_sf_widget(self) -> None:
        try:
            sf = self.query_one(StepFeedControl)
            sf.step = self.step
            sf.feed = self.feed
        except Exception:
            pass

    def _start_mqtt_subscriber(self) -> None:
        a = self._app
        self._mqtt_sub = MqttLogSubscriber(
            host=a.mqtt_host,
            port=a.mqtt_port,
            user=a.mqtt_user,
            pw=a.mqtt_pass,
            robot_id=a.robot_id,
            callback=self._on_mqtt_log,
        )

    def _on_mqtt_log(self, markup: str, level: str = "info") -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        self.call_from_thread(self._append_log, f"[dim]{ts}[/] {markup}")
        if level in ("calibrating", "error") and self._calibrating:
            self.call_from_thread(self._update_cal_status, markup, level)

    def _append_log(self, markup: str) -> None:
        try:
            log = self.query_one("#rich-log", RichLog)
            log.write(markup)
        except Exception:
            pass

    def _update_cal_status(self, markup: str, level: str) -> None:
        try:
            panel = self.query_one(CalibrationPanel)
            if level == "calibrating":
                # {"status":0} = success, {"status":1} = failed
                if '"status":0' in markup:
                    panel.status = "[bold green]Calibration succeeded![/]"
                    panel.set_running(False)
                    self._calibrating = False
                elif '"status":1' in markup:
                    panel.status = "[bold red]Calibration failed after all retries.[/]"
                    panel.set_running(False)
                    self._calibrating = False
                else:
                    panel.status = f"[cyan]{markup}[/]"
            elif level == "error":
                panel.status = f"[red]{markup}[/]"
                panel.set_running(False)
                self._calibrating = False
        except Exception:
            pass

    # ── reactive watches ─────────────────────────────────────────────────────

    def watch_step(self, val: float) -> None:
        self._apply_settings()
        self._update_sf_widget()
        try:
            self.query_one(StatusBar).step = val
        except Exception:
            pass

    def watch_feed(self, val: int) -> None:
        self._apply_settings()
        self._update_sf_widget()
        try:
            self.query_one(StatusBar).feed = val
        except Exception:
            pass

    # ── actions ──────────────────────────────────────────────────────────────

    def action_jog(self, cmd: str) -> None:
        self._dispatch(cmd)

    def action_step_down(self) -> None:
        if self._step_idx > 0:
            self._step_idx -= 1
            self.step = STEP_CHOICES[self._step_idx]

    def action_step_up(self) -> None:
        if self._step_idx < len(STEP_CHOICES) - 1:
            self._step_idx += 1
            self.step = STEP_CHOICES[self._step_idx]

    def action_feed_down(self) -> None:
        if self._feed_idx > 0:
            self._feed_idx -= 1
            self.feed = FEED_CHOICES[self._feed_idx]

    def action_feed_up(self) -> None:
        if self._feed_idx < len(FEED_CHOICES) - 1:
            self._feed_idx += 1
            self.feed = FEED_CHOICES[self._feed_idx]

    def action_reset_n(self) -> None:
        self._log_cmd("RESET N")
        threading.Thread(target=self._app.reset_n, daemon=True).start()

    def action_focus_calibration(self) -> None:
        try:
            self.query_one("#cal-wall-id", Input).focus()
        except Exception:
            pass

    # ── button handler ───────────────────────────────────────────────────────

    @on(Button.Pressed)
    def _button_pressed(self, event: Button.Pressed) -> None:
        btn_id = event.button.id
        if btn_id == "reset-btn":
            self.action_reset_n()
            return
        if btn_id in ("step_down", "step_up", "feed_down", "feed_up"):
            getattr(self, f"action_{btn_id}")()
            return
        if btn_id == "cal-go-btn":
            self._start_calibration()
            return
        # CommandButton
        if isinstance(event.button, CommandButton):
            self._dispatch(event.button.scribit_cmd)

    # ── internal ─────────────────────────────────────────────────────────────

    def _start_calibration(self) -> None:
        try:
            panel = self.query_one(CalibrationPanel)
        except Exception:
            return

        wall_id_str = panel.wall_id_value
        g1_cmd = panel.g1_cmd_value

        if not wall_id_str:
            panel.status = "[red]Enter a Wall ID first.[/]"
            return
        try:
            wall_id = int(wall_id_str)
        except ValueError:
            panel.status = "[red]Wall ID must be an integer.[/]"
            return
        if not g1_cmd.startswith("G1"):
            panel.status = "[red]G1 command must start with 'G1'.[/]"
            return

        self._calibrating = True
        panel.set_running(True)
        panel.status = "[yellow]Calibration started — waiting for robot…[/]"
        ts = datetime.now().strftime("%H:%M:%S")
        try:
            log = self.query_one("#rich-log", RichLog)
            log.write(f"[dim]{ts}[/] [bold yellow]→ CALIBRATE[/] wall=[cyan]{wall_id}[/]  g1=[white]{g1_cmd}[/]")
        except Exception:
            pass
        threading.Thread(
            target=self._app.publish_calibration,
            args=(wall_id, g1_cmd),
            daemon=True,
        ).start()

    def _dispatch(self, cmd: str) -> None:
        self._log_cmd(cmd)
        threading.Thread(
            target=self._app.publish_manual_cmd,
            args=(cmd,),
            daemon=True,
        ).start()

    def _log_cmd(self, cmd: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        try:
            log = self.query_one("#rich-log", RichLog)
            log.write(f"[dim]{ts}[/] [bold green]→ SEND[/] [yellow]{cmd}[/]  step=[white]{self.step:g}[/]  feed=[white]{self.feed}[/]")
        except Exception:
            pass
        try:
            self.query_one(StatusBar).last_cmd = cmd
        except Exception:
            pass

    def on_unmount(self) -> None:
        if self._mqtt_sub:
            try:
                self._mqtt_sub.stop()
            except Exception:
                pass


# ─── entry point ─────────────────────────────────────────────────────────────

def run_curses(app: App, step0: float, feed0: int) -> None:
    """Replaces the old curses TUI with a Textual app (same signature)."""
    tui = ScribitTUI(app_ref=app, step0=step0, feed0=feed0)
    tui.run()
