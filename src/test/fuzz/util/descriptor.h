// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_FUZZ_UTIL_DESCRIPTOR_H
#define BITCOIN_TEST_FUZZ_UTIL_DESCRIPTOR_H

#include <key_io.h>
#include <util/strencodings.h>
#include <script/descriptor.h>

#include <functional>

/**
 * Converts a mocked descriptor string to a valid one. Every key in a mocked descriptor key is
 * represented by 2 hex characters preceded by the '%' character. We parse the two hex characters
 * as an index in a list of pre-generated keys. This list contains keys of the various types
 * accepted in descriptor keys expressions.
 */
class MockedDescriptorConverter {
private:
    //! Types are raw (un)compressed pubkeys, raw xonly pubkeys, raw privkeys (WIF), xpubs, xprvs.
    static constexpr uint8_t KEY_TYPES_COUNT{6};
    //! How many keys we'll generate in total.
    static constexpr size_t TOTAL_KEYS_GENERATED{std::numeric_limits<uint8_t>::max() + 1};
    //! 256 keys of various types.
    std::array<std::string, TOTAL_KEYS_GENERATED> keys_str;

public:
    // We derive the type of key to generate from the 1-byte id parsed from hex.
    bool IdIsCompPubKey(uint8_t idx) const { return idx % KEY_TYPES_COUNT == 0; }
    bool IdIsUnCompPubKey(uint8_t idx) const { return idx % KEY_TYPES_COUNT == 1; }
    bool IdIsXOnlyPubKey(uint8_t idx) const { return idx % KEY_TYPES_COUNT == 2; }
    bool IdIsConstPrivKey(uint8_t idx) const { return idx % KEY_TYPES_COUNT == 3; }
    bool IdIsXpub(uint8_t idx) const { return idx % KEY_TYPES_COUNT == 4; }
    bool IdIsXprv(uint8_t idx) const { return idx % KEY_TYPES_COUNT == 5; }

    //! When initializing the target, populate the list of keys.
    void Init();

    //! Parse an id in the keys vectors from a 2-characters hex string.
    std::optional<uint8_t> IdxFromHex(std::string_view hex_characters) const;

    //! Get an actual descriptor string from a descriptor string whose keys were mocked.
    std::optional<std::string> GetDescriptor(std::string_view mocked_desc) const;
};

#endif // BITCOIN_TEST_FUZZ_UTIL_DESCRIPTOR_H
