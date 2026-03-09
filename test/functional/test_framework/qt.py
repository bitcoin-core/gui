#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Driver helpers for the Qt Widgets test automation bridge."""

import json
import os
import socket
import subprocess
import time


class QtDriverError(Exception):
    """Raised when the Qt test bridge returns an error response."""


class QtDriver:
    """Drive bitcoin-qt via the Qt Widgets test bridge."""

    def __init__(self, socket_path, timeout=30):
        self.socket_path = socket_path
        self.timeout = timeout
        self.sock = None
        self._connect()

    def _connect(self):
        deadline = time.time() + self.timeout
        last_err = None
        while time.time() < deadline:
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.settimeout(self.timeout)
                self.sock.connect(self.socket_path)
                return
            except (ConnectionRefusedError, FileNotFoundError, OSError) as e:
                last_err = e
                if self.sock:
                    self.sock.close()
                    self.sock = None
                time.sleep(0.25)
        raise QtDriverError(f"Could not connect to test bridge at {self.socket_path}: {last_err}")

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def __enter__(self):
        return self

    def __exit__(self, *_exc_info):
        self.close()

    def list_windows(self):
        return self._expect(self._send({"cmd": "list_windows"}), "list_windows")["windows"]

    def get_active_window(self):
        return self._expect(self._send({"cmd": "get_active_window"}), "get_active_window")["window"]

    def get_current_view(self, window=None):
        cmd = {"cmd": "get_current_view"}
        if window is not None:
            cmd["window"] = window
        return self._expect(self._send(cmd), "get_current_view")["view"]

    def click(self, object_name, *, window=None):
        cmd = {"cmd": "click", "objectName": object_name}
        if window is not None:
            cmd["window"] = window
        self._expect(self._send(cmd), f"click({object_name!r})")

    def set_text(self, object_name, text, *, window=None):
        cmd = {"cmd": "set_text", "objectName": object_name, "text": text}
        if window is not None:
            cmd["window"] = window
        self._expect(self._send(cmd), f"set_text({object_name!r})")

    def get_text(self, object_name, *, window=None):
        cmd = {"cmd": "get_text", "objectName": object_name}
        if window is not None:
            cmd["window"] = window
        return self._expect(self._send(cmd), f"get_text({object_name!r})")["text"]

    def get_property(self, object_name, prop, *, window=None):
        cmd = {"cmd": "get_property", "objectName": object_name, "prop": prop}
        if window is not None:
            cmd["window"] = window
        return self._expect(self._send(cmd), f"get_property({object_name!r}, {prop!r})")["value"]

    def wait_for_window(self, window_name, timeout_ms=5000):
        self._expect(
            self._send({"cmd": "wait_for_window", "window": window_name, "timeout": timeout_ms}),
            f"wait_for_window({window_name!r})",
        )

    def wait_for_view(self, view_name, *, window=None, timeout_ms=5000):
        cmd = {"cmd": "wait_for_view", "view": view_name, "timeout": timeout_ms}
        if window is not None:
            cmd["window"] = window
        self._expect(self._send(cmd), f"wait_for_view({view_name!r})")

    def wait_for_property(self, object_name, prop, *, window=None, timeout_ms=5000, value=None, contains=None, non_empty=False):
        cmd = {
            "cmd": "wait_for_property",
            "objectName": object_name,
            "prop": prop,
            "timeout": timeout_ms,
        }
        if window is not None:
            cmd["window"] = window
        if value is not None:
            cmd["value"] = value
        if contains is not None:
            cmd["contains"] = contains
        if non_empty:
            cmd["nonEmpty"] = True
        return self._expect(self._send(cmd), f"wait_for_property({object_name!r}, {prop!r})").get("value")

    def list_objects(self, *, window=None):
        cmd = {"cmd": "list_objects"}
        if window is not None:
            cmd["window"] = window
        return self._expect(self._send(cmd), "list_objects")["objects"]

    def save_screenshot(self, path, *, window=None):
        cmd = {"cmd": "save_screenshot", "path": path}
        if window is not None:
            cmd["window"] = window
        return self._expect(self._send(cmd), f"save_screenshot({path!r})")

    def _send(self, cmd):
        payload = json.dumps(cmd) + "\n"
        self.sock.sendall(payload.encode("utf-8"))
        return self._recv()

    def _recv(self):
        buf = b""
        while True:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise QtDriverError("Connection closed by test bridge")
            buf += chunk
            if b"\n" in buf:
                line, _ = buf.split(b"\n", 1)
                return json.loads(line)

    @staticmethod
    def _expect(response, operation):
        if "error" in response:
            raise QtDriverError(f"{operation} failed: {response['error']}")
        return response


def find_bitcoin_qt_binary(config):
    env_path = os.getenv("BITCOIN_QT")
    if env_path and os.path.isfile(env_path):
        return env_path

    builddir = config["environment"]["BUILDDIR"]
    exeext = config["environment"]["EXEEXT"]
    default_path = os.path.join(builddir, "bin", f"bitcoin-qt{exeext}")
    if os.path.isfile(default_path):
        return default_path

    raise FileNotFoundError(
        "Cannot find bitcoin-qt binary. Set BITCOIN_QT or build bitcoin-qt with "
        "-DENABLE_TEST_AUTOMATION=ON."
    )


def bitcoin_qt_supports_test_automation(binary_path):
    try:
        result = subprocess.run(
            ["strings", binary_path],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
        if "-test-automation=<path>" in result.stdout:
            return True
    except OSError:
        pass

    env = dict(os.environ)
    env.setdefault("QT_QPA_PLATFORM", "minimal")
    result = subprocess.run(
        [binary_path, "-help"],
        check=False,
        capture_output=True,
        text=True,
        timeout=20,
        env=env,
    )
    output = result.stdout + result.stderr
    return "-test-automation=<path>" in output
