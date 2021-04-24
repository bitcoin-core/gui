#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Anchors functionality"""

import os

from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


def check_node_connections(*, node, num_in, num_out):
    info = node.getnetworkinfo()
    assert_equal(info["connections_in"], num_in)
    assert_equal(info["connections_out"], num_out)


class AnchorsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        self.log.info("Add 2 block-relay-only connections to node 0")
        for i in range(2):
            self.log.debug(f"block-relay-only: {i}")
            self.nodes[0].add_outbound_p2p_connection(
                P2PInterface(), p2p_idx=i, connection_type="block-relay-only"
            )

        self.log.info("Add 5 inbound connections to node 0")
        for i in range(5):
            self.log.debug(f"inbound: {i}")
            self.nodes[0].add_p2p_connection(P2PInterface())

        self.log.info("Check node 0 connections")
        check_node_connections(node=self.nodes[0], num_in=5, num_out=2)

        # 127.0.0.1
        ip = "7f000001"

        # Since the ip is always 127.0.0.1 for this case,
        # we store only the port to identify the peers
        block_relay_nodes_port = []
        inbound_nodes_port = []
        for p in self.nodes[0].getpeerinfo():
            addr_split = p["addr"].split(":")
            if p["connection_type"] == "block-relay-only":
                block_relay_nodes_port.append(hex(int(addr_split[1]))[2:])
            else:
                inbound_nodes_port.append(hex(int(addr_split[1]))[2:])

        self.log.info("Stop node 0")
        self.stop_node(0)

        node0_anchors_path = os.path.join(
            self.nodes[0].datadir, "regtest", "anchors.dat"
        )

        # It should contain only the block-relay-only addresses
        self.log.info("Check the addresses in anchors.dat")

        with open(node0_anchors_path, "rb") as file_handler:
            anchors = file_handler.read().hex()

        for port in block_relay_nodes_port:
            ip_port = ip + port
            assert ip_port in anchors
        for port in inbound_nodes_port:
            ip_port = ip + port
            assert ip_port not in anchors

        self.log.info("Start node 0")
        self.start_node(0)

        self.log.info("When node starts, check if anchors.dat doesn't exist anymore")
        assert not os.path.exists(node0_anchors_path)


if __name__ == "__main__":
    AnchorsTest().main()
