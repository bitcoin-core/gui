// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/bumpfeechoosechangedialog.h>
#include <qt/forms/ui_bumpfeechoosechangedialog.h>

#include <addresstype.h>
#include <key_io.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

BumpfeeChooseChangeDialog::BumpfeeChooseChangeDialog(WalletModel *model, QWidget *parent, const Txid& txid) :
    QDialog(parent, GUIUtil::dialog_flags),
    ui(new Ui::BumpfeeChooseChangeDialog),
    model(model)
{
    ui->setupUi(this);

    bool found_change = false;
    CTransactionRef tx = model->wallet().getTx(txid);
    for (size_t i = 0; i < tx->vout.size(); ++i) {
        const CTxOut& txout = tx->vout.at(i);
        QString address_info = tr("No address decoded");
        CTxDestination dest;
        if (ExtractDestination(txout.scriptPubKey, dest)) {
            std::string address = EncodeDestination(dest);
            std::string label;
            if (model->wallet().getAddress(dest, &label, nullptr) && !label.empty()) {
                address_info = QString::fromStdString(label) + QString(" (") + QString::fromStdString(address) + QString(")");
            } else {
                address_info = QString::fromStdString(address);
            }
        }
        QString output_info = tr("%1: %2 to %3").arg(i).arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), txout.nValue)).arg(address_info);

        QRadioButton *radio_button = new QRadioButton(output_info, nullptr);
        radio_button->setObjectName(QString::number(i) + QString("_radioButton"));
        ui->verticalLayout->addWidget(radio_button);

        if (!found_change && model->wallet().isChange(txout)) {
            radio_button->setChecked(true);
            ui->none_radioButton->setChecked(false);
            found_change = true;
        }
    }
    GUIUtil::handleCloseWindowShortcut(this);
}

std::optional<uint32_t> BumpfeeChooseChangeDialog::GetSelectedOutput()
{
    for (int i = 0; i < ui->verticalLayout->count(); ++i) {
        QRadioButton* child = dynamic_cast<QRadioButton*>(ui->verticalLayout->itemAt(i)->widget());
        if (child->isChecked()) {
            if (i == 0) {
                // "None" option selected
                return std::nullopt;
            }
            // Return the output index, offset by one for the "None" option at index 0
            return static_cast<uint32_t>(i - 1);
        }
    }
    return std::nullopt;
}
