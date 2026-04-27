#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Driver helpers for the Qt Widgets test automation bridge."""

import json
import os
import re
import socket
import subprocess
import time
from decimal import Decimal

from .test_framework import BitcoinTestFramework, SkipTest
from .test_node import TestNode
from .util import assert_equal, get_datadir_path


GUI_NODE_INDEX = 2


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


def parse_display_amount(text):
    cleaned = re.sub(r"[^0-9.\-]", "", text)
    return Decimal(cleaned) if cleaned else Decimal("0")


class QtWidgetsTestFramework(BitcoinTestFramework):
    GUI_NODE_INDEX = GUI_NODE_INDEX

    def set_test_params(self):
        self.num_nodes = 3
        self.noban_tx_relay = True
        self.supports_cli = False
        self.uses_wallet = True
        common_args = ["-fallbackfee=0.0002", "-walletrbf=1"]
        self.extra_args = [list(common_args), list(common_args), list(common_args)]

    def run_test(self):
        raise NotImplementedError

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        try:
            self.gui_binary = find_bitcoin_qt_binary(self.config)
        except FileNotFoundError as e:
            raise SkipTest(str(e))
        if not bitcoin_qt_supports_test_automation(self.gui_binary):
            raise SkipTest("bitcoin-qt was not built with -DENABLE_TEST_AUTOMATION=ON")

    def get_gui_node_extra_args(self):
        return list(self.extra_args[self.GUI_NODE_INDEX])

    def should_import_gui_wallet(self):
        return self.uses_wallet

    def gui_start_env(self):
        return {"QT_QPA_PLATFORM": "offscreen"}

    def setup_nodes(self):
        self.socket_path = os.path.join(self.options.tmpdir, "qt_bridge.sock")
        self.screenshot_dir = os.path.join(self.options.tmpdir, "qt_screenshots")
        self.screenshot_index = 0

        self.add_nodes(2, self.extra_args[:2])

        gui_binaries = self.get_binaries()
        gui_binaries.paths.bitcoind = self.gui_binary
        gui_node = TestNode(
            self.GUI_NODE_INDEX,
            get_datadir_path(self.options.tmpdir, self.GUI_NODE_INDEX),
            chain=self.chain,
            rpchost=None,
            timewait=self.rpc_timeout,
            timeout_factor=self.options.timeout_factor,
            binaries=gui_binaries,
            coverage_dir=self.options.coveragedir,
            cwd=self.options.tmpdir,
            extra_args=self.get_gui_node_extra_args() + [f"-test-automation={self.socket_path}"],
            use_cli=self.options.usecli,
            start_perf=self.options.perf,
            v2transport=self.options.v2transport,
            uses_wallet=self.uses_wallet,
        )
        self.nodes.append(gui_node)

        self.start_node(0)
        self.start_node(1)
        self.start_node(self.GUI_NODE_INDEX, env=self.gui_start_env())

        if self.uses_wallet:
            self.init_wallet(node=0)
            self.init_wallet(node=1)
            if self.should_import_gui_wallet():
                self.init_wallet(node=self.GUI_NODE_INDEX)

        if not self.setup_clean_chain:
            for node in self.nodes:
                assert_equal(node.getblockchaininfo()["blocks"], 199)
            block_hash = self.generate(self.nodes[0], 1, sync_fun=self.no_op)[0]
            block = self.nodes[0].getblock(blockhash=block_hash, verbosity=0)
            for node in self.nodes:
                node.submitblock(block)

    def create_gui_driver(self, *, timeout=60):
        return QtDriver(self.socket_path, timeout=timeout)

    def wait_for_main_window(self, gui, *, timeout_ms=60000):
        gui.wait_for_window("mainWindow", timeout_ms=timeout_ms)
        return gui

    def restart_gui(self, gui=None):
        if gui is not None:
            gui.close()
        self.stop_node(self.GUI_NODE_INDEX)
        self.start_node(self.GUI_NODE_INDEX, env=self.gui_start_env())
        return self.create_gui_driver()

    def capture_screenshot(self, gui, step_name, *, window=None):
        os.makedirs(self.screenshot_dir, exist_ok=True)
        self.screenshot_index += 1
        safe_name = re.sub(r"[^a-z0-9_]+", "_", step_name.lower()).strip("_")
        target_window = window
        if target_window is None:
            try:
                target_window = gui.get_active_window()
            except Exception:
                target_window = "mainWindow"
        path = os.path.join(self.screenshot_dir, f"{self.screenshot_index:02d}_{safe_name}.png")
        try:
            gui.save_screenshot(path, window=target_window)
            self.log.info("Saved GUI screenshot %s (%s)", path, target_window)
        except Exception as e:
            self.log.info("Failed to save GUI screenshot %s: %s", path, e)

    def dump_gui_state(self, gui):
        try:
            self.log.info("GUI windows: %s", gui.list_windows())
        except Exception as e:
            self.log.info("Failed to list GUI windows: %s", e)
        try:
            active_window = gui.get_active_window()
            self.log.info("Active GUI window: %s", active_window)
            if active_window == "QMessageBox":
                self.log.info("Active message box text: %s", gui.get_text("qt_msgbox_label", window=active_window))
        except Exception as e:
            self.log.info("Failed to inspect active GUI window: %s", e)
        try:
            self.log.info("Main window objects: %s", gui.list_objects(window="mainWindow"))
        except Exception as e:
            self.log.info("Failed to list main window objects: %s", e)
        self.capture_screenshot(gui, "failure_active_window")
        self.capture_screenshot(gui, "failure_main_window", window="mainWindow")
