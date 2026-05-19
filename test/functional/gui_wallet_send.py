#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""End-to-end send transaction test via bitcoin-qt and the test bridge."""

from decimal import Decimal

from test_framework.qt import (
    GUI_NODE_INDEX,
    QtWidgetsTestFramework,
    parse_display_amount,
)
from test_framework.util import (
    assert_equal,
)


class WalletSendGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def run_test(self):
        gui_wallet = self.nodes[GUI_NODE_INDEX].get_wallet_rpc(self.default_wallet_name)
        recipient_wallet = self.nodes[1].get_wallet_rpc(self.default_wallet_name)
        amount = Decimal("1.25")
        amount_sats = int(amount * Decimal(100_000_000))
        recipient_address = recipient_wallet.getnewaddress()

        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
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
