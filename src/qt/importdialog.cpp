#include <qt/importdialog.h>
#include <qt/forms/ui_importdialog.h>

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
        }
    }

    GUIUtil::handleCloseWindowShortcut(this);
}

ImportDialog::~ImportDialog()
{
    delete ui;
}

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
            break;
        }
    }
    QMessageBox::information(this, tr("Import Succeeded"), tr("Import Succeeded"));
    QDialog::accept();
}
