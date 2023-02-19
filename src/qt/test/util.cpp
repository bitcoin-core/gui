// Copyright (c) 2018-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/util.h>

#include <chrono>
#include <key_io.h>
#include <wallet/receive.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QTimer>
#include <QWidget>

void ConfirmMessage(QString* text, std::chrono::milliseconds msec)
{
    QTimer::singleShot(msec, [text]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->inherits("QMessageBox")) {
                QMessageBox* messageBox = qobject_cast<QMessageBox*>(widget);
                if (text) *text = messageBox->text();
                messageBox->defaultButton()->click();
            }
        }
    });
}

std::pair<bool, int> DescribeAddressTest(const CTxDestination& dest, const SigningProvider* provider)
{
    return std::visit(DescribeAddressVisitorTest(provider), dest);
}

QString DescsumCreate(std::string& descriptor) {
    return QString::fromStdString(descriptor + "#" + GetDescriptorChecksum(descriptor));
}

Key BuildAddress(wallet::CWallet* wallet) {
    Key key;
    key.key.MakeNewKey(true);
    key.dest = GetDestinationForKey(key.key.GetPubKey(), wallet->m_default_address_type);
    key.privkey = QString::fromStdString(EncodeSecret(key.key));
    key.pubkey = QString::fromStdString(HexStr(key.key.GetPubKey()));
    key.p2pkh_addr = QString::fromStdString(EncodeDestination(PKHash(key.key.GetPubKey())));
    key.p2pkh_script = QString::fromStdString(HexStr(GetScriptForDestination(PKHash(key.key.GetPubKey()))));
    key.p2wpkh_addr = QString::fromStdString(EncodeDestination(WitnessV0KeyHash(key.key.GetPubKey())));
    key.p2wpkh_script = QString::fromStdString(HexStr(GetScriptForDestination(WitnessV0KeyHash(key.key.GetPubKey()))));
    key.p2sh_p2wpkh_addr = QString::fromStdString(EncodeDestination(ScriptHash(GetScriptForDestination(WitnessV0KeyHash(key.key.GetPubKey())))));
    key.p2sh_p2wpkh_script = QString::fromStdString(HexStr(GetScriptForDestination(ScriptHash(GetScriptForDestination(WitnessV0KeyHash(key.key.GetPubKey()))))));
    key.p2sh_p2wpkh_redeem_script = QString::fromStdString(HexStr(GetScriptForDestination(WitnessV0KeyHash(key.key.GetPubKey()))));
    return key;
}

void GetAddressInfo(wallet::CWallet* wallet, AddressInfo& addressInfo, QString input) {
    addressInfo = AddressInfo();

    LOCK(wallet->cs_wallet);

    CTxDestination dest = DecodeDestination(input.toStdString());

    // Make sure the destination is valid
    QVERIFY2(IsValidDestination(dest), "Invalid address");

    wallet::isminetype mine = wallet->IsMine(dest);
    addressInfo.ismine = bool(mine & wallet::ISMINE_SPENDABLE);
    addressInfo.iswatchonly = bool(mine & wallet::ISMINE_WATCH_ONLY);
    CScript scriptPubKey = GetScriptForDestination(dest);
    addressInfo.ischange = ScriptIsChange(*wallet, scriptPubKey);
    std::unique_ptr<SigningProvider> provider = wallet->GetSolvingProvider(scriptPubKey);
    std::pair<bool, int> addressDescription = DescribeAddressTest(dest, provider.get());
    addressInfo.isscript = addressDescription.first;
    addressInfo.sigsrequired = addressDescription.second;

    if (provider) {
        auto inferred = InferDescriptor(scriptPubKey, *provider);
        bool solvable = inferred->IsSolvable();
        addressInfo.solvable = solvable;
    } else {
        addressInfo.solvable = false;
    }

    const auto& spk_mans = wallet->GetScriptPubKeyMans(scriptPubKey);
    // In most cases there is only one matching ScriptPubKey manager and we can't resolve ambiguity in a better way
    wallet::ScriptPubKeyMan* spk_man{nullptr};
    if (spk_mans.size()) spk_man = *spk_mans.begin();

    if (spk_man) {
        if (const std::unique_ptr<wallet::CKeyMetadata> meta = spk_man->GetMetadata(dest)) {
            addressInfo.timestamp = meta->nCreateTime;
        }
    }

    const auto* address_book_entry = wallet->FindAddressBookEntry(dest);
    if (address_book_entry) {
        addressInfo.label = address_book_entry->GetLabel();
    }
}

