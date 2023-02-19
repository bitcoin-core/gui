// Copyright (c) 2018-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_UTIL_H
#define BITCOIN_QT_TEST_UTIL_H

#include <chrono>

#include <qglobal.h>

#include <script/standard.h>
#include <script/signingprovider.h>
#include <QString>
#include <qt/walletmodel.h>

/**
 * Press "Ok" button in message box dialog.
 *
 * @param text - Optionally store dialog text.
 * @param msec - Number of milliseconds to pause before triggering the callback.
 */
void ConfirmMessage(QString* text, std::chrono::milliseconds msec);

class DescribeAddressVisitorTest
{
public:
    const SigningProvider * const provider;

    int ProcessSubScript(const CScript& subscript) const
    {
        // Always present: script type and redeemscript
        std::vector<std::vector<unsigned char>> solutions_data;
        TxoutType which_type = Solver(subscript, solutions_data);

        if (which_type == TxoutType::MULTISIG) {
            return solutions_data[0][0];
        }
        return -1;
    }

    explicit DescribeAddressVisitorTest(const SigningProvider* _provider) : provider(_provider) {};

    std::pair<bool, int> operator()(const CNoDestination& dest) const
    {
        return std::make_pair(false, -1);
    }

    std::pair<bool, int> operator()(const PKHash& keyID) const
    {
        return std::make_pair(false, -1);
    }

    std::pair<bool, int> operator()(const ScriptHash& scripthash) const
    {
        CScriptID scriptID(scripthash);
        CScript subscript;
        int sigsrequired = -1;
        if (provider && provider->GetCScript(scriptID, subscript)) {
            sigsrequired = ProcessSubScript(subscript);
        }
        return std::make_pair(true, sigsrequired);
    }

    std::pair<bool, int> operator()(const WitnessV0KeyHash& id) const
    {
        return std::make_pair(false, -1);
    }

    std::pair<bool, int> operator()(const WitnessV0ScriptHash& id) const
    {
        CScript subscript;
        CRIPEMD160 hasher;
        uint160 hash;
        hasher.Write(id.begin(), 32).Finalize(hash.begin());
        int sigsrequired = -1;
        if (provider && provider->GetCScript(CScriptID(hash), subscript)) {
            sigsrequired = ProcessSubScript(subscript);
        }
        return std::make_pair(true, sigsrequired);
    }

    std::pair<bool, int> operator()(const WitnessV1Taproot& tap) const
    {
        return std::make_pair(true, -1);
    }

    std::pair<bool, int> operator()(const WitnessUnknown& id) const
    {
        return std::make_pair(false, -1);
    }
};

struct Key
{
    CKey key;
    CTxDestination dest;
    QString privkey;
    QString pubkey;
    QString p2pkh_addr;
    QString p2pkh_script;
    QString p2wpkh_addr;
    QString p2wpkh_script;
    QString p2sh_p2wpkh_addr;
    QString p2sh_p2wpkh_script;
    QString p2sh_p2wpkh_redeem_script;
};

struct AddressInfo
{
    bool ischange = false;
    bool ismine = false;
    bool isscript = false;
    bool iswatchonly = false;
    std::string label;
    int sigsrequired = -1;
    bool solvable = false;
    int64_t timestamp = 0;
};

std::pair<bool, int> DescribeAddressTest(const CTxDestination& dest, const SigningProvider* provider);
QString DescsumCreate(std::string& descriptor);
Key BuildAddress(wallet::CWallet* wallet);
void GetAddressInfo(wallet::CWallet* wallet, AddressInfo& addressInfo, QString input);

#endif // BITCOIN_QT_TEST_UTIL_H
