// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/importlegacytests.h>
#include <qt/test/util.h>
#include <test/util/setup_common.h>

#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <qt/bitcoinamountfield.h>
#include <qt/importdialog.h>
#include <qt/clientmodel.h>
#include <qt/editaddressdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <key_io.h>
#include <script/standard.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>
#include <wallet/test/util.h>
#include <walletinitinterface.h>

#include <chrono>

#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QString>

using wallet::AddWallet;
using wallet::CreateMockWalletDatabase;
using wallet::CWallet;
using wallet::RemoveWallet;
using wallet::WalletContext;

struct ImportKeyData
{
    QString key;
    QString label;
    bool rescan = true;
    bool p2sh = false;
};

void EditKeyAndSubmit(ImportDialog* dialog, ImportDialog::Page _page, const ImportKeyData& data, QString expected_msg) {
    QString warning_text;

    dialog->findChild<QLineEdit*>("keyEditIKP")->setText(data.key);
    dialog->findChild<QLineEdit*>("labelEditIKP")->setText(data.label);
    dialog->findChild<QCheckBox*>("rescanButtonIKP")->setChecked(data.rescan);
    if (_page == ImportDialog::importAddress) {
        dialog->findChild<QCheckBox*>("p2shButtonIKP")->setChecked(data.p2sh);
    }

    ConfirmMessage(&warning_text, 5ms);
    dialog->accept();
    QCOMPARE(warning_text, expected_msg);
}

void TestImportLegacy(interfaces::Node& node)
{
    // Set up wallet and chain with 105 blocks.
    TestChain100Setup test;
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    node.setContext(&test.m_node);
    const std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(node.context()->chain.get(), "", CreateMockWalletDatabase());
    {
        LOCK2(wallet->cs_wallet, ::cs_main);
        wallet->SetLastBlockProcessed(105, node.context()->chainman->ActiveChain().Tip()->GetBlockHash());
    }

    wallet->LoadWallet();
    wallet->SetupLegacyScriptPubKeyMan();

    // Initialize relevant QT models.
    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    OptionsModel optionsModel(node);
    bilingual_str error;
    QVERIFY(optionsModel.Init(error));
    ClientModel clientModel(node, &optionsModel);
    WalletContext& context = *node.walletLoader().context();
    AddWallet(context, wallet);
    WalletModel walletModel(interfaces::MakeWallet(context, wallet), clientModel, platformStyle.get());
    RemoveWallet(context, wallet, /* load_on_start= */ std::nullopt);

    ImportDialog importDialogPubkey(ImportDialog::importPubkey, &walletModel);
    ImportDialog importDialogPrivkey(ImportDialog::importPrivkey, &walletModel);
    ImportDialog importDialogAddress(ImportDialog::importAddress, &walletModel);

    // Public key
    Key key = BuildAddress(wallet.get());
    ImportKeyData data;
    data.key = key.pubkey;
    EditKeyAndSubmit(&importDialogPubkey, ImportDialog::importPubkey, data, QString("Import Succeeded"));

    AddressInfo addressInfo;
    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);

    // Public key + Label
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.pubkey;
    data.rescan = false;
    data.label = "Successful public key import";
    EditKeyAndSubmit(&importDialogPubkey, ImportDialog::importPubkey, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.label, "Successful public key import");

    // Public key + Rescan
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.pubkey;
    EditKeyAndSubmit(&importDialogPubkey, ImportDialog::importPubkey, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);

    // Public key must be hex string
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.p2pkh_addr;
    EditKeyAndSubmit(&importDialogPubkey, ImportDialog::importPubkey, data, QString("Pubkey must be a hex string"));

    // Public key not a valid public key
    data = ImportKeyData();
    data.key = "02e053f77836d086a6082f441fc0db6a71de0620f3e1c9797d80ade82979c73c";
    EditKeyAndSubmit(&importDialogPubkey, ImportDialog::importPubkey, data, QString("Pubkey is not a valid public key"));

    // Private key
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.privkey;
    EditKeyAndSubmit(&importDialogPrivkey, ImportDialog::importPrivkey, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, true);

    // Private key + Label
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.privkey;
    data.label = "Successful private key import";
    EditKeyAndSubmit(&importDialogPrivkey, ImportDialog::importPrivkey, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.label, "Successful private key import");

    // Private key not a valid public key
    data = ImportKeyData();
    data.key = "cSFUNyEbpxP6Jur5kJftYEugGzkd1QUjHwSwkjH5yco6z1oERCq";
    EditKeyAndSubmit(&importDialogPrivkey, ImportDialog::importPrivkey, data, QString("Invalid private key encoding."));

    // Address
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.p2pkh_addr;
    EditKeyAndSubmit(&importDialogAddress, ImportDialog::importAddress, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);

    // Address + Label
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.p2pkh_addr;
    data.label = "Successful address import";
    EditKeyAndSubmit(&importDialogAddress, ImportDialog::importAddress, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.label, "Successful address import");

    // Invalid Address or script
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.privkey;
    EditKeyAndSubmit(&importDialogAddress, ImportDialog::importAddress, data, QString("Invalid Bitcoin address or script"));

    // Invalid Address can't import Bech32m into a legacy wallet
    data = ImportKeyData();
    data.key = "bcrt1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqc8gma6";
    EditKeyAndSubmit(&importDialogAddress, ImportDialog::importAddress, data, QString("Bech32m addresses cannot be imported into legacy wallets"));

    // Invalid Address use the p2sh flag with an address
    key = BuildAddress(wallet.get());
    data = ImportKeyData();
    data.key = key.p2pkh_addr;
    data.p2sh = true;
    EditKeyAndSubmit(&importDialogAddress, ImportDialog::importAddress, data, QString("Cannot use the p2sh flag with an address - use a script instead"));
}

void ImportLegacyTests::importLegacyTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        // Disable for mac on "minimal" platform to avoid crashes inside the Qt
        // framework when it tries to look up unimplemented cocoa functions,
        // and fails to handle returned nulls
        // (https://bugreports.qt.io/browse/QTBUG-49686).
        QWARN("Skipping ImportLegacyTests on mac build with 'minimal' platform set due to Qt bugs. To run AppTests, invoke "
              "with 'QT_QPA_PLATFORM=cocoa test_bitcoin-qt' on mac, or else use a linux or windows build.");
        return;
    }
#endif
    TestImportLegacy(m_node);
}