// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <key_io.h>
#include <pubkey.h>
#include <script/descriptor.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util/descriptor.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

//! The converter of mocked descriptors, needs to be initialized when the target is.
MockedDescriptorConverter MOCKED_DESC_CONVERTER;

/** Test a successfully parsed descriptor. */
static void TestDescriptor(const Descriptor& desc, FlatSigningProvider& sig_provider, std::string& dummy)
{
    // Trivial helpers.
    (void)desc.IsRange();
    const bool is_solvable{desc.IsSolvable()};
    (void)desc.IsSingleType();
    (void)desc.GetOutputType();

    // Serialization to string representation.
    (void)desc.ToString();
    (void)desc.ToPrivateString(sig_provider, dummy);
    (void)desc.ToNormalizedString(sig_provider, dummy);

    // Serialization to Script.
    DescriptorCache cache;
    std::vector<CScript> out_scripts;
    (void)desc.Expand(0, sig_provider, out_scripts, sig_provider, &cache);
    (void)desc.ExpandPrivate(0, sig_provider, sig_provider);
    (void)desc.ExpandFromCache(0, cache, out_scripts, sig_provider);

    // If we could serialize to script we must be able to infer using the same provider.
    if (!out_scripts.empty()) {
        assert(InferDescriptor(out_scripts.back(), sig_provider));

        // The ScriptSize() must match the size of the serialized Script. (ScriptSize() is set for all descs but 'combo()'.)
        const bool is_combo{!desc.IsSingleType()};
        assert(is_combo || desc.ScriptSize() == out_scripts.back().size());
    }

    const auto max_sat_maxsig{desc.MaxSatisfactionWeight(true)};
    const auto max_sat_nonmaxsig{desc.MaxSatisfactionWeight(true)};
    const auto max_elems{desc.MaxSatisfactionElems()};
    // We must be able to estimate the max satisfaction size for any solvable descriptor (but combo).
    const bool is_nontop_or_nonsolvable{!is_solvable || !desc.GetOutputType()};
    const bool is_input_size_info_set{max_sat_maxsig && max_sat_nonmaxsig && max_elems};
    assert(is_input_size_info_set || is_nontop_or_nonsolvable);
}

void initialize_descriptor_parse()
{
    ECC_Start();
    SelectParams(ChainType::MAIN);
}

void initialize_mocked_descriptor_parse()
{
    initialize_descriptor_parse();
    MOCKED_DESC_CONVERTER.Init();
}

FUZZ_TARGET(mocked_descriptor_parse, .init = initialize_mocked_descriptor_parse)
{
    const std::string mocked_descriptor{buffer.begin(), buffer.end()};
    if (const auto descriptor = MOCKED_DESC_CONVERTER.GetDescriptor(mocked_descriptor)) {
        FlatSigningProvider signing_provider;
        std::string error;
        const auto desc = Parse(*descriptor, signing_provider, error);
        if (desc) TestDescriptor(*desc, signing_provider, error);
    }
}

FUZZ_TARGET(descriptor_parse, .init = initialize_descriptor_parse)
{
    const std::string descriptor(buffer.begin(), buffer.end());
    FlatSigningProvider signing_provider;
    std::string error;
    for (const bool require_checksum : {true, false}) {
        const auto desc = Parse(descriptor, signing_provider, error, require_checksum);
        if (desc) TestDescriptor(*desc, signing_provider, error);
    }
}
