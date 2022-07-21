#!/usr/bin/env python3
# Copyright (c) 2014-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listaddresses RPC."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal
)
from random import randrange

class ListAddressesTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

        self.extra_args = [
            ['-keypool=5']
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def test_addresses(self, wallet, address_type = None):

        self.generatetoaddress(self.nodes[0], 1, wallet.getnewaddress('', address_type))

        self.generate(self.nodes[0], 120)

        wallet.sendtoaddress("bcrt1qs758ursh4q9z627kt3pp5yysm78ddny6txaqgw", 2)

        if (address_type == None):
            list_addresses = wallet.listaddresses()
        else:
            list_addresses = wallet.listaddresses(options={"address_type": address_type})

        assert_equal(list_addresses[0]['index'], 0)
        assert_equal(list_addresses[0]['tx_count'], 2)
        if (address_type == None):
            assert_equal(list_addresses[0]['output_type'], "bech32")
        else:
            assert_equal(list_addresses[0]['output_type'], address_type)
        assert_equal(list_addresses[0]['internal'], False)

        if (self.options.descriptors):
            assert_equal(list_addresses[0]['hdkeypath'], "m/84'/1'/0'/0/0")
        else:
            assert_equal(list_addresses[0]['hdkeypath'], "m/0'/0'/0'")


    def run_test(self):

        print(self.options.descriptors)

        self.nodes[0].createwallet(wallet_name="test", descriptors=self.options.descriptors)

        wallet = self.nodes[0].get_wallet_rpc("test")

        self.test_addresses(wallet)

        # not working due to legacy wallet behavior

        # self.test_addresses(wallet, "legacy")
        # self.test_addresses(wallet, "p2sh-segwit")
        # self.test_addresses(wallet, "bech32")

if __name__ == '__main__':
    ListAddressesTest().main()
