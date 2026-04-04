#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Wallet create, close, and reload lifecycle test via bitcoin-qt."""

from test_framework.qt import GUI_NODE_INDEX, QtWidgetsTestFramework


class WalletLifecycleGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def get_gui_node_extra_args(self):
        return super().get_gui_node_extra_args() + ["-nowallet"]

    def should_import_gui_wallet(self):
        return False

    def run_test(self):
        wallet_name = "gui_lifecycle_wallet"

        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
            gui.wait_for_property("createWalletButton", "visible", window="mainWindow", value=True, timeout_ms=10000)
            self.capture_screenshot(gui, "no_wallet_view", window="mainWindow")

            gui.click("createWalletButton", window="mainWindow")
            gui.wait_for_window("CreateWalletDialog", timeout_ms=10000)
            gui.set_text("wallet_name_line_edit", wallet_name, window="CreateWalletDialog")
            gui.wait_for_property("createWalletOkButton", "enabled", window="CreateWalletDialog", value=True, timeout_ms=5000)
            self.capture_screenshot(gui, "create_wallet_dialog", window="CreateWalletDialog")
            gui.click("createWalletOkButton", window="CreateWalletDialog")

            self.wait_until(lambda: wallet_name in self.nodes[GUI_NODE_INDEX].listwallets(), timeout=30)
            gui.wait_for_view("OverviewPage", window="mainWindow", timeout_ms=30000)
            self.capture_screenshot(gui, "wallet_created", window="mainWindow")

            gui.wait_for_property("closeWalletAction", "enabled", window="mainWindow", value=True, timeout_ms=10000)
            gui.click("closeWalletAction", window="mainWindow")
            gui.wait_for_window("closeWalletConfirmDialog", timeout_ms=10000)
            gui.click("closeWalletConfirmButton", window="closeWalletConfirmDialog")
            self.wait_until(lambda: wallet_name not in self.nodes[GUI_NODE_INDEX].listwallets(), timeout=30)
            gui.wait_for_property("createWalletButton", "visible", window="mainWindow", value=True, timeout_ms=10000)
            self.capture_screenshot(gui, "wallet_closed", window="mainWindow")

            self.nodes[GUI_NODE_INDEX].loadwallet(wallet_name)
            self.wait_until(lambda: wallet_name in self.nodes[GUI_NODE_INDEX].listwallets(), timeout=30)
            gui.wait_for_view("OverviewPage", window="mainWindow", timeout_ms=30000)
            self.capture_screenshot(gui, "wallet_reloaded", window="mainWindow")
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    WalletLifecycleGuiTest(__file__).main()
