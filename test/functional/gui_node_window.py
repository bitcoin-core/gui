#!/usr/bin/env python3
# Copyright (c) 2026 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Node window smoke test via bitcoin-qt and the test bridge."""

from test_framework.qt import QtWidgetsTestFramework


class NodeWindowGuiTest(QtWidgetsTestFramework):
    def set_test_params(self):
        super().set_test_params()

    def run_test(self):
        gui = self.create_gui_driver()
        try:
            self.wait_for_main_window(gui)
            gui.wait_for_property("openRPCConsoleAction", "enabled", window="mainWindow", value=True, timeout_ms=60000)
            gui.click("openRPCConsoleAction", window="mainWindow")
            gui.wait_for_window("rpcConsole", timeout_ms=10000)
            gui.wait_for_property("clientVersion", "text", window="rpcConsole", non_empty=True, timeout_ms=10000)
            gui.wait_for_property("networkName", "text", window="rpcConsole", contains="regtest", timeout_ms=10000)
            gui.wait_for_property("numberOfConnections", "text", window="rpcConsole", non_empty=True, timeout_ms=10000)
            gui.wait_for_property("numberOfBlocks", "text", window="rpcConsole", contains="200", timeout_ms=10000)
            gui.wait_for_property("mempoolNumberTxs", "text", window="rpcConsole", non_empty=True, timeout_ms=10000)
            self.capture_screenshot(gui, "node_window", window="rpcConsole")
        except Exception:
            self.dump_gui_state(gui)
            raise
        finally:
            gui.close()


if __name__ == "__main__":
    NodeWindowGuiTest(__file__).main()
