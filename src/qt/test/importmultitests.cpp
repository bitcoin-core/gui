// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/importmultitests.h>
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
#include <QSpinBox>

using wallet::AddWallet;
using wallet::AvailableCoinsListUnspent;
using wallet::CCoinControl;
using wallet::CRecipient;
using wallet::CoinFilterParams;
using wallet::COutput;
using wallet::CreateMockWalletDatabase;
using wallet::CWallet;
using wallet::CWalletTx;
using wallet::RemoveWallet;
using wallet::TxStateConfirmed;
using wallet::WalletContext;
using wallet::WalletRescanReserver;

struct ImportMultiTestData
{
    QString desc;
    QString scriptPubKey;
    QString redeem_script;
    QString witness_script;
    QString label;
    QString public_keys;
    QString private_keys;
    int64_t range_start = 0;
    int64_t range_end = 0;
    int64_t timestamp = 0;
    bool internal = false;
    bool watch_only = false;
    bool keypool = false;
};

struct UnspentInfo
{
    bool spendable = false;
    bool solvable = false;
};

void EditMultiAndSubmit(ImportDialog* dialog, const ImportMultiTestData& data, QString expected_msg)
{
    QString warning_text;

    if (!data.scriptPubKey.isEmpty()) {
        dialog->findChild<QRadioButton*>("importScriptPubKeyRadio")->setChecked(true);
        dialog->findChild<QLineEdit*>("scriptPubKey")->setText(data.scriptPubKey);
        dialog->findChild<QLineEdit*>("redeemScript")->setText(data.redeem_script);
        dialog->findChild<QLineEdit*>("witnessScript")->setText(data.witness_script);
        dialog->findChild<QLineEdit*>("publicKey")->setText(data.public_keys);
        dialog->findChild<QLineEdit*>("privateKey")->setText(data.private_keys);
        dialog->findChild<QLineEdit*>("timestampMulti")->setText(QString::number(data.timestamp));
        dialog->findChild<QCheckBox*>("checkboxInternalMulti")->setChecked(data.internal);
        dialog->findChild<QCheckBox*>("checkboxKeyPoolMulti")->setChecked(data.keypool);
        dialog->findChild<QCheckBox*>("checkboxWatchOnlyMulti")->setChecked(data.watch_only);
        dialog->findChild<QLineEdit*>("labelMulti")->setText(data.label);
    } else {
        dialog->findChild<QRadioButton*>("importDescriptorRadio")->setChecked(true);
        dialog->findChild<QLineEdit*>("desccMulti")->setText(data.desc);
        dialog->findChild<QSpinBox*>("startRangeMulti")->setValue(data.range_start);
        dialog->findChild<QSpinBox*>("endRangeMulti")->setValue(data.range_end);
        dialog->findChild<QLineEdit*>("privateKey")->setText(data.private_keys);
        dialog->findChild<QLineEdit*>("timestampMulti")->setText(QString::number(data.timestamp));
        dialog->findChild<QCheckBox*>("checkboxInternalMulti")->setChecked(data.internal);
        dialog->findChild<QCheckBox*>("checkboxKeyPoolMulti")->setChecked(data.keypool);
        dialog->findChild<QCheckBox*>("checkboxWatchOnlyMulti")->setChecked(data.watch_only);
        dialog->findChild<QLineEdit*>("labelMulti")->setText(data.label);
    }

    ConfirmMessage(&warning_text, 5ms);
    dialog->accept();
    QCOMPARE(warning_text, expected_msg);
}

class TestImportMultiSetup : public TestChain100Setup
{
public:
    CWalletTx& AddTx(CWallet& wallet, CRecipient recipient, bool& error)
    {
        CTransactionRef tx;
        CCoinControl dummy;
        {
            constexpr int RANDOM_CHANGE_POSITION = -1;
            auto res = CreateTransaction(wallet, {recipient}, RANDOM_CHANGE_POSITION, dummy);
            if (!res) {
                error = true;
            }
            tx = res->tx;
        }
        wallet.CommitTransaction(tx, {}, {});
        CMutableTransaction blocktx;
        {
            LOCK(wallet.cs_wallet);
            blocktx = CMutableTransaction(*wallet.mapWallet.at(tx->GetHash()).tx);
        }
        CreateAndProcessBlock({CMutableTransaction(blocktx)}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

        LOCK(wallet.cs_wallet);
        LOCK(Assert(m_node.chainman)->GetMutex());
        wallet.SetLastBlockProcessed(wallet.GetLastBlockHeight() + 1, m_node.chainman->ActiveChain().Tip()->GetBlockHash());
        auto it = wallet.mapWallet.find(tx->GetHash());
        if (it == wallet.mapWallet.end()) {
            error = true;
        }
        it->second.m_state = TxStateConfirmed{m_node.chainman->ActiveChain().Tip()->GetBlockHash(), m_node.chainman->ActiveChain().Height(), /*index=*/1};
        return it->second;
    }
};

void TestImportMulti(interfaces::Node& node)
{
    // Set up wallet and chain with 105 blocks.
    TestImportMultiSetup test;
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    node.setContext(&test.m_node);
    const std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(node.context()->chain.get(), "", CreateMockWalletDatabase());
    {
        LOCK2(wallet->cs_wallet, ::cs_main);
        wallet->SetLastBlockProcessed(105, node.context()->chainman->ActiveChain().Tip()->GetBlockHash());
    }
    wallet->LoadWallet();
    {
        auto spk_man = wallet->GetOrCreateLegacyScriptPubKeyMan();
        LOCK2(wallet->cs_wallet, spk_man->cs_KeyStore);
        spk_man->AddKeyPubKey(test.coinbaseKey, test.coinbaseKey.GetPubKey());
    }

    {
        WalletRescanReserver reserver(*wallet);
        reserver.reserve();
        CWallet::ScanResult result = wallet->ScanForWalletTransactions(Params().GetConsensus().hashGenesisBlock, /*start_height=*/0, /*max_height=*/{}, reserver, /*fUpdate=*/true, /*save_progress=*/false);
        QCOMPARE(result.status, CWallet::ScanResult::SUCCESS);
        {
            LOCK(::cs_main);
            QCOMPARE(result.last_scanned_block, node.context()->chainman->ActiveChain().Tip()->GetBlockHash());
        }
        QVERIFY(result.last_failed_block.IsNull());
    }

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
    ImportDialog importDialog(ImportDialog::importMulti, &walletModel);

    auto get_multisig = [](int key_length = 3) {
        Key key;
        CKey k[3];
        for (int i = 0; i < 3; i++)
            k[i].MakeNewKey(true);

        CScript script_code;
        script_code << OP_2 << ToByteVector(k[0].GetPubKey()) << ToByteVector(k[1].GetPubKey()) << ToByteVector(k[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

        // privkey and pubkey here are a list of keys separated by commas
        std::string privkeys = EncodeSecret(k[0]);
        for (int i = 0; i < key_length - 1; i++) {
            privkeys += ",";
            privkeys += EncodeSecret(k[i + 1]);
        }
        key.privkey = QString::fromStdString(privkeys);
        std::string pubkeys = HexStr(k[0].GetPubKey());
        for (int i = 0; i < key_length - 1; i++) {
            pubkeys += ",";
            pubkeys += HexStr(k[i + 1].GetPubKey());
        }
        key.pubkey = QString::fromStdString(pubkeys);
        key.p2pkh_addr = QString::fromStdString(EncodeDestination(ScriptHash(script_code)));
        key.p2pkh_script = QString::fromStdString(HexStr(GetScriptForDestination(ScriptHash(script_code))));
        key.p2wpkh_addr = QString::fromStdString(EncodeDestination(WitnessV0ScriptHash(script_code)));
        key.p2wpkh_script = QString::fromStdString(HexStr(GetScriptForDestination(WitnessV0ScriptHash(script_code))));
        key.p2sh_p2wpkh_addr = QString::fromStdString(EncodeDestination(ScriptHash(GetScriptForDestination(WitnessV0ScriptHash(script_code)))));
        key.p2sh_p2wpkh_script = QString::fromStdString(HexStr(GetScriptForDestination(ScriptHash(GetScriptForDestination(WitnessV0ScriptHash(script_code))))));
        key.p2sh_p2wpkh_redeem_script = QString::fromStdString(HexStr(script_code));
        return key;
    };

    auto get_unspent_info = [&wallet](QString& input) {
        UnspentInfo unspentInfo;

        CoinFilterParams filter_coins;
        filter_coins.min_amount = 0;
        filter_coins.max_amount = MAX_MONEY;
        filter_coins.min_sum_amount = MAX_MONEY;
        filter_coins.max_count = 0;
        std::vector<COutput> vecOutputs;
        {
            CCoinControl cctl;
            cctl.m_avoid_address_reuse = false;
            cctl.m_min_depth = 0;
            cctl.m_max_depth = 999999;
            cctl.m_include_unsafe_inputs = true;
            LOCK(wallet->cs_wallet);
            vecOutputs = AvailableCoinsListUnspent(*wallet, &cctl, filter_coins).All();
        }

        LOCK(wallet->cs_wallet);
        CTxDestination dest = DecodeDestination(input.toStdString());
        std::set<CTxDestination> destinations;
        destinations.insert(dest);
        for (const COutput& out : vecOutputs) {
            CTxDestination address;
            const CScript &scriptPubKey = out.txout.scriptPubKey;
            bool fValidAddress = ExtractDestination(scriptPubKey, address);

            if (!fValidAddress || !destinations.count(address))
                continue;

            unspentInfo.spendable = out.spendable;
            unspentInfo.solvable = out.solvable;
            break;
        }
        return unspentInfo;
    };

    auto send_coins = [&wallet, &test](QString& address, CAmount amount) {
        LOCK(wallet->cs_wallet);
        CTxDestination dest = DecodeDestination(address.toStdString());

        // Make sure the destination is valid
        QVERIFY2(IsValidDestination(dest), "Invalid address");
        CScript script_pub_key = GetScriptForDestination(dest);
        bool error = false;
        test.AddTx(*wallet, CRecipient{script_pub_key, amount, false}, error);
        QVERIFY(!error);
    };

    // Bitcoin Address (implicit non-internal)
    Key key;
    key = BuildAddress(wallet.get());
    ImportMultiTestData data;
    data.scriptPubKey = key.p2pkh_addr;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    QString watchonly_address = key.p2pkh_addr;
    int64_t timestamp = 0;
    {
        LOCK(::cs_main);
        timestamp = node.context()->chainman->ActiveChain().Tip()->GetMedianTimePast();
    }

    AddressInfo addressInfo;
    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, timestamp);
    QCOMPARE(addressInfo.ischange, false);

    data = ImportMultiTestData();
    data.scriptPubKey = "not valid scriptPubKey";
    EditMultiAndSubmit(&importDialog, data,
                       QString("Error: Invalid scriptPubKey \"not valid scriptPubKey\""));

    // ScriptPubKey + internal
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_script;
    data.internal = true;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, timestamp);
    QCOMPARE(addressInfo.ischange, true);

    // should pass since GUI can't accept a label if internal is set to true
    // ScriptPubKey + internal + label
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_script;
    data.internal = true;
    data.label = "Unsuccessful labelling for internal addresses";
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    // Nonstandard scriptPubKey + !internal
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = QString::fromStdString(HexStr(GetScriptForDestination(key.dest) << OP_NOP));
    EditMultiAndSubmit(&importDialog, data,
                       QString("Error: Internal must be set to true for nonstandard scriptPubKey imports."));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, 0);

    // Address + Public key + !Internal(explicit)
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.public_keys = key.pubkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // ScriptPubKey + Public key + internal
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_script;
    data.public_keys = key.pubkey;
    data.internal = true;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // Nonstandard scriptPubKey + Public key + !internal
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = QString::fromStdString(HexStr(GetScriptForDestination(key.dest) << OP_NOP));
    data.public_keys = key.pubkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Error: Internal must be set to true for nonstandard scriptPubKey imports."));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, 0);

    // Address + Private key + !watchonly
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.timestamp, timestamp);

    EditMultiAndSubmit(&importDialog, data,
                       QString("Error: The wallet already contains the private key for this address or script (\"" + key.p2pkh_script + "\")"));

    // Address + Private key + watchonly
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.private_keys = key.privkey;
    data.watch_only = true;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded\n") +
            QString("Warnings: All private keys are provided, outputs will be considered spendable. If this is intentional, do not specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // ScriptPubKey + Private key + internal
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_script;
    data.private_keys = key.privkey;
    data.internal = true;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // Nonstandard scriptPubKey + Private key + !internal
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = QString::fromStdString(HexStr(GetScriptForDestination(key.dest) << OP_NOP));
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Error: Internal must be set to true for nonstandard scriptPubKey imports."));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.timestamp, 0);

    // P2SH address
    key = get_multisig();
    send_coins(key.p2pkh_addr, 5 * COIN);
    {
        LOCK(::cs_main);
        timestamp = node.context()->chainman->ActiveChain().Tip()->GetMedianTimePast();
    }
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.isscript, true);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.timestamp, timestamp);

    UnspentInfo unspentInfo = get_unspent_info(key.p2pkh_addr);
    QCOMPARE(unspentInfo.spendable, false);
    QCOMPARE(unspentInfo.solvable, false);

    // P2SH + Redeem script
    key = get_multisig();
    send_coins(key.p2pkh_addr, 5 * COIN);
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.redeem_script = key.p2sh_p2wpkh_redeem_script;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded\n") +
                                            QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.timestamp, timestamp);


    unspentInfo = get_unspent_info(key.p2pkh_addr);
    QCOMPARE(unspentInfo.spendable, false);
    QCOMPARE(unspentInfo.solvable, true);

    // P2SH + Redeem script + Private Keys + !Watchonly
    key = get_multisig(2);
    send_coins(key.p2pkh_addr, 5 * COIN);
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.redeem_script = key.p2sh_p2wpkh_redeem_script;
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded\n") +
                                            QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.timestamp, timestamp);

    unspentInfo = get_unspent_info(key.p2pkh_addr);
    QCOMPARE(unspentInfo.spendable, false);
    QCOMPARE(unspentInfo.solvable, true);

    // P2SH + Redeem script + Private Keys + Watchonly
    key = get_multisig(2);
    send_coins(key.p2pkh_addr, 5 * COIN);
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.redeem_script = key.p2sh_p2wpkh_redeem_script;
    data.private_keys = key.privkey;
    data.watch_only = true;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // Address + Public key + !Internal + Wrong pubkey
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    Key wrong_key = BuildAddress(wallet.get());
    data.public_keys = wrong_key.pubkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\nImporting as non-solvable: some required keys are missing. If this is intentional, don't provide any keys, pubkeys, witnessscript, or redeemscript.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, false);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // ScriptPubKey + Public key + internal + Wrong pubkey
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_script;
    wrong_key = BuildAddress(wallet.get());
    data.public_keys = wrong_key.pubkey;
    data.internal = true;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\nImporting as non-solvable: some required keys are missing. If this is intentional, don't provide any keys, pubkeys, witnessscript, or redeemscript.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, false);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // Address + Private key + !watchonly + Wrong private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    wrong_key = BuildAddress(wallet.get());
    data.private_keys = wrong_key.privkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\nImporting as non-solvable: some required keys are missing. If this is intentional, don't provide any keys, pubkeys, witnessscript, or redeemscript.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, false);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // ScriptPubKey + Private key + internal + Wrong private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_script;
    wrong_key = BuildAddress(wallet.get());
    data.private_keys = wrong_key.privkey;
    data.internal = true;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\nImporting as non-solvable: some required keys are missing. If this is intentional, don't provide any keys, pubkeys, witnessscript, or redeemscript.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, false);
    QCOMPARE(addressInfo.timestamp, timestamp);

    // Import P2WPKH address as watch only
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2wpkh_addr;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2wpkh_addr);
    QCOMPARE(addressInfo.iswatchonly, true);
    QCOMPARE(addressInfo.solvable, false);

    // Import P2WPKH address with public key but no private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2wpkh_addr;
    data.public_keys = key.pubkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2wpkh_addr);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);

    // Import P2WPKH address with key and check it is spendable
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2wpkh_addr;
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2wpkh_addr);
    QCOMPARE(addressInfo.iswatchonly, false);
    QCOMPARE(addressInfo.ismine, true);

    // P2WSH multisig address without scripts or keys
    key = get_multisig();
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2wpkh_addr;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.solvable, false);

    // Same P2WSH multisig address as above, but now with witnessscript + private keys
    key = get_multisig();
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2pkh_addr;
    data.redeem_script = key.p2sh_p2wpkh_redeem_script;
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.sigsrequired, 2);

    // P2SH-P2WPKH address with no redeemscript or public or private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2sh_p2wpkh_addr;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2sh_p2wpkh_addr);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, false);

    // P2SH-P2WPKH address + redeemscript + public key with no private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2sh_p2wpkh_addr;
    data.redeem_script = key.p2sh_p2wpkh_redeem_script;
    data.public_keys = key.pubkey;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2sh_p2wpkh_addr);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);

    // P2SH-P2WPKH address + redeemscript + private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2sh_p2wpkh_addr;
    data.redeem_script = key.p2sh_p2wpkh_redeem_script;
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2wpkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);

    // P2SH-P2WSH multisig + redeemscript with no private key
    key = get_multisig();
    data = ImportMultiTestData();
    data.scriptPubKey = key.p2sh_p2wpkh_addr;
    data.redeem_script = key.p2wpkh_script;
    data.witness_script = key.p2sh_p2wpkh_redeem_script;
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);

    // Test importing of a P2SH-P2WPKH address via descriptor + private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.desc = "sh(wpkh(" + key.pubkey + "))";
    data.label = "Unsuccessful P2SH-P2WPKH descriptor import";
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Error: Missing checksum"));

    // Test importing of a P2SH-P2WPKH address via descriptor + private key
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    std::string desc_str = "sh(wpkh(" + key.pubkey.toStdString() + "))";
    data.desc = DescsumCreate(desc_str);
    data.label = "Successful P2SH-P2WPKH descriptor import";
    data.private_keys = key.privkey;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2sh_p2wpkh_addr);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.label, "Successful P2SH-P2WPKH descriptor import");

    // Test ranged descriptor fails if range is not specified
    std::string xpriv = "tprv8ZgxMBicQKsPeuVhWwi6wuMQGfPKi9Li5GtX35jVNknACgqe3CY4g5xgkfDDJcmtF7o1QnxWDRYw4H5P26PXq7sbcUkEqeR4fg3Kxp2tigg";
    std::vector<QString> addresses = {"2N7yv4p8G8yEaPddJxY41kPihnWvs39qCMf", "2MsHxyb2JS3pAySeNUsJ7mNnurtpeenDzLA", // hdkeypath=m/0'/0'/0' and 1'
                        "bcrt1qrd3n235cj2czsfmsuvqqpr3lu6lg0ju7scl8gn", "bcrt1qfqeppuvj0ww98r6qghmdkj70tv8qpchehegrg8"}; // wpkh subscripts corresponding to the above addresses
    std::string desc = "sh(wpkh(" + xpriv + "/0'/0'/*'" + "))";
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.desc = DescsumCreate(desc);
    EditMultiAndSubmit(&importDialog, data, QString("Error: Descriptor is ranged, please specify the range"));

    // Test importing of a ranged descriptor with xpriv
    data.range_end = 1;
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    for (const auto& address: addresses) {
        GetAddressInfo(wallet.get(), addressInfo, address);
        QCOMPARE(addressInfo.ismine, true);
        QCOMPARE(addressInfo.solvable, true);
    }

    // GUI only accepts range 0 to 0x7FFFFFFF
    data.range_start = 2;
    data.range_end = 1;
    EditMultiAndSubmit(&importDialog, data, QString("Error: Range specified as [begin,end] must not have begin after end"));

    data.range_start = 0;
    data.range_end = 1000001;
    EditMultiAndSubmit(&importDialog, data, QString("Error: Range is too large"));

    // Test importing a descriptor containing a WIF private key
    std::string wif_priv = "cTe1f5rdT8A8DFgVWTjyPwACsDPJM9ff4QngFxUixCSvvbg1x6sh";
    QString address = "2MuhcG52uHPknxDgmGPsV18jSHFBnnRgjPg";
    desc = "sh(wpkh(" + wif_priv + "))";
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    data.desc = DescsumCreate(desc);
    EditMultiAndSubmit(&importDialog, data, QString("Import Succeeded"));

    GetAddressInfo(wallet.get(), addressInfo, address);
    QCOMPARE(addressInfo.ismine, true);
    QCOMPARE(addressInfo.solvable, true);

    // Test importing of a P2PKH address via descriptor
    key = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    desc_str = "pkh(" + key.pubkey.toStdString() + ")";
    data.desc = DescsumCreate(desc_str);
    data.label = "P2PKH descriptor import";
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.solvable, true);
    QCOMPARE(addressInfo.label, "P2PKH descriptor import");

    // Test importing of a multisig via descriptor
    key = BuildAddress(wallet.get());
    Key key2 = BuildAddress(wallet.get());
    data = ImportMultiTestData();
    desc_str = "multi(1," + key.pubkey.toStdString() + "," + key2.pubkey.toStdString() + ")";
    data.desc = DescsumCreate(desc_str);
    EditMultiAndSubmit(&importDialog, data,
                       QString("Import Succeeded\n") +
                       QString("Warnings: Some private keys are missing, outputs will be considered watchonly. If this is intentional, specify the watchonly flag.\n"));

    GetAddressInfo(wallet.get(), addressInfo, key.p2pkh_addr);
    QCOMPARE(addressInfo.ismine, false);
    QCOMPARE(addressInfo.iswatchonly, false);
}

void ImportMultiTests::importMultiTests()
{
#ifdef Q_OS_MACOS
    if (QApplication::platformName() == "minimal") {
        // Disable for mac on "minimal" platform to avoid crashes inside the Qt
        // framework when it tries to look up unimplemented cocoa functions,
        // and fails to handle returned nulls
        // (https://bugreports.qt.io/browse/QTBUG-49686).
        QWARN("Skipping ImportMultiTests on mac build with 'minimal' platform set due to Qt bugs. To run AppTests, invoke "
              "with 'QT_QPA_PLATFORM=cocoa test_bitcoin-qt' on mac, or else use a linux or windows build.");
        return;
    }
#endif
    TestImportMulti(m_node);
}