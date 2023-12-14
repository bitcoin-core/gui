// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/txmempool.h>

#include <chainparams.h>
#include <node/context.h>
#include <node/mempool_args.h>
#include <txmempool.h>
#include <util/check.h>
#include <util/time.h>
#include <util/translation.h>
#include <validation.h>

using node::NodeContext;

CTxMemPool::Options MemPoolOptionsForTest(const NodeContext& node)
{
    CTxMemPool::Options mempool_opts{
        // Default to always checking mempool regardless of
        // chainparams.DefaultConsistencyChecks for tests
        .check_ratio = 1,
    };
    const auto result{ApplyArgsManOptions(*node.args, ::Params(), mempool_opts)};
    Assert(result);
    return mempool_opts;
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction& tx) const
{
    return FromTx(MakeTransactionRef(tx));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef& tx) const
{
    return CTxMemPoolEntry{tx, nFee, TicksSinceEpoch<std::chrono::seconds>(time), nHeight, m_sequence, spendsCoinbase, sigOpCost, lp};
}

std::optional<std::string> CheckPackageMempoolAcceptResult(const Package& txns,
                                                           const PackageMempoolAcceptResult& result,
                                                           bool expect_valid,
                                                           const CTxMemPool* mempool)
{
    if (expect_valid) {
        if (result.m_state.IsInvalid()) {
            return strprintf("Package validation unexpectedly failed: %s", result.m_state.ToString());
        }
    } else {
        if (result.m_state.IsValid()) {
            return strprintf("Package validation unexpectedly succeeded. %s", result.m_state.ToString());
        }
    }
    if (result.m_state.GetResult() != PackageValidationResult::PCKG_POLICY && txns.size() != result.m_tx_results.size()) {
        return strprintf("txns size %u does not match tx results size %u", txns.size(), result.m_tx_results.size());
    }
    for (const auto& tx : txns) {
        const auto& wtxid = tx->GetWitnessHash();
        if (result.m_tx_results.count(wtxid) == 0) {
            return strprintf("result not found for tx %s", wtxid.ToString());
        }

        const auto& atmp_result = result.m_tx_results.at(wtxid);
        const bool valid{atmp_result.m_result_type == MempoolAcceptResult::ResultType::VALID};
        if (expect_valid && atmp_result.m_state.IsInvalid()) {
            return strprintf("tx %s unexpectedly failed: %s", wtxid.ToString(), atmp_result.m_state.ToString());
        }

        //m_replaced_transactions should exist iff the result was VALID
        if (atmp_result.m_replaced_transactions.has_value() != valid) {
            return strprintf("tx %s result should %shave m_replaced_transactions",
                                    wtxid.ToString(), valid ? "" : "not ");
        }

        // m_vsize and m_base_fees should exist iff the result was VALID or MEMPOOL_ENTRY
        const bool mempool_entry{atmp_result.m_result_type == MempoolAcceptResult::ResultType::MEMPOOL_ENTRY};
        if (atmp_result.m_base_fees.has_value() != (valid || mempool_entry)) {
            return strprintf("tx %s result should %shave m_base_fees", wtxid.ToString(), valid || mempool_entry ? "" : "not ");
        }
        if (atmp_result.m_vsize.has_value() != (valid || mempool_entry)) {
            return strprintf("tx %s result should %shave m_vsize", wtxid.ToString(), valid || mempool_entry ? "" : "not ");
        }

        // m_other_wtxid should exist iff the result was DIFFERENT_WITNESS
        const bool diff_witness{atmp_result.m_result_type == MempoolAcceptResult::ResultType::DIFFERENT_WITNESS};
        if (atmp_result.m_other_wtxid.has_value() != diff_witness) {
            return strprintf("tx %s result should %shave m_other_wtxid", wtxid.ToString(), diff_witness ? "" : "not ");
        }

        // m_effective_feerate and m_wtxids_fee_calculations should exist iff the result was valid
        // or if the failure was TX_RECONSIDERABLE
        const bool valid_or_reconsiderable{atmp_result.m_result_type == MempoolAcceptResult::ResultType::VALID ||
                    atmp_result.m_state.GetResult() == TxValidationResult::TX_RECONSIDERABLE};
        if (atmp_result.m_effective_feerate.has_value() != valid_or_reconsiderable) {
            return strprintf("tx %s result should %shave m_effective_feerate",
                                    wtxid.ToString(), valid ? "" : "not ");
        }
        if (atmp_result.m_wtxids_fee_calculations.has_value() != valid_or_reconsiderable) {
            return strprintf("tx %s result should %shave m_effective_feerate",
                                    wtxid.ToString(), valid ? "" : "not ");
        }

        if (mempool) {
            // The tx by txid should be in the mempool iff the result was not INVALID.
            const bool txid_in_mempool{atmp_result.m_result_type != MempoolAcceptResult::ResultType::INVALID};
            if (mempool->exists(GenTxid::Txid(tx->GetHash())) != txid_in_mempool) {
                return strprintf("tx %s should %sbe in mempool", wtxid.ToString(), txid_in_mempool ? "" : "not ");
            }
            // Additionally, if the result was DIFFERENT_WITNESS, we shouldn't be able to find the tx in mempool by wtxid.
            if (tx->HasWitness() && atmp_result.m_result_type == MempoolAcceptResult::ResultType::DIFFERENT_WITNESS) {
                if (mempool->exists(GenTxid::Wtxid(wtxid))) {
                    return strprintf("wtxid %s should not be in mempool", wtxid.ToString());
                }
            }
        }
    }
    return std::nullopt;
}
