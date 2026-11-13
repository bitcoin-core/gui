// Copyright (c) 2017-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/addressbooktests.h>
#include <qt/test/util.h>
#include <test/util/setup_common.h>

#include <interfaces/chain.h>
#include <interfaces/node.h>
#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/clientmodel.h>
#include <qt/editaddressdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <key.h>
#include <key_io.h>
#include <script/descriptor.h>
#include <wallet/wallet.h>
#include <wallet/test/util.h>
#include <walletinitinterface.h>

#include <chrono>

#include <QApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableView>
#include <QTimer>

using wallet::AddWallet;
using wallet::CWallet;
using wallet::CreateMockableWalletDatabase;
using wallet::RemoveWallet;
using wallet::WALLET_FLAG_DESCRIPTORS;
using wallet::WalletContext;
using wallet::WalletDescriptor;

namespace
{

/**
 * Fill the edit address dialog box with data, submit it, and ensure that
 * the resulting message meets expectations.
 */
void EditAddressAndSubmit(
        EditAddressDialog* dialog,
        const QString& label, const QString& address, QString expected_msg)
{
    QString warning_text;

    dialog->findChild<QLineEdit*>("labelEdit")->setText(label);
    dialog->findChild<QValidatedLineEdit*>("addressEdit")->setText(address);

    ConfirmMessage(&warning_text, 5ms);
    dialog->accept();
    QCOMPARE(warning_text, expected_msg);
}

/**
 * Test adding various send addresses to the address book.
 *
 * There are three cases tested:
 *
 *   - new_address: a new address which should add as a send address successfully.
 *   - existing_s_address: an existing sending address which won't add successfully.
 *   - existing_r_address: an existing receiving address which won't add successfully.
 *
 * In each case, verify the resulting state of the address book and optionally
 * the warning message presented to the user.
 */
void TestAddAddressesToSendBook(interfaces::Node& node)
{
    TestChain100Setup test;
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    node.setContext(&test.m_node);
    const std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(node.context()->chain.get(), "", CreateMockableWalletDatabase());
    wallet->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
    {
        LOCK(wallet->cs_wallet);
        wallet->SetupDescriptorScriptPubKeyMans();
    }

    auto build_address{[]() {
        const WitnessV0KeyHash dest{GenerateRandomKey().GetPubKey()};
        return std::make_pair(dest, QString::fromStdString(EncodeDestination(dest)));
    }};

    CTxDestination r_key_dest, s_key_dest;

    // Add a preexisting "receive" entry in the address book.
    QString preexisting_r_address;
    QString r_label("already here (r)");

    // Add a preexisting "send" entry in the address book.
    QString preexisting_s_address;
    QString s_label("already here (s)");

    // Define a new address (which should add to the address book successfully).
    QString new_address_a;
    QString new_address_b;

    std::tie(r_key_dest, preexisting_r_address) = build_address();
    std::tie(s_key_dest, preexisting_s_address) = build_address();
    std::tie(std::ignore, new_address_a) = build_address();
    std::tie(std::ignore, new_address_b) = build_address();

    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBook(r_key_dest, r_label.toStdString(), wallet::AddressPurpose::RECEIVE);
        wallet->SetAddressBook(s_key_dest, s_label.toStdString(), wallet::AddressPurpose::SEND);
    }

    auto check_addbook_size = [&wallet](int expected_size) {
        LOCK(wallet->cs_wallet);
        QCOMPARE(static_cast<int>(wallet->m_address_book.size()), expected_size);
    };

    // We should start with the two addresses we added earlier and nothing else.
    check_addbook_size(2);

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
    EditAddressDialog editAddressDialog(EditAddressDialog::NewSendingAddress);
    editAddressDialog.setModel(walletModel.getAddressTableModel());

    AddressBookPage address_book{platformStyle.get(), AddressBookPage::ForEditing, AddressBookPage::SendingTab};
    address_book.setModel(walletModel.getAddressTableModel());
    auto table_view = address_book.findChild<QTableView*>("tableView");
    QCOMPARE(table_view->model()->rowCount(), 1);

    EditAddressAndSubmit(
        &editAddressDialog, QString("uhoh"), preexisting_r_address,
        QString(
            "Address \"%1\" already exists as a receiving address with label "
            "\"%2\" and so cannot be added as a sending address."
            ).arg(preexisting_r_address).arg(r_label));
    check_addbook_size(2);
    QCOMPARE(table_view->model()->rowCount(), 1);

    EditAddressAndSubmit(
        &editAddressDialog, QString("uhoh, different"), preexisting_s_address,
        QString(
            "The entered address \"%1\" is already in the address book with "
            "label \"%2\"."
            ).arg(preexisting_s_address).arg(s_label));
    check_addbook_size(2);
    QCOMPARE(table_view->model()->rowCount(), 1);

    // Submit a new address which should add successfully - we expect the
    // warning message to be blank.
    EditAddressAndSubmit(
        &editAddressDialog, QString("io - new A"), new_address_a, QString(""));
    check_addbook_size(3);
    QCOMPARE(table_view->model()->rowCount(), 2);

    EditAddressAndSubmit(
        &editAddressDialog, QString("io - new B"), new_address_b, QString(""));
    check_addbook_size(4);
    QCOMPARE(table_view->model()->rowCount(), 3);

    auto search_line = address_book.findChild<QLineEdit*>("searchLineEdit");

    search_line->setText(r_label);
    QCOMPARE(table_view->model()->rowCount(), 0);

    search_line->setText(s_label);
    QCOMPARE(table_view->model()->rowCount(), 1);

    search_line->setText("io");
    QCOMPARE(table_view->model()->rowCount(), 2);

    // Check wildcard "?".
    search_line->setText("io?new");
    QCOMPARE(table_view->model()->rowCount(), 0);
    search_line->setText("io???new");
    QCOMPARE(table_view->model()->rowCount(), 2);

    // Check wildcard "*".
    search_line->setText("io*new");
    QCOMPARE(table_view->model()->rowCount(), 2);
    search_line->setText("*");
    QCOMPARE(table_view->model()->rowCount(), 3);

    search_line->setText(preexisting_r_address);
    QCOMPARE(table_view->model()->rowCount(), 0);

    search_line->setText(preexisting_s_address);
    QCOMPARE(table_view->model()->rowCount(), 1);

    search_line->setText(new_address_a);
    QCOMPARE(table_view->model()->rowCount(), 1);

    search_line->setText(new_address_b);
    QCOMPARE(table_view->model()->rowCount(), 1);

    search_line->setText("");
    QCOMPARE(table_view->model()->rowCount(), 3);
}

/**
 * Test that CanSignMessageRole correctly filters the sign-message address picker:
 * - watch-only PKHash addresses (no private key) → not signable
 * - bech32 addresses (not PKHash) → not signable
 * - spendable PKHash addresses (private key imported) → signable
 * Also verifies AddressBookPage::AddressFilter::Signable shows only signable addresses.
 */
void TestSignableAddressFilter(interfaces::Node& node)
{
    TestChain100Setup test;
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    node.setContext(&test.m_node);

    const std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(
        node.context()->chain.get(), "", CreateMockableWalletDatabase());

    {
        LOCK(wallet->cs_wallet);
        wallet->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
        wallet->SetupDescriptorScriptPubKeyMans();

        // (a) Watch-only PKHash: address in book but no private key → not signable
        const PKHash watchonly_dest{GenerateRandomKey().GetPubKey()};
        wallet->SetAddressBook(watchonly_dest, "watchonly", wallet::AddressPurpose::RECEIVE);

        // (b) Bech32 (P2WPKH): not a PKHash destination → not signable
        const WitnessV0KeyHash bech32_dest{GenerateRandomKey().GetPubKey()};
        wallet->SetAddressBook(bech32_dest, "bech32", wallet::AddressPurpose::RECEIVE);

        // (c) Spendable PKHash: private key imported via pkh() descriptor → signable
        CKey legacy_key;
        legacy_key.MakeNewKey(true);
        FlatSigningProvider legacy_provider;
        std::string legacy_err;
        auto legacy_descs = Parse("pkh(" + EncodeSecret(legacy_key) + ")", legacy_provider, legacy_err, /*require_checksum=*/false);
        assert(!legacy_descs.empty());
        WalletDescriptor legacy_w_desc(std::move(legacy_descs[0]), /*creation_time=*/0, /*range_start=*/0, /*range_end=*/1, /*next_index=*/1);
        QVERIFY(wallet->AddWalletDescriptor(legacy_w_desc, legacy_provider, "", /*internal=*/false));
        const PKHash legacy_dest{legacy_key.GetPubKey()};
        wallet->SetAddressBook(legacy_dest, "legacy", wallet::AddressPurpose::RECEIVE);
    }

    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    OptionsModel optionsModel(node);
    bilingual_str error;
    QVERIFY(optionsModel.Init(error));
    ClientModel clientModel(node, &optionsModel);
    WalletContext& context = *node.walletLoader().context();
    AddWallet(context, wallet);
    WalletModel walletModel(interfaces::MakeWallet(context, wallet), clientModel, platformStyle.get());
    RemoveWallet(context, wallet, /*load_on_start=*/std::nullopt);
    AddressTableModel* addrModel = walletModel.getAddressTableModel();
    QVERIFY(addrModel);

    // Only the spendable PKHash should report CanSignMessageRole=true
    int signable_count = 0;
    for (int i = 0; i < addrModel->rowCount({}); ++i) {
        if (addrModel->data(addrModel->index(i, AddressTableModel::Address, {}),
                            AddressTableModel::CanSignMessageRole).toBool()) {
            ++signable_count;
        }
    }
    QCOMPARE(signable_count, 1);

    // AddressFilter::Signable must show only the spendable PKHash; no filter shows all
    AddressBookPage page_all{platformStyle.get(), AddressBookPage::ForSelection, AddressBookPage::ReceivingTab};
    page_all.setModel(addrModel);
    AddressBookPage page_signable{platformStyle.get(), AddressBookPage::ForSelection, AddressBookPage::ReceivingTab,
                                  nullptr, AddressBookPage::AddressFilter::Signable};
    page_signable.setModel(addrModel);

    auto* table_all      = page_all.findChild<QTableView*>("tableView");
    auto* table_signable = page_signable.findChild<QTableView*>("tableView");
    QVERIFY(table_all != nullptr);
    QVERIFY(table_signable != nullptr);
    QCOMPARE(table_signable->model()->rowCount(), 1);
    QVERIFY(table_all->model()->rowCount() >= 3);
}

/**
 * Regression test for the stale shared model bug: clicking the address book
 * button in SignVerifyMessageDialog previously called
 * WalletModel::refresh(pk_hash_only=true), which replaced the shared
 * addressTableModel pointer with a new filtered instance. Any other view
 * (e.g. the Receiving Addresses window) holding that pointer was left stale
 * and would not reflect addresses added after the picker was opened.
 *
 * Directly invokes on_addressBookButton_SM_clicked() — the exact trigger of
 * the bug — and verifies the shared addressTableModel pointer is unchanged.
 */
void TestAddressTableModelStability(interfaces::Node& node)
{
    TestChain100Setup test;
    auto wallet_loader = interfaces::MakeWalletLoader(*test.m_node.chain, *Assert(test.m_node.args));
    test.m_node.wallet_loader = wallet_loader.get();
    node.setContext(&test.m_node);

    const std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>(
        node.context()->chain.get(), "", CreateMockableWalletDatabase());
    {
        LOCK(wallet->cs_wallet);
        wallet->SetWalletFlag(WALLET_FLAG_DESCRIPTORS);
        wallet->SetupDescriptorScriptPubKeyMans();
        wallet->SetAddressBook(PKHash{GenerateRandomKey().GetPubKey()}, "addr1", wallet::AddressPurpose::RECEIVE);
    }

    std::unique_ptr<const PlatformStyle> platformStyle(PlatformStyle::instantiate("other"));
    OptionsModel optionsModel(node);
    bilingual_str error;
    QVERIFY(optionsModel.Init(error));
    ClientModel clientModel(node, &optionsModel);
    WalletContext& context = *node.walletLoader().context();
    AddWallet(context, wallet);
    WalletModel walletModel(interfaces::MakeWallet(context, wallet), clientModel, platformStyle.get());
    RemoveWallet(context, wallet, /*load_on_start=*/std::nullopt);

    // Store the shared model pointer — this is what Receiving Addresses holds
    AddressTableModel* initial_model = walletModel.getAddressTableModel();
    QVERIFY(initial_model != nullptr);
    QCOMPARE(initial_model->rowCount({}), 1);

    SignVerifyMessageDialog svd(platformStyle.get(), nullptr);
    svd.setModel(&walletModel);

    // Close the address book picker as soon as it opens — it uses exec() so
    // the timer fires inside that modal event loop
    QTimer::singleShot(0, []() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* page = qobject_cast<AddressBookPage*>(widget)) page->reject();
        }
    });

    // Invoke the exact slot that previously called model->refresh(pk_hash_only=true)
    QMetaObject::invokeMethod(&svd, "on_addressBookButton_SM_clicked");

    // The shared model pointer must be unchanged — Receiving Addresses is not stale
    QCOMPARE(walletModel.getAddressTableModel(), initial_model);
    QCOMPARE(initial_model->rowCount({}), 1);
}

} // namespace

void AddressBookTests::addressBookTests()
{
    TestAddAddressesToSendBook(m_node);
    TestAddressTableModelStability(m_node);
    TestSignableAddressFilter(m_node);
}
