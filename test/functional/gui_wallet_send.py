#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""End-to-end send transaction test via bitcoin-qt and the test bridge."""

from decimal import Decimal
import os
import re

from test_framework.qt import (
    QtDriver,
    bitcoin_qt_supports_test_automation,
    find_bitcoin_qt_binary,
)
from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.test_node import TestNode
from test_framework.util import (
    assert_equal,
    get_datadir_path,
)


GUI_NODE_INDEX = 2


def parse_display_amount(text):
    cleaned = re.sub(r"[^0-9.\-]", "", text)
    return Decimal(cleaned) if cleaned else Decimal("0")


class WalletSendGuiTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.noban_tx_relay = True
        self.supports_cli = False
        self.uses_wallet = True
        self.extra_args = [
            ["-fallbackfee=0.0002", "-walletrbf=1"],
            ["-fallbackfee=0.0002", "-walletrbf=1"],
            ["-fallbackfee=0.0002", "-walletrbf=1"],
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        try:
            self.gui_binary = find_bitcoin_qt_binary(self.config)
        except FileNotFoundError as e:
            raise SkipTest(str(e))
        if not bitcoin_qt_supports_test_automation(self.gui_binary):
            raise SkipTest("bitcoin-qt was not built with -DENABLE_TEST_AUTOMATION=ON")

    def setup_nodes(self):
        self.socket_path = os.path.join(self.options.tmpdir, "qt_bridge.sock")
        self.screenshot_dir = os.path.join(self.options.tmpdir, "qt_screenshots")
        self.screenshot_index = 0

        self.add_nodes(2, self.extra_args[:2])

        gui_binaries = self.get_binaries()
        gui_binaries.paths.bitcoind = self.gui_binary
        gui_node = TestNode(
            GUI_NODE_INDEX,
            get_datadir_path(self.options.tmpdir, GUI_NODE_INDEX),
            chain=self.chain,
            rpchost=None,
            timewait=self.rpc_timeout,
            timeout_factor=self.options.timeout_factor,
            binaries=gui_binaries,
            coverage_dir=self.options.coveragedir,
            cwd=self.options.tmpdir,
            extra_args=self.extra_args[GUI_NODE_INDEX] + [f"-test-automation={self.socket_path}"],
            use_cli=self.options.usecli,
            start_perf=self.options.perf,
            v2transport=self.options.v2transport,
            uses_wallet=self.uses_wallet,
        )
        self.nodes.append(gui_node)

        self.start_node(0)
        self.start_node(1)
        self.start_node(GUI_NODE_INDEX, env={"QT_QPA_PLATFORM": "offscreen"})

        if self.uses_wallet:
            self.import_deterministic_coinbase_privkeys()

        if not self.setup_clean_chain:
            for node in self.nodes:
                assert_equal(node.getblockchaininfo()["blocks"], 199)
            block_hash = self.generate(self.nodes[0], 1, sync_fun=self.no_op)[0]
            block = self.nodes[0].getblock(blockhash=block_hash, verbosity=0)
            for node in self.nodes:
                node.submitblock(block)

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

    def run_test(self):
        gui_wallet = self.nodes[GUI_NODE_INDEX].get_wallet_rpc(self.default_wallet_name)
        recipient_wallet = self.nodes[1].get_wallet_rpc(self.default_wallet_name)
        amount = Decimal("1.25")
        amount_sats = int(amount * Decimal(100_000_000))
        recipient_address = recipient_wallet.getnewaddress()

        gui = QtDriver(self.socket_path, timeout=60)
        try:
            gui.wait_for_window("mainWindow", timeout_ms=60000)
            gui.wait_for_property("sendCoinsAction", "enabled", window="mainWindow", value=True, timeout_ms=60000)
            self.capture_screenshot(gui, "main_window_ready", window="mainWindow")

            self.log.info("Open the Send view and submit a transaction through bitcoin-qt")
            gui.click("sendCoinsAction", window="mainWindow")
            gui.wait_for_view("SendCoinsDialog", window="mainWindow", timeout_ms=10000)
            self.wait_until(
                lambda: parse_display_amount(gui.get_text("labelBalance", window="mainWindow")) > amount,
                timeout=30,
            )
            self.capture_screenshot(gui, "send_view_open", window="mainWindow")
            gui.set_text("payTo", recipient_address, window="mainWindow")
            gui.set_text("payAmount", str(amount), window="mainWindow")
            self.wait_until(lambda: gui.get_text("payTo", window="mainWindow") == recipient_address, timeout=5)
            self.wait_until(lambda: gui.get_property("payAmount", "value", window="mainWindow") == amount_sats, timeout=5)
            self.capture_screenshot(gui, "send_form_filled", window="mainWindow")
            gui.click("sendButton", window="mainWindow")

            self.wait_until(lambda: gui.get_active_window() != "mainWindow", timeout=10)
            active_window = gui.get_active_window()
            if active_window != "sendConfirmationDialog":
                message_text = ""
                if active_window == "QMessageBox":
                    message_text = gui.get_text("qt_msgbox_label", window=active_window)
                raise AssertionError(f"Unexpected modal window {active_window}: {message_text}")
            self.capture_screenshot(gui, "confirmation_dialog_shown", window="sendConfirmationDialog")
            gui.wait_for_property("sendConfirmButton", "enabled", window="sendConfirmationDialog", value=True, timeout_ms=10000)
            self.capture_screenshot(gui, "confirmation_dialog_enabled", window="sendConfirmationDialog")
            gui.click("sendConfirmButton", window="sendConfirmationDialog")

            self.wait_until(
                lambda: recipient_wallet.getreceivedbyaddress(recipient_address, 0) == amount,
                timeout=30,
            )
            self.capture_screenshot(gui, "transaction_broadcast", window="mainWindow")
            self.sync_mempools([self.nodes[0], self.nodes[1], self.nodes[GUI_NODE_INDEX]])

            txid = gui_wallet.listtransactions(count=1)[0]["txid"]
            self.log.info("GUI broadcast transaction %s", txid)
            self.wait_until(lambda: txid in self.nodes[0].getrawmempool(), timeout=30)

            self.generate(self.nodes[0], 1)
            self.sync_blocks([self.nodes[0], self.nodes[1], self.nodes[GUI_NODE_INDEX]])
            self.wait_until(
                lambda: recipient_wallet.gettransaction(txid)["confirmations"] > 0,
                timeout=30,
            )
            assert_equal(recipient_wallet.getreceivedbyaddress(recipient_address, 1), amount)
            self.capture_screenshot(gui, "transaction_confirmed", window="mainWindow")
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    WalletSendGuiTest(__file__).main()
