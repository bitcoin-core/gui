// Copyright (c) 2024-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_UTIL_SPEND_H
#define BITCOIN_WALLET_UTIL_SPEND_H

#include <policy/fees.h>
#include <wallet/transaction.h>

#include <optional>

struct CreatedTransactionResult
{
    CTransactionRef tx;
    CAmount fee;
    FeeCalculation fee_calc;
    std::optional<unsigned int> change_pos;

    CreatedTransactionResult(CTransactionRef _tx, CAmount _fee, std::optional<unsigned int> _change_pos, const FeeCalculation& _fee_calc)
            : tx(_tx), fee(_fee), fee_calc(_fee_calc), change_pos(_change_pos) {}
};

#endif // BITCOIN_WALLET_UTIL_SPEND_H
