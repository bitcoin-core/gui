#include <qt/importdialog.h>
#include <qt/forms/ui_importdialog.h>
#include <qt/importentry.h>

#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <key_io.h>
#include <qt/walletmodel.h>
#include <interfaces/node.h>
#include <wallet/imports.h>
#include <wallet/wallet.h>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <iostream>
#include <QScrollBar>
#include <tinyformat.h>
#include <chain.h>

void RangeDescriptorCheck(int64_t low, int64_t high)
{
    if (low > high) {
        throw wallet::InvalidParameter("Range specified as [begin,end] must not have begin after end");
    }
    if (low < 0) {
        throw wallet::InvalidParameter("Range should be greater or equal than 0");
    }
    if ((high >> 31) != 0) {
        throw wallet::InvalidParameter("End of range is too high");
    }
    if (high >= low + 1000000) {
        throw wallet::InvalidParameter("Range is too large");
    }
    return;
}

ImportDialog::ImportDialog(Page _page, WalletModel *walletModel, QWidget *parent) :
    QDialog(parent, GUIUtil::dialog_flags),
    ui(new Ui::ImportDialog),
    walletModel(walletModel),
    page(_page)
{
    ui->setupUi(this);

    switch (page) {
        case importPubkey: {
            ui->QStackContainer->setCurrentWidget(ui->importKeyPage);
            this->setWindowTitle("Import Public Key");
            ui->keyLabelIKP->setText("Public Key");
            ui->keyEditIKP->setPlaceholderText("Enter a Public Key (e.g. 1NS17iag9jJgTHDlVXjvLCEnZuQ3rJDE9L)");
            ui->rescanButtonIKP->setCheckState(Qt::Checked);
            ui->p2shButtonIKP->hide();
            break;
        } case importPrivkey: {
            ui->QStackContainer->setCurrentWidget(ui->importKeyPage);
            this->setWindowTitle("Import Private Key");
            ui->keyLabelIKP->setText("Private Key");
            ui->keyEditIKP->setPlaceholderText("Enter a Private Key (e.g. L4sTHokqoFZpcmJgsAG2yxrf2f71v6PYpK2qEkCvc2Fp8VVdMiU3)");
            ui->rescanButtonIKP->setCheckState(Qt::Checked);
            ui->p2shButtonIKP->hide();
            break;
        } case importAddress: {
            ui->QStackContainer->setCurrentWidget(ui->importKeyPage);
            this->setWindowTitle("Import Address");
            ui->keyLabelIKP->setText("Address");
            ui->keyEditIKP->setPlaceholderText("Enter a Bitcoin address (e.g. 1NS17iag9jJgTHDlVXjvLCEnZuQ3rJDE9L)");
            ui->rescanButtonIKP->setCheckState(Qt::Checked);
            ui->p2shButtonIKP->setCheckState(Qt::Unchecked);
            break;
        } case importMulti: {
            ui->QStackContainer->setCurrentWidget(ui->importEntries);
            this->setWindowTitle("Import Multi");
            this->resize(240, this->height());
            this->resize(650, this->width());
            addEntry();
            connect(ui->addButton, &QPushButton::clicked, this, &ImportDialog::addEntry);
            break;
        }
        case importDescriptors: {
            ui->QStackContainer->setCurrentWidget(ui->importEntries);
            this->setWindowTitle("Import Descriptors");
            this->resize(240, this->height());
            this->resize(650, this->width());
            addEntry();
            connect(ui->addButton, &QPushButton::clicked, this, &ImportDialog::addEntry);
            break;
        }
    }

    GUIUtil::handleCloseWindowShortcut(this);
}

ImportDialog::~ImportDialog()
{
    delete ui;
}

int64_t ImportDialog::GetImportTimestamp(const int64_t &entry, int64_t now) {
    if (entry) {
        return entry;
    } else {
        return now;
    }
}

struct resultpdi {
    bool success;
    std::vector<std::string> warnings;
    std::string error;
};

void ImportDialog::accept() {
    // Whether to perform rescan after import
    bool fRescan = true;
    std::string strIKP;
    std::string strLabel;
    if (page == Page::importPubkey || page == Page::importPrivkey || page == Page::importAddress) {
        strIKP = ui->keyEditIKP->text().toStdString();
        if (strIKP.empty()) {
            QMessageBox::warning(this, tr("Error"), tr("%1 is empty").arg(ui->keyLabelIKP->text()));
            return;
        }
        strLabel = ui->labelEditIKP->text().toStdString();

        if (!ui->rescanButtonIKP->isChecked())
            fRescan = false;

        if (fRescan && walletModel->wallet().chain().havePruned()) {
            // Exit early and print an error.
            // If a block is pruned after this check, we will import the key(s),
            // but fail the rescan with a generic error.
            QMessageBox::critical(this, tr("Rescan failed"), tr("Rescan is disabled when blocks are pruned"));
            return;
        }
    }

    wallet::WalletRescanReserver reserver = walletModel->wallet().getReserver();
    if (fRescan && !reserver.reserve()) {
        QMessageBox::critical(this, tr("Rescan failed"),
                              tr("Wallet is currently rescanning. Abort existing rescan or wait."));
        return;
    }

    switch (page) {
        case importPubkey: {
            if (!IsHex(strIKP)) {
                QMessageBox::critical(this, tr("Invalid Public Key"), tr("Pubkey must be a hex string"));
                return;
            }

            std::vector<unsigned char> data(ParseHex(strIKP));
            CPubKey pubKey(data);
            if (!pubKey.IsFullyValid()) {
                QMessageBox::critical(this, tr("Invalid Public Key"),
                                      tr("Pubkey is not a valid public key"));
                return;
            }

            walletModel->wallet().processPublicKey(strLabel, pubKey);

            if (fRescan) {
                int64_t scanned_time = walletModel->wallet().RescanFromTime(0, reserver, true);
                if (walletModel->wallet().IsAbortingRescan()) {
                    QMessageBox::critical(this, tr("Misc Error"), tr("Rescan aborted by user."));
                    return;
                } else if (scanned_time > 0) {
                    QMessageBox::critical(this, tr("Wallet Error"),
                                          tr("Rescan was unable to fully rescan the blockchain. Some transactions may be missing."));
                    return;
                }
                walletModel->wallet().ResubmitWalletTransactions(/*relay=*/false, /*force=*/true);
            }
            QMessageBox::information(this, tr("Import Succeeded"), tr("Import Succeeded"));
            break;
        }
        case importPrivkey: {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if (!ctx.isValid()) {
                return;
            }

            CKey key = DecodeSecret(strIKP);
            if (!key.IsValid()) {
                QMessageBox::critical(this, tr("Invalid Private Key"), tr("Invalid private key encoding."));
                return;
            }

            try {
                walletModel->wallet().processPrivateKey(strLabel, key);
            } catch (const wallet::WalletError& e) {
                QMessageBox::critical(this, tr("Wallet Error"), tr(e.error.c_str()));
                return;
            }

            if (fRescan) {
                int64_t scanned_time = walletModel->wallet().RescanFromTime(0, reserver, true);
                if (walletModel->wallet().IsAbortingRescan()) {
                    QMessageBox::critical(this, tr("Misc Error"),
                                          tr("Rescan aborted by user."));
                    return;
                } else if (scanned_time > 0) {
                    QMessageBox::critical(this, tr("Wallet Error"),
                                          tr("Rescan was unable to fully rescan the blockchain. Some transactions may be missing."));
                    return;
                }
            }
            QMessageBox::information(this, tr("Import Succeeded"), tr("Import Succeeded"));
            break;
        }
        case importAddress: {
            // Whether to import a p2sh version, too
            bool fP2SH = false;
            if (ui->p2shButtonIKP->isChecked())
                fP2SH = true;

            try {
                walletModel->wallet().processAddress(strIKP, strLabel, fP2SH);
            } catch (const wallet::InvalidAddressOrKey& e) {
                QMessageBox::critical(this, tr("Invalid Address"), tr(e.error.c_str()));
                return;
            }

            if (fRescan) {
                int64_t scanned_time = walletModel->wallet().RescanFromTime(0, reserver, true);
                if (walletModel->wallet().IsAbortingRescan()) {
                    QMessageBox::critical(this, tr("Misc Error"),
                                          tr("Rescan aborted by user."));
                    return;
                } else if (scanned_time > 0) {
                    QMessageBox::critical(this, tr("Wallet Error"),
                                          tr("Rescan was unable to fully rescan the blockchain. Some transactions may be missing."));
                    return;
                }
                walletModel->wallet().ResubmitWalletTransactions(/*relay=*/false, /*force=*/true);
            }
            QMessageBox::information(this, tr("Import Succeeded"), tr("Import Succeeded"));
            break;
        }
        case importMulti: {
            int64_t now = 0;
            bool fRunScan = false;
            int64_t nLowestTimestamp = 0;
            std::vector<resultpdi> response;

            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if (!ctx.isValid()) {
                return;
            }

            // Verify all timestamps are present before importing any keys.
            CHECK_NONFATAL(walletModel->wallet().chain().findBlock(walletModel->wallet().GetLastBlockHash(),
                                                                   interfaces::FoundBlock().time(nLowestTimestamp).mtpTime(now)));
            const int64_t minimumTimestamp = 1;

            for (int i = 0; i < ui->entries->count(); ++i) {
                ImportEntry *entry = qobject_cast<ImportEntry *>(ui->entries->itemAt(i)->widget());
                wallet::ImportMultiData mData = entry->getMultiData();
                const int64_t timestamp = std::max(GetImportTimestamp(mData.timestamp, now), minimumTimestamp);
                resultpdi result;

                try {
                    // Internal addresses should not have a label
                    if (mData.internal && !mData.label.empty()) {
                        throw wallet::InvalidParameter("Internal addresses should not have a label");
                    }

                    if (!mData.scriptPubKey.empty()) {
                        // if the input isn't a valid address assume it is a script
                        CTxDestination dest = DecodeDestination(mData.scriptPubKey);
                        if (!IsValidDestination(dest)) {
                            mData.isScript = true;
                        }
                    } else {
                        std::string error;
                        mData.parsed_desc = Parse(entry->getDesc(), mData.keys, error, /* require_checksum = */ true);
                        if (!mData.parsed_desc) {
                            throw wallet::InvalidAddressOrKey(error);
                        }
                        if (mData.parsed_desc->GetOutputType() == OutputType::BECH32M) {
                            throw wallet::InvalidAddressOrKey("Bech32m descriptors cannot be imported into legacy wallets");
                        }
                        bool range_exists = mData.range_start > 0 || mData.range_end > 0;
                        if (!mData.parsed_desc->IsRange() && range_exists) {
                            throw wallet::InvalidParameter("Range should not be specified for an un-ranged descriptor");
                        } else if (mData.parsed_desc->IsRange()) {
                            if (!range_exists) {
                                throw wallet::InvalidParameter("Descriptor is ranged, please specify the range");
                            }
                            RangeDescriptorCheck(mData.range_start, mData.range_end);
                        }
                    }
                    walletModel->wallet().processImport(mData, result.warnings, timestamp);
                    result.success = true;
                } catch (const wallet::MiscError& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (const wallet::WalletError& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (const wallet::InvalidAddressOrKey& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (const wallet::InvalidParameter& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (...) {
                    result.success = false;
                    result.error = "Missing required fields";
                }

                response.push_back(result);

                if (!fRescan) {
                    continue;
                }

                // If at least one request was successful then allow rescan.
                if (result.success) {
                    fRunScan = true;
                }

                // Get the lowest timestamp.
                if (timestamp < nLowestTimestamp) {
                    nLowestTimestamp = timestamp;
                }
            }

            if (fRescan && fRunScan && ui->entries->count()) {
                int64_t scannedTime = walletModel->wallet().RescanFromTime(nLowestTimestamp, reserver, true);
                walletModel->wallet().ResubmitWalletTransactions(/*relay=*/false, /*force=*/true);

                if (walletModel->wallet().IsAbortingRescan()) {
                    QMessageBox::critical(this, tr("Misc Error"),
                                          tr("Rescan aborted by user."));

                    if (scannedTime > nLowestTimestamp) {
                        std::vector <resultpdi> results = response;
                        response.clear();

                        for (int i = 0; i < ui->entries->count(); ++i) {
                            ImportEntry *entry = qobject_cast<ImportEntry *>(ui->entries->itemAt(i)->widget());
                            // If key creation date is within the successfully scanned
                            // range, or if the import result already has an error set, let
                            // the result stand unmodified. Otherwise, replace the result
                            // with an error message.
                            if (scannedTime <= GetImportTimestamp(entry->getMultiData().timestamp, now) || !results.at(i).error.empty()) {
                                response.push_back(results.at(i));
                            } else {
                                resultpdi result;
                                result.success = false;
                                result.error = strprintf(
                                                "Rescan failed for key with creation timestamp %d. There was an error reading a "
                                                "block from time %d, which is after or within %d seconds of key creation, and "
                                                "could contain transactions pertaining to the key. As a result, transactions "
                                                "and coins using this key may not appear in the wallet. This error could be "
                                                "caused by pruning or data corruption (see bitcoind log for details) and could "
                                                "be dealt with by downloading and rescanning the relevant blocks (see -reindex "
                                                "option and rescanblockchain RPC).",
                                                GetImportTimestamp(entry->getMultiData().timestamp, now), scannedTime - TIMESTAMP_WINDOW - 1,
                                                TIMESTAMP_WINDOW);
                                response.push_back(std::move(result));
                            }
                        }
                    }
                }
            }

            bool success = false;
            for (size_t i = 0; i < response.size(); i++) {
                if (response[i].success) {
                    std::string warning_str;
                    if (response[i].warnings.size() != 0) {
                        warning_str = "\nWarnings: ";
                        for (const auto& w: response[i].warnings) {
                            warning_str += w + "\n";
                        }
                    }
                    QMessageBox::information(this, QString::fromStdString(strprintf("Import Succeeded: input box # %d", i)),
                                             QString::fromStdString("Import Succeeded" + warning_str));
                } else {
                    success = true;
                    QMessageBox::critical(this, QString::fromStdString(strprintf("Error: input box # %d", i)),
                                          QString::fromStdString("Error: " + response[i].error));
                }
            }
            if (success) return;
            break;
        }
        case importDescriptors: {
            const int64_t minimum_timestamp = 1;
            int64_t now = 0;
            int64_t lowest_timestamp = 0;
            bool rescan = false;
            std::vector<resultpdi> response;

            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if (!ctx.isValid()) {
                return;
            }

            CHECK_NONFATAL(walletModel->wallet().chain().findBlock(walletModel->wallet().GetLastBlockHash(),
                                                                   interfaces::FoundBlock().time(lowest_timestamp).mtpTime(now)));

            // Get all timestamps and extract the lowest timestamp
            for (int i = 0; i < ui->entries->count(); ++i) {
                ImportEntry *entry = qobject_cast<ImportEntry *>(ui->entries->itemAt(i)->widget());

                // This throws an error if "timestamp" doesn't exist
                wallet::ImportDescriptorData dData = entry->getDescriptorData();
                const int64_t timestamp = std::max(GetImportTimestamp(dData.timestamp, now), minimum_timestamp);
                resultpdi result;

                try {
                    if (entry->getDesc().empty()) {
                        throw wallet::InvalidParameter("Descriptor not found.");
                    }

                    // Parse descriptor string
                    FlatSigningProvider keys;
                    std::string error;
                    dData.parsed_desc = Parse(entry->getDesc(), keys, error, /* require_checksum = */ true);
                    if (!dData.parsed_desc) {
                        throw wallet::InvalidAddressOrKey(error);
                    }

                    bool range_exists = dData.range_start > 0 || dData.range_end > 0;
                    if (!dData.parsed_desc->IsRange() && range_exists) {
                        throw wallet::InvalidParameter("Range should not be specified for an un-ranged descriptor");
                    } else if (dData.parsed_desc->IsRange()) {
                        if (!range_exists) {
                            result.warnings.push_back("Range not given, using default keypool range");
                            dData.range_start = 0;
                            dData.range_end = walletModel->wallet().GetKeypoolSize();
                        } else {
                            RangeDescriptorCheck(dData.range_start, dData.range_end);
                            dData.range_end++;
                        }

                        if (dData.next_index) {
                            // bound checks
                            if (dData.next_index < dData.range_start || dData.next_index >= dData.range_end) {
                                throw wallet::InvalidParameter("next_index is out of range");
                            }
                        } else {
                            dData.next_index = dData.range_start;
                        }
                    } else {
                        dData.range_start = 0, dData.range_end = 1, dData.next_index = 0;
                    }

                    walletModel->wallet().processDescriptorImport(dData, result.warnings, keys, range_exists, timestamp);
                    result.success = true;
                } catch (const wallet::MiscError& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (const wallet::WalletError& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (const wallet::InvalidAddressOrKey& e) {
                    result.success = false;
                    result.error = e.error;
                } catch (const wallet::InvalidParameter& e) {
                    result.success = false;
                    result.error = e.error;
                }

                response.push_back(result);

                if (lowest_timestamp > timestamp) {
                    lowest_timestamp = timestamp;
                }

                // If we know the chain tip, and at least one request was successful then allow rescan
                if (!rescan && result.success) {
                    rescan = true;
                }
            }
            walletModel->wallet().ConnectScriptPubKeyManNotifiers();

            // Rescan the blockchain using the lowest timestamp
            if (rescan) {
                int64_t scanned_time = walletModel->wallet().RescanFromTime(lowest_timestamp, reserver, true);
                walletModel->wallet().ResubmitWalletTransactions(/*relay=*/false, /*force=*/true);

                if (walletModel->wallet().IsAbortingRescan()) {
                    QMessageBox::critical(this, tr("Misc Error"),
                                          tr("Rescan aborted by user."));

                    if (scanned_time > lowest_timestamp) {
                        std::vector<resultpdi> results = response;
                        response.clear();

                        for (int i = 0; i < ui->entries->count(); ++i) {
                            ImportEntry *entry = qobject_cast<ImportEntry *>(ui->entries->itemAt(i)->widget());

                            // If the descriptor timestamp is within the successfully scanned
                            // range, or if the import result already has an error set, let
                            // the result stand unmodified. Otherwise, replace the result
                            // with an error message.
                            if (scanned_time <= GetImportTimestamp(entry->getDescriptorData().timestamp, now) || !results.at(i).error.empty()) {
                                response.push_back(results.at(i));
                            } else {
                                resultpdi result;
                                result.success = false;
                                result.error = strprintf(
                                            "Rescan failed for descriptor with timestamp %d. There was an error reading a "
                                            "block from time %d, which is after or within %d seconds of key creation, and "
                                            "could contain transactions pertaining to the desc. As a result, transactions "
                                            "and coins using this desc may not appear in the wallet. This error could be "
                                            "caused by pruning or data corruption (see bitcoind log for details) and could "
                                            "be dealt with by downloading and rescanning the relevant blocks (see -reindex "
                                            "option and rescanblockchain RPC).",
                                            GetImportTimestamp(entry->getDescriptorData().timestamp, now),
                                            scanned_time - TIMESTAMP_WINDOW - 1, TIMESTAMP_WINDOW);
                                response.push_back(std::move(result));
                            }
                        }
                    }
                }
            }

            bool success = false;
            for (size_t i = 0; i < response.size(); i++) {
                if (response[i].success) {
                    std::string warning_str;
                    if (response[i].warnings.size() != 0) {
                        warning_str = "\nWarnings: ";
                        for (const auto& w: response[i].warnings) {
                            warning_str += w + "\n";
                        }
                    }
                    QMessageBox::information(this, QString::fromStdString(strprintf("Import Succeeded: input box # %d", i)),
                                             QString::fromStdString("Import Succeeded" + warning_str));
                } else {
                    success = true;
                    QMessageBox::critical(this, QString::fromStdString(strprintf("Error: input box # %d", i)),
                                          QString::fromStdString("Error: " + response[i].error));
                }
            }
            if (success) return;
            break;
        }
    }
    QDialog::accept();
}

ImportEntry *ImportDialog::addEntry()
{
    ImportEntry *entry = nullptr;
    if (page == Page::importMulti) {
        entry = new ImportEntry(ImportEntry::importMultiEntry, this);
    } else {
        entry = new ImportEntry(ImportEntry::importDescriptorsEntry, this);
    }
    entry->setModel(walletModel);
    ui->entries->addWidget(entry);
    connect(entry, &ImportEntry::removeEntry, this, &ImportDialog::removeEntry);

    // Focus the field, so that entry can start immediately
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if(bar)
        bar->setSliderPosition(bar->maximum());

    return entry;
}

void ImportDialog::removeEntry(ImportEntry* entry)
{
    entry->hide();

    // If the last entry is about to be removed add an empty one
    if (ui->entries->count() == 1)
        addEntry();

    entry->deleteLater();
}
