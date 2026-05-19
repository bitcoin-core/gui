#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""End-to-end receive transaction test via bitcoin-qt and the test bridge."""

from decimal import Decimal

from test_framework.qt import GUI_NODE_INDEX, QtWidgetsTestFramework
from test_framework.util import assert_equal


class WalletReceiveGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def run_test(self):
        gui_wallet = self.nodes[GUI_NODE_INDEX].get_wallet_rpc(self.default_wallet_name)
        sender_wallet = self.nodes[1].get_wallet_rpc(self.default_wallet_name)
        amount = Decimal("0.75")
        label = "gui receive"
        message = "functional test payment"

        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
            self.capture_screenshot(gui, "main_window_ready", window="mainWindow")

            gui.click("receiveCoinsAction", window="mainWindow")
            gui.wait_for_view("ReceiveCoinsDialog", window="mainWindow", timeout_ms=10000)
            gui.set_text("reqLabel", label, window="mainWindow")
            gui.set_text("reqMessage", message, window="mainWindow")
            gui.set_text("reqAmount", str(amount), window="mainWindow")
            self.capture_screenshot(gui, "receive_form_filled", window="mainWindow")
            gui.click("receiveButton", window="mainWindow")

            gui.wait_for_window("ReceiveRequestDialog", timeout_ms=10000)
            gui.wait_for_property("address_content", "text", window="ReceiveRequestDialog", non_empty=True, timeout_ms=10000)
            address = gui.get_text("address_content", window="ReceiveRequestDialog")
            assert_equal(gui.get_text("label_content", window="ReceiveRequestDialog"), label)
            assert_equal(gui.get_text("message_content", window="ReceiveRequestDialog"), message)
            assert str(amount) in gui.get_text("amount_content", window="ReceiveRequestDialog")
            self.capture_screenshot(gui, "receive_request_dialog", window="ReceiveRequestDialog")

            txid = sender_wallet.sendtoaddress(address, amount)
            self.sync_mempools([self.nodes[0], self.nodes[1], self.nodes[GUI_NODE_INDEX]])
            self.generate(self.nodes[0], 1)
            self.sync_blocks([self.nodes[0], self.nodes[1], self.nodes[GUI_NODE_INDEX]])

            self.wait_until(lambda: gui_wallet.getreceivedbyaddress(address, 1) == amount, timeout=30)
            self.wait_until(lambda: gui_wallet.gettransaction(txid)["confirmations"] > 0, timeout=30)

            gui.click("historyAction", window="mainWindow")
            gui.wait_for_view("TransactionsPage", window="mainWindow", timeout_ms=10000)
            self.capture_screenshot(gui, "transaction_history_after_receive", window="mainWindow")
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    WalletReceiveGuiTest(__file__).main()
