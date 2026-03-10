#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Advanced send flow test covering PSBT creation via bitcoin-qt."""

from decimal import Decimal

from test_framework.qt import GUI_NODE_INDEX, QtWidgetsTestFramework
from test_framework.util import assert_equal


class WalletSendAdvancedGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def run_test(self):
        recipient_address = self.nodes[1].get_wallet_rpc(self.default_wallet_name).getnewaddress()
        amount = Decimal("0.5")
        amount_sats = int(amount * Decimal(100_000_000))

        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
            gui.click("optionsAction", window="mainWindow")
            gui.wait_for_window("OptionsDialog", timeout_ms=10000)
            if not gui.get_property("m_enable_psbt_controls", "checked", window="OptionsDialog"):
                gui.click("m_enable_psbt_controls", window="OptionsDialog")
            gui.click("okButton", window="OptionsDialog")

            gui.click("sendCoinsAction", window="mainWindow")
            gui.wait_for_view("SendCoinsDialog", window="mainWindow", timeout_ms=10000)
            gui.set_text("payTo", recipient_address, window="mainWindow")
            gui.set_text("payAmount", str(amount), window="mainWindow")
            self.wait_until(lambda: gui.get_property("payAmount", "value", window="mainWindow") == amount_sats, timeout=5)
            self.capture_screenshot(gui, "psbt_send_form", window="mainWindow")
            gui.click("sendButton", window="mainWindow")

            gui.wait_for_window("sendConfirmationDialog", timeout_ms=10000)
            gui.wait_for_property("createUnsignedButton", "enabled", window="sendConfirmationDialog", value=True, timeout_ms=10000)
            self.capture_screenshot(gui, "psbt_confirmation", window="sendConfirmationDialog")
            gui.click("createUnsignedButton", window="sendConfirmationDialog")

            gui.wait_for_window("psbt_copied_message", timeout_ms=10000)
            self.capture_screenshot(gui, "psbt_created", window="psbt_copied_message")
            assert_equal(self.nodes[0].getrawmempool(), [])
            gui.click("psbtDiscardButton", window="psbt_copied_message")
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    WalletSendAdvancedGuiTest(__file__).main()
