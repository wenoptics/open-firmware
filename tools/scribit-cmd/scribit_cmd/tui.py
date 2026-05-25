from __future__ import annotations

import threading
from datetime import datetime
from typing import ClassVar

import paho.mqtt.client as mqtt
from textual import on
from textual.app import App as TextualApp
from textual.app import ComposeResult
from textual.binding import Binding
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Button, Footer, Header, Label, RichLog, Static, TabbedContent, TabPane, TextArea

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
]


# ─── MQTT subscriber for the log panel ───────────────────────────────────────

class MqttLogSubscriber:
    """Persistent MQTT client that subscribes to all tin/<robot_id>/# topics."""

    def __init__(self, host: str, port: int, user: str, pw: str, robot_id: str, callback) -> None:
        self._host = host
        self._port = port
        self._cb = callback
        self._topics = [f"tin/{robot_id}/#", f"tout/{robot_id}/#"]
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
            for topic in self._topics:
                client.subscribe(topic, qos=0)
                self._cb(f"[dim]Subscribed to {topic}[/]")
        else:
            self._cb(f"[red]MQTT connect refused: {reason_code}[/]")

    def _on_message(self, _client, _userdata, message) -> None:
        topic = message.topic
        payload = message.payload.decode(errors="replace")
        self._cb(f"[cyan]{topic}[/]  [white]{payload}[/]")

    def _on_disconnect(self, _client, _userdata, _disconnect_flags, reason_code, _properties) -> None:
        self._cb(f"[yellow]MQTT subscriber disconnected (rc={reason_code})[/]")

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


class PenGroup(Widget):
    """Four pen slots — each row has ↓ DOWN and ↑ UP side by side."""

    DEFAULT_CSS = """
    PenGroup {
        border: round $accent-darken-2;
        padding: 0 1;
        margin: 0 0 1 0;
        height: auto;
        layout: vertical;
    }
    PenGroup Label.group-title {
        color: $accent;
        text-style: bold;
        height: 1;
    }
    PenGroup .pen-row {
        layout: grid;
        grid-size: 2;
        grid-gutter: 0;
        height: 3;
    }
    PenGroup CommandButton {
        height: 3;
        margin: 0;
        background: $surface;
    }
    PenGroup CommandButton:hover {
        background: $accent-darken-3;
    }
    PenGroup CommandButton:focus {
        background: $accent-darken-2;
    }
    """

    def compose(self) -> ComposeResult:
        yield Label("Pens  (1-4 / !@#$)", classes="group-title")
        for pen, down_key, up_key in [(1, "1", "!"), (2, "2", "@"), (3, "3", "#"), (4, "4", "$")]:
            with Horizontal(classes="pen-row"):
                yield CommandButton(f"{down_key}  Pen {pen} ↓", f"P{pen}_DOWN", f"p{pen}d")
                yield CommandButton(f"{up_key}  Pen {pen} ↑", f"P{pen}_UP", f"p{pen}u")


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


class GCodePanel(Widget):
    """Multi-line GCode editor with a Submit button."""

    DEFAULT_CSS = """
    GCodePanel {
        height: 1fr;
        layout: vertical;
        padding: 0;
    }
    GCodePanel Label.gcode-title {
        color: $accent;
        text-style: bold;
        height: auto;
        margin-bottom: 1;
    }
    GCodePanel TextArea {
        height: 1fr;
        margin-bottom: 1;
    }
    GCodePanel #gcode-submit {
        width: 1fr;
        height: 3;
        background: $success-darken-2;
        color: $text;
    }
    GCodePanel #gcode-submit:hover {
        background: $success;
    }
    GCodePanel #gcode-clear {
        width: 1fr;
        height: 3;
        background: $surface;
        color: $text;
        margin-top: 0;
    }
    GCodePanel #gcode-clear:hover {
        background: $accent-darken-3;
    }
    """

    def compose(self) -> ComposeResult:
        yield Label("Raw GCode  (ctrl+enter to send)", classes="gcode-title")
        yield TextArea(id="gcode-editor", language="python")
        yield Button("Send GCode  (ctrl+enter)", id="gcode-submit", variant="success")
        yield Button("Clear", id="gcode-clear")

    def get_gcode(self) -> str:
        return self.query_one("#gcode-editor", TextArea).text

    def clear(self) -> None:
        self.query_one("#gcode-editor", TextArea).clear()


# ─── Main TUI App ─────────────────────────────────────────────────────────────

class ScribitTUI(TextualApp):
    """Scribit interactive jog TUI built with Textual."""

    TITLE = "Scribit Jog CLI"
    CSS = """
    Screen {
        layout: horizontal;
    }

    Screen > Horizontal {
        height: 1fr;
    }

    #left-panel {
        width: 36;
        min-width: 30;
        height: 100%;
        border-right: tall $accent-darken-3;
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
        margin: 1 0;
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

    TabbedContent {
        height: 100%;
    }

    TabbedContent ContentSwitcher {
        height: 1fr;
    }

    TabPane {
        height: 100%;
        padding: 0 1;
    }

    #jog-scroll {
        height: 100%;
    }
    """

    BINDINGS = [
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
        # mode switching
        Binding("ctrl+g", "switch_tab('gcode')", "GCode Mode", show=True),
        Binding("ctrl+j", "switch_tab('jog')", "Jog Mode", show=True),
        # gcode submit
        Binding("ctrl+enter", "submit_gcode", "Send GCode", show=False),
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
            with Container(id="left-panel"):
                with TabbedContent(id="mode-tabs"):
                    with TabPane("Jog", id="tab-jog"):
                        with VerticalScroll(id="jog-scroll"):
                            for title, buttons in BUTTON_GROUPS:
                                yield CommandGroup(title, buttons)
                            yield PenGroup()
                            yield StepFeedControl(id="sf-ctrl")
                            yield Button("X  Reset / Stop", id="reset-btn", variant="error")
                    with TabPane("GCode", id="tab-gcode"):
                        yield GCodePanel(id="gcode-panel")
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

    def _on_mqtt_log(self, markup: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        self.call_from_thread(self._append_log, f"[dim]{ts}[/] {markup}")

    def _append_log(self, markup: str) -> None:
        try:
            log = self.query_one("#rich-log", RichLog)
            log.write(markup)
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

    def action_switch_tab(self, tab_id: str) -> None:
        try:
            self.query_one("#mode-tabs", TabbedContent).active = f"tab-{tab_id}"
        except Exception:
            pass

    def action_submit_gcode(self) -> None:
        try:
            panel = self.query_one(GCodePanel)
            gcode = panel.get_gcode().strip()
        except Exception:
            return
        if gcode:
            self._dispatch_raw_gcode(gcode)

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
        if btn_id == "gcode-submit":
            self.action_submit_gcode()
            return
        if btn_id == "gcode-clear":
            try:
                self.query_one(GCodePanel).clear()
            except Exception:
                pass
            return
        if isinstance(event.button, CommandButton):
            self._dispatch(event.button.scribit_cmd)

    # ── internal ─────────────────────────────────────────────────────────────

    def _dispatch(self, cmd: str) -> None:
        self._log_cmd(cmd)
        threading.Thread(
            target=self._app.publish_manual_cmd,
            args=(cmd,),
            daemon=True,
        ).start()

    def _dispatch_raw_gcode(self, gcode: str) -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        lines = gcode.splitlines()
        preview = lines[0] if lines else ""
        suffix = f" (+{len(lines)-1} more)" if len(lines) > 1 else ""
        try:
            log = self.query_one("#rich-log", RichLog)
            log.write(f"[dim]{ts}[/] [bold green]→ GCODE[/] [yellow]{preview}[/][dim]{suffix}[/]")
        except Exception:
            pass
        try:
            self.query_one(StatusBar).last_cmd = f"GCODE({len(lines)} lines)"
        except Exception:
            pass
        threading.Thread(
            target=self._app.publish_mqtt,
            args=("manualMove", gcode),
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
