// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/importdescriptorstests.h>
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
#include <QSpinBox>

using wallet::AddWallet;
using wallet::CreateMockWalletDatabase;
using wallet::CWallet;
using wallet::FEATURE_HD_SPLIT;
using wallet::FEATURE_LATEST;
using wallet::RemoveWallet;
using wallet::WALLET_FLAG_BLANK_WALLET;
using wallet::WALLET_FLAG_DESCRIPTORS;
using wallet::WALLET_FLAG_DISABLE_PRIVATE_KEYS;
using wallet::WalletContext;

struct ImportDescriptorTestData
{
    QString desc;
    QString label;
    int64_t index = 0;
    int64_t range_start = 0;
    int64_t range_end = 0;
    int64_t timestamp = 0;
    bool active = false;
    bool internal = false;
};

struct KeypoolInfo
{
    int64_t keypoolsize = 0;
    int64_t keypoolsize_hd_internal = 0;
};

void EditDescriptorAndSubmit(WalletModel &walletModel, const ImportDescriptorTestData& data, QString expected_msg)
{
    QString warning_text;

    ImportDialog dialog(ImportDialog::importDescriptors, &walletModel);

    dialog.findChild<QLineEdit*>("descriptor")->setText(data.desc);
    dialog.findChild<QLineEdit*>("label")->setText(data.label);
    dialog.findChild<QLineEdit*>("timestamp")->setText(QString::number(data.timestamp));
    dialog.findChild<QCheckBox*>("checkboxActive")->setChecked(data.active);
    dialog.findChild<QCheckBox*>("checkboxInternal")->setChecked(data.internal);
    dialog.findChild<QSpinBox*>("nextIndex")->setValue(data.index);
    dialog.findChild<QSpinBox*>("startRange")->setValue(data.range_start);
    dialog.findChild<QSpinBox*>("endRange")->setValue(data.range_end);

    ConfirmMessage(&warning_text, 5ms);
    dialog.accept();
    QCOMPARE(warning_text, expected_msg);
}

void TestImportDescriptors(interfaces::Node& node)
{
    auto get_keypool_info = [](CWallet* wallet) {
        KeypoolInfo keypoolInfo;

        LOCK(wallet->cs_wallet);
        size_t kpExternalSize = wallet->KeypoolCountExternalKeys();
        keypoolInfo.keypoolsize = kpExternalSize;

        if (wallet->CanSupportFeature(FEATURE_HD_SPLIT)) {
            keypoolInfo.keypoolsize_hd_internal = (int64_t)(wallet->GetKeyPoolSize() - kpExternalSize);
        }

        return keypoolInfo;
    };

    // Set up wallet and chain with 105 blocks.
    TestChain100Setup test;
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    node.setContext(&test.m_node);

    const std::shared_ptr<CWallet> w1 = std::make_shared<CWallet>(node.context()->chain.get(), "", CreateMockWalletDatabase());
    w1->LoadWallet();
    w1->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    w1->SetWalletFlag(WALLET_FLAG_DISABLE_PRIVATE_KEYS);
    w1->SetWalletFlag(WALLET_FLAG_BLANK_WALLET);
    QCOMPARE(get_keypool_info(w1.get()).keypoolsize, 0);

    const std::shared_ptr<CWallet> wpriv = std::make_shared<CWallet>(node.context()->chain.get(), "", CreateMockWalletDatabase());
    wpriv->LoadWallet();
    wpriv->SetMinVersion(FEATURE_LATEST);
    wpriv->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    wpriv->SetWalletFlag(WALLET_FLAG_BLANK_WALLET);
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 0);

    {
        LOCK2(w1->cs_wallet, ::cs_main);
        w1->SetLastBlockProcessed(105, node.context()->chainman->ActiveChain().Tip()->GetBlockHash());
    }

    {
        LOCK2(wpriv->cs_wallet, ::cs_main);
        wpriv->SetLastBlockProcessed(105, node.context()->chainman->ActiveChain().Tip()->GetBlockHash());
    }

    // Initialize relevant QT models.
    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    OptionsModel optionsModel(node);
    bilingual_str error;
    QVERIFY(optionsModel.Init(error));
    ClientModel clientModel(node, &optionsModel);
    WalletContext& context = *node.walletLoader().context();

    // w1 wallet
    AddWallet(context, w1);
    WalletModel walletModelW1(interfaces::MakeWallet(context, w1), clientModel, platformStyle.get());
    RemoveWallet(context, w1,  std::nullopt);

    // wpriv wallet
    AddWallet(context, wpriv);
    WalletModel walletModelWpriv(interfaces::MakeWallet(context, wpriv), clientModel, platformStyle.get());
    RemoveWallet(context, wpriv, std::nullopt);

    // Test import fails if no descriptor present
    // Import should fail if a descriptor is not provided
    ImportDescriptorTestData data;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Descriptor not found."));

    // Test importing of a P2PKH descriptor
    Key key;
    key = BuildAddress(w1.get());
    // Should import a p2pkh descriptor
    data = ImportDescriptorTestData();
    std::string desc_str = "pkh(" + key.pubkey.toStdString() + ")";
    data.desc = DescsumCreate(desc_str);
    data.label = "Descriptor import test";
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded"));

    AddressInfo addressInfo;
    GetAddressInfo(w1.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.label, "Descriptor import test");
    QCOMPARE(get_keypool_info(w1.get()).keypoolsize, 0);

    // Test can import same descriptor with public key twice
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded"));

    // Test can update descriptor label
    data.label = "Updated label";
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded"));
    GetAddressInfo(w1.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.label, "Updated label");

    // Internal addresses cannot have labels
    data.internal = true;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Internal addresses should not have a label"));

    // Internal addresses should be detected as such
    key = BuildAddress(w1.get());
    data = ImportDescriptorTestData();
    desc_str = "pkh(" + key.pubkey.toStdString() + ")";
    data.desc = DescsumCreate(desc_str);
    data.internal = true;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded"));
    GetAddressInfo(w1.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.ischange, true);

    // Test importing of a P2SH-P2WPKH descriptor
    // Should not import a p2sh-p2wpkh descriptor without checksum
    key = BuildAddress(w1.get());
    data = ImportDescriptorTestData();
    desc_str = "sh(wpkh(" + key.pubkey.toStdString() + "))";
    data.desc = QString::fromStdString(desc_str);
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Missing checksum"));

    // Should not import a p2sh-p2wpkh descriptor that has range specified
    data = ImportDescriptorTestData();
    desc_str = "sh(wpkh(" + key.pubkey.toStdString() + "))";
    data.desc = DescsumCreate(desc_str);
    data.range_end = 1;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Range should not be specified for an un-ranged descriptor"));

    // Should not import a p2sh-p2wpkh descriptor and have it set to active
    data = ImportDescriptorTestData();
    desc_str = "sh(wpkh(" + key.pubkey.toStdString() + "))";
    data.desc = DescsumCreate(desc_str);
    data.active = true;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Active descriptors must be ranged"));

    // Should import a (non-active) p2sh-p2wpkh descriptor
    data = ImportDescriptorTestData();
    desc_str = "sh(wpkh(" + key.pubkey.toStdString() + "))";
    data.desc = DescsumCreate(desc_str);
    data.active = false;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(w1.get()).keypoolsize, 0);
    GetAddressInfo(w1.get(), addressInfo, key.p2sh_p2wpkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);

    // Test importing of a multisig descriptor
    Key key1 = BuildAddress(w1.get());
    Key key2 = BuildAddress(w1.get());
    // Should import a 1-of-2 bare multisig from descriptor
    data = ImportDescriptorTestData();
    desc_str = "multi(1," + key1.pubkey.toStdString() + "," + key2.pubkey.toStdString() + ")";
    data.desc = DescsumCreate(desc_str);
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded"));
    // Should not treat individual keys from the imported bare multisig as watchonly
    GetAddressInfo(w1.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, false);

    // Test ranged descriptors
    std::string xpriv = "tprv8ZgxMBicQKsPeuVhWwi6wuMQGfPKi9Li5GtX35jVNknACgqe3CY4g5xgkfDDJcmtF7o1QnxWDRYw4H5P26PXq7sbcUkEqeR4fg3Kxp2tigg";
    std::string xpub = "tpubD6NzVbkrYhZ4YNXVQbNhMK1WqguFsUXceaVJKbmno2aZ3B6QfbMeraaYvnBSGpV3vxLyTTK9DYT1yoEck4XUScMzXoQ2U2oSmE2JyMedq3H";
    std::vector<QString> addresses = {"2N7yv4p8G8yEaPddJxY41kPihnWvs39qCMf", "2MsHxyb2JS3pAySeNUsJ7mNnurtpeenDzLA", // hdkeypath=m/0'/0'/0' and 1'
                                      "bcrt1qrd3n235cj2czsfmsuvqqpr3lu6lg0ju7scl8gn", "bcrt1qfqeppuvj0ww98r6qghmdkj70tv8qpchehegrg8"}; // wpkh subscripts corresponding to the above addresses
    std::string desc = "sh(wpkh(" + xpub + "/0/0/*" + "))";

    // Ranged descriptors cannot have labels
    data = ImportDescriptorTestData();
    data.desc = DescsumCreate(desc);
    data.range_start = 0;
    data.range_end = 100;
    data.label = "test";
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Ranged descriptors should not have a label"));

    // Private keys required for private keys enabled wallet
    data = ImportDescriptorTestData();
    data.desc = DescsumCreate(desc);
    data.range_start = 0;
    data.range_end = 100;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Error: Cannot import descriptor without private keys to a wallet with private keys enabled"));

    // Ranged descriptor import should warn without a specified range
    data = ImportDescriptorTestData();
    data.desc = DescsumCreate(desc);
    EditDescriptorAndSubmit(walletModelW1, data, QString("Import Succeeded\nWarnings: Range not given, using default keypool range\n"));
    QCOMPARE(get_keypool_info(w1.get()).keypoolsize, 0);

    // Test importing of a ranged descriptor with xpriv
    // Should not import a ranged descriptor that includes xpriv into a watch-only wallet
    data = ImportDescriptorTestData();
    desc = "sh(wpkh(" + xpriv + "/0'/0'/*'" + "))";
    data.desc = DescsumCreate(desc);
    data.range_end = 1;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Cannot import private keys to a wallet with private keys disabled"));

    // Should not import a descriptor with hardened derivations when private keys are disabled
    data = ImportDescriptorTestData();
    desc = "wpkh(" + xpub + "/1h/*)";
    data.desc = DescsumCreate(desc);
    data.range_end = 1;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Cannot expand descriptor. Probably because of hardened derivations without private keys provided"));

    for (const auto& address: addresses) {
        GetAddressInfo(w1.get(), addressInfo, address);
        QCOMPARE(addressInfo.ismine, false);
        QCOMPARE(addressInfo.solvable, false);
    }

    // GUI only accepts range 0 to 0x7FFFFFFF
    data = ImportDescriptorTestData();
    desc = "sh(wpkh(" + xpriv + "/0'/0'/*'" + "))";
    data.desc = DescsumCreate(desc);

    data.range_start = 2;
    data.range_end = 1;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Range specified as [begin,end] must not have begin after end"));

    data.range_start = 0;
    data.range_end = 1000001;
    EditDescriptorAndSubmit(walletModelW1, data, QString("Error: Range is too large"));

    // Verify we can only extend descriptor's range
    data.range_start = 5;
    data.range_end = 1010;
    data.active = true;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 1006);
    data.range_start = 0;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 1011);
    data.range_end = 1020;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 1021);

    // Can keep range the same
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 1021);

    data.range_start = 5;
    data.range_end = 1010;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Error: new range must include current range = [0,1020]"));
    data.range_start = 0;
    data.range_end = 1010;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Error: new range must include current range = [0,1020]"));
    data.range_start = 5;
    data.range_end = 1020;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Error: new range must include current range = [0,1020]"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 1021);

    // Check we can change descriptor internal flag
    data.range_start = 0;
    data.range_end = 1020;
    data.internal = true;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 0);
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize_hd_internal, 1021);

    data.internal = false;
    EditDescriptorAndSubmit(walletModelWpriv, data, QString("Import Succeeded"));
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize, 1021);
    QCOMPARE(get_keypool_info(wpriv.get()).keypoolsize_hd_internal, 0);
}

void ImportDescriptorsTests::importDescriptorsTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        // Disable for mac on "minimal" platform to avoid crashes inside the Qt
        // framework when it tries to look up unimplemented cocoa functions,
        // and fails to handle returned nulls
        // (https://bugreports.qt.io/browse/QTBUG-49686).
        QWARN("Skipping ImportDescriptorsTests on mac build with 'minimal' platform set due to Qt bugs. To run AppTests, invoke "
              "with 'QT_QPA_PLATFORM=cocoa test_bitcoin-qt' on mac, or else use a linux or windows build.");
        return;
    }
#endif
    TestImportDescriptors(m_node);
}
