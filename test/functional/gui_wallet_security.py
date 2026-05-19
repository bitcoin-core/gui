#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Wallet encryption and unlock-flow test via bitcoin-qt."""

from decimal import Decimal

from test_framework.qt import GUI_NODE_INDEX, QtWidgetsTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class WalletSecurityGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def run_test(self):
        gui_wallet = self.nodes[GUI_NODE_INDEX].get_wallet_rpc(self.default_wallet_name)
        recipient_address = self.nodes[1].get_wallet_rpc(self.default_wallet_name).getnewaddress()
        old_passphrase = "gui security old passphrase"
        new_passphrase = "gui security new passphrase"
        amount = Decimal("0.1")

        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
            gui.wait_for_property("encryptWalletAction", "enabled", window="mainWindow", value=True, timeout_ms=60000)
            gui.click("encryptWalletAction", window="mainWindow")
            gui.wait_for_window("AskPassphraseDialog", timeout_ms=10000)
            gui.set_text("passEdit2", old_passphrase, window="AskPassphraseDialog")
            gui.set_text("passEdit3", old_passphrase, window="AskPassphraseDialog")
            gui.wait_for_property("passphraseOkButton", "enabled", window="AskPassphraseDialog", value=True, timeout_ms=5000)
            self.capture_screenshot(gui, "encrypt_wallet_dialog", window="AskPassphraseDialog")
            gui.click("passphraseOkButton", window="AskPassphraseDialog")

            gui.wait_for_window("encryptWalletConfirmDialog", timeout_ms=10000)
            self.capture_screenshot(gui, "encrypt_wallet_confirm", window="encryptWalletConfirmDialog")
            gui.click("encryptWalletContinueButton", window="encryptWalletConfirmDialog")

            gui.wait_for_window("walletEncryptedDialog", timeout_ms=10000)
            self.capture_screenshot(gui, "wallet_encrypted_message", window="walletEncryptedDialog")
            gui.click("walletEncryptedOkButton", window="walletEncryptedDialog")

            self.wait_until(
                lambda: gui.get_property("changePassphraseAction", "enabled", window="mainWindow") is True,
                timeout=10,
            )
            assert gui.get_property("encryptWalletAction", "checked", window="mainWindow")

            gui.click("changePassphraseAction", window="mainWindow")
            gui.wait_for_window("AskPassphraseDialog", timeout_ms=10000)
            gui.set_text("passEdit1", old_passphrase, window="AskPassphraseDialog")
            gui.set_text("passEdit2", new_passphrase, window="AskPassphraseDialog")
            gui.set_text("passEdit3", new_passphrase, window="AskPassphraseDialog")
            gui.wait_for_property("passphraseOkButton", "enabled", window="AskPassphraseDialog", value=True, timeout_ms=5000)
            self.capture_screenshot(gui, "change_passphrase_dialog", window="AskPassphraseDialog")
            gui.click("passphraseOkButton", window="AskPassphraseDialog")

            gui.wait_for_window("passphraseChangedDialog", timeout_ms=10000)
            self.capture_screenshot(gui, "passphrase_changed_message", window="passphraseChangedDialog")
            gui.click("passphraseChangedOkButton", window="passphraseChangedDialog")

            assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect", gui_wallet.walletpassphrase, old_passphrase, 1)
            gui_wallet.walletpassphrase(new_passphrase, 1)
            gui_wallet.walletlock()

            gui.click("sendCoinsAction", window="mainWindow")
            gui.wait_for_view("SendCoinsDialog", window="mainWindow", timeout_ms=10000)
            gui.set_text("payTo", recipient_address, window="mainWindow")
            gui.set_text("payAmount", str(amount), window="mainWindow")
            self.capture_screenshot(gui, "encrypted_send_form", window="mainWindow")
            gui.click("sendButton", window="mainWindow")

            gui.wait_for_window("AskPassphraseDialog", timeout_ms=10000)
            self.capture_screenshot(gui, "unlock_wallet_dialog", window="AskPassphraseDialog")
            gui.click("passphraseCancelButton", window="AskPassphraseDialog")
            self.wait_until(lambda: gui.get_active_window() == "mainWindow", timeout=10)
            assert_equal(self.nodes[0].getrawmempool(), [])
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    WalletSecurityGuiTest(__file__).main()
