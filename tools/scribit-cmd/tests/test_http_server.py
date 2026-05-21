from __future__ import annotations

import threading
import urllib.request
from http.server import ThreadingHTTPServer

from scribit_cmd.gcode import Cache
from scribit_cmd.http_server import DynamicGcodeStore, FileHandler, GcodeHandler
from scribit_cmd.settings import CurrentSettings, SharedSettings


def serve(handler: type) -> tuple[ThreadingHTTPServer, str]:
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    host, port = httpd.server_address
    return httpd, f"http://{host}:{port}"


def fetch(url: str) -> tuple[int, bytes]:
    try:
        with urllib.request.urlopen(url, timeout=2) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read()


def test_gcode_handler_serves_dynamic_gcode_before_cache() -> None:
    class TestHandler(GcodeHandler):
        settings = SharedSettings(CurrentSettings(step=2.0, feed=900))
        cache = Cache()
        dynamic_gcode = DynamicGcodeStore()

    TestHandler.dynamic_gcode.set("P1_UP", "dynamic\n")
    httpd, base_url = serve(TestHandler)
    try:
        assert fetch(f"{base_url}/g/P1_UP.gcode") == (200, b"dynamic\n")
    finally:
        httpd.shutdown()


def test_gcode_handler_builds_static_gcode_from_settings() -> None:
    class TestHandler(GcodeHandler):
        settings = SharedSettings(CurrentSettings(step=3.0, feed=1200))
        cache = Cache()
        dynamic_gcode = DynamicGcodeStore()

    httpd, base_url = serve(TestHandler)
    try:
        assert fetch(f"{base_url}/g/R_OUT.gcode") == (
            200,
            b"G21\nG91\nM17\nG1 X0.000 Y-3.000 F1200\n",
        )
    finally:
        httpd.shutdown()


def test_file_handler_serves_quoted_filename(tmp_path) -> None:
    path = tmp_path / "hello world.gcode"
    path.write_text("G21\n", encoding="utf-8")

    class TestHandler(FileHandler):
        downloaded = threading.Event()
        url_path = "/hello world.gcode"
        gcode_path = path

    httpd, base_url = serve(TestHandler)
    try:
        assert fetch(f"{base_url}/hello%20world.gcode") == (200, b"G21\n")
        assert TestHandler.downloaded.is_set()
    finally:
        httpd.shutdown()
