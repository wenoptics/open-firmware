from __future__ import annotations

from .app import App
from .settings import SETTINGS


def run_curses(app: App, step0: float, feed0: int) -> None:
    import curses

    step_choices = [0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 50.0, 72.0, 90.0, 100.0, 200.0]
    feed_choices = [600, 900, 1200, 1800, 2400, 3000]

    def nearest(val, choices):
        return min(choices, key=lambda c: abs(c - val))

    step = nearest(step0, step_choices)
    feed = nearest(feed0, feed_choices)

    def apply_settings() -> None:
        SETTINGS.set(step=step, feed=feed)

    apply_settings()

    help_lines = [
        "Arrow keys / WASD : Up=both in, Down=both out, Left=leftish, Right=rightish",
        "Q / E             : left in/out",
        "Z / C             : right in/out",
        "J / K             : carousel CCW/CW by step (relative Z)",
        "1 2 3 4           : pen 1-4 DOWN (slot Z + G101)",
        "! @ # $           : pen 1-4 UP   (slot Z)",
        "H                 : G77 (home Z-cylinder / carousel)",
        "[ / ]             : step down/up",
        "- / =             : feed down/up",
        "x                 : Reset N (stop)",
        "ESC / Ctrl+G      : quit",
        "",
        "NOTE: Interactive jogs publish inline G-code to MQTT manualMove.",
        "NOTE: Draw/file jobs still use MQTT print payloads as 'URL;<suffix>'.",
    ]

    def draw(stdscr) -> None:
        stdscr.clear()
        stdscr.addstr(0, 0, "Scribit Jog CLI (motor-space + carousel/pen helpers)")
        stdscr.addstr(2, 0, f"MQTT broker:  {app.mqtt_host}:{app.mqtt_port}   robot_id={app.robot_id}")
        stdscr.addstr(3, 0, "Command:      manualMove")
        stdscr.addstr(6, 0, f"step={step:g}   feed={feed}   (step is mm for cables, degrees for carousel)")
        with app.last_lock:
            last = app.last
        stdscr.addstr(7, 0, f"last: {last}")

        y = 9
        for line in help_lines:
            stdscr.addstr(y, 0, line)
            y += 1
        stdscr.refresh()

    def publish(cmd: str) -> None:
        app.publish_manual_cmd(cmd)

    def step_down() -> None:
        nonlocal step
        i = step_choices.index(step)
        if i > 0:
            step = step_choices[i - 1]
            apply_settings()

    def step_up() -> None:
        nonlocal step
        i = step_choices.index(step)
        if i < len(step_choices) - 1:
            step = step_choices[i + 1]
            apply_settings()

    def feed_down() -> None:
        nonlocal feed
        i = feed_choices.index(feed)
        if i > 0:
            feed = feed_choices[i - 1]
            apply_settings()

    def feed_up() -> None:
        nonlocal feed
        i = feed_choices.index(feed)
        if i < len(feed_choices) - 1:
            feed = feed_choices[i + 1]
            apply_settings()

    def loop(stdscr) -> None:
        stdscr.nodelay(False)
        stdscr.keypad(True)
        curses.curs_set(0)

        while True:
            draw(stdscr)
            ch = stdscr.getch()

            if ch == curses.KEY_UP:
                publish("BOTH_IN")
            elif ch == curses.KEY_DOWN:
                publish("BOTH_OUT")
            elif ch == curses.KEY_LEFT:
                publish("LEFTISH")
            elif ch == curses.KEY_RIGHT:
                publish("RIGHTISH")
            elif ch in (ord("w"), ord("W")):
                publish("BOTH_IN")
            elif ch in (ord("s"), ord("S")):
                publish("BOTH_OUT")
            elif ch in (ord("a"), ord("A")):
                publish("LEFTISH")
            elif ch in (ord("d"), ord("D")):
                publish("RIGHTISH")
            elif ch in (ord("q"), ord("Q")):
                publish("L_IN")
            elif ch in (ord("e"), ord("E")):
                publish("L_OUT")
            elif ch in (ord("z"), ord("Z")):
                publish("R_IN")
            elif ch in (ord("c"), ord("C")):
                publish("R_OUT")
            elif ch in (ord("j"), ord("J")):
                publish("CAR_CCW")
            elif ch in (ord("k"), ord("K")):
                publish("CAR_CW")
            elif ch == ord("1"):
                publish("P1_DOWN")
            elif ch == ord("2"):
                publish("P2_DOWN")
            elif ch == ord("3"):
                publish("P3_DOWN")
            elif ch == ord("4"):
                publish("P4_DOWN")
            elif ch == ord("!"):
                publish("P1_UP")
            elif ch == ord("@"):
                publish("P2_UP")
            elif ch == ord("#"):
                publish("P3_UP")
            elif ch == ord("$"):
                publish("P4_UP")
            elif ch == ord("H"):
                publish("G77")
            elif ch == ord("["):
                step_down()
            elif ch == ord("]"):
                step_up()
            elif ch == ord("-"):
                feed_down()
            elif ch == ord("="):
                feed_up()
            elif ch in (ord("x"), ord("X")):
                app.reset_n()
            elif ch in (27, 7):
                return

    curses.wrapper(loop)
