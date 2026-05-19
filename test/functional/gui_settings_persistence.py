#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Settings persistence test via bitcoin-qt and the test bridge."""

from test_framework.qt import QtWidgetsTestFramework


class SettingsPersistenceGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def run_test(self):
        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
            gui.click("sendCoinsAction", window="mainWindow")
            gui.wait_for_view("SendCoinsDialog", window="mainWindow", timeout_ms=10000)
            self.wait_until(
                lambda: gui.get_property("frameCoinControl", "visible", window="mainWindow") is False,
                timeout=10,
            )
            self.capture_screenshot(gui, "coin_control_disabled", window="mainWindow")

            gui.click("optionsAction", window="mainWindow")
            gui.wait_for_window("OptionsDialog", timeout_ms=10000)
            if not gui.get_property("coinControlFeatures", "checked", window="OptionsDialog"):
                gui.click("coinControlFeatures", window="OptionsDialog")
            gui.click("okButton", window="OptionsDialog")
            self.wait_until(
                lambda: gui.get_property("frameCoinControl", "visible", window="mainWindow") is True,
                timeout=10,
            )
            self.capture_screenshot(gui, "coin_control_enabled", window="mainWindow")

            gui = self.restart_gui(gui)
            self.wait_for_main_window(gui)
            gui.click("sendCoinsAction", window="mainWindow")
            gui.wait_for_view("SendCoinsDialog", window="mainWindow", timeout_ms=10000)
            self.wait_until(
                lambda: gui.get_property("frameCoinControl", "visible", window="mainWindow") is True,
                timeout=10,
            )

            gui.click("optionsAction", window="mainWindow")
            gui.wait_for_window("OptionsDialog", timeout_ms=10000)
            assert gui.get_property("coinControlFeatures", "checked", window="OptionsDialog")
            self.capture_screenshot(gui, "coin_control_persisted", window="OptionsDialog")
            gui.click("cancelButton", window="OptionsDialog")
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    SettingsPersistenceGuiTest(__file__).main()
