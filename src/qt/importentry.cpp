// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/importentry.h>
#include <qt/forms/ui_importentry.h>

#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>
#include <util/string.h>
#include <wallet/imports.h>

#include <QGroupBox>
#include <QApplication>
#include <QClipboard>

ImportEntry::ImportEntry(EntryPage _entryPage, QWidget *parent) :
        QStackedWidget(parent),
        ui(new Ui::ImportEntry),
        entryPage(_entryPage)
{
    ui->setupUi(this);

    switch (entryPage) {
        case importMultiEntry: {
            setCurrentWidget(ui->importMultiEntry);
            connect(ui->importScriptPubKeyRadio, &QRadioButton::clicked, this, &ImportEntry::changeImportDialog);
            connect(ui->importDescriptorRadio, &QRadioButton::clicked, this, &ImportEntry::changeImportDialog);
            connect(ui->hideScriptsButton, &QPushButton::clicked, this, &ImportEntry::useHideScriptsButtonClicked);
            connect(ui->deleteMultiButton, &QPushButton::clicked, this, &ImportEntry::deleteEntryClicked);

            ui->redeemScriptLabel->hide();
            ui->redeemScript->hide();
            ui->witnessScriptLabel->hide();
            ui->witnessScript->hide();
            ui->descLabelMulti->hide();
            ui->descMulti->hide();

            ui->timestampMulti->setValidator(new Int64_tValidator(0, 0x7FFFFFFFFFFFFFFF, this));
            ui->startRangeMulti->setRange(0, 0x7FFFFFFF);
            ui->endRangeMulti->setRange(0, 0x7FFFFFFF);
            connect(ui->checkboxInternalMulti, &QCheckBox::clicked, this, &ImportEntry::hideLabel);
            break;
        } case importDescriptorsEntry: {
            setCurrentWidget(ui->importDescriptorsEntry);
            ui->nextIndexLabel->hide();
            ui->hiddenMenu->hide();
            ui->timestamp->setValidator(new Int64_tValidator(0, 0x7FFFFFFFFFFFFFFF, this));
            ui->nextIndex->setRange(0, 0x7FFFFFFF);
            ui->startRange->setRange(0, 0x7FFFFFFF);
            ui->endRange->setRange(0, 0x7FFFFFFF);
            connect(ui->hiddenButton, &QPushButton::clicked, this, &ImportEntry::usehiddenButtonClicked);
            connect(ui->deleteDescriptorButton, &QPushButton::clicked, this, &ImportEntry::deleteEntryClicked);
            connect(ui->checkboxInternal, &QCheckBox::clicked, this, &ImportEntry::hideLabelDesc);
            break;
        }
    }
}

ImportEntry::~ImportEntry()
{
    delete ui;
}

void ImportEntry::setModel(WalletModel *_model)
{
    this->model = _model;
}

void ImportEntry::usehiddenButtonClicked()
{
    if (!hiddenButtonState) {
        ui->nextIndexLabel->show();
        ui->hiddenMenu->show();
        hiddenButtonState = true;
        ui->hiddenButton->setText("△");
    } else {
        ui->nextIndexLabel->hide();
        ui->hiddenMenu->hide();
        hiddenButtonState = false;
        ui->hiddenButton->setText("▽");
    }
}

void ImportEntry::useHideScriptsButtonClicked()
{
    if (!hideScriptsButtonState) {
        ui->redeemScriptLabel->show();
        ui->redeemScript->show();

        ui->witnessScriptLabel->show();
        ui->witnessScript->show();
        hideScriptsButtonState = true;
        ui->hideScriptsButton->setText("△");
    } else {
        ui->redeemScriptLabel->hide();
        ui->redeemScript->hide();

        ui->witnessScriptLabel->hide();
        ui->witnessScript->hide();
        hideScriptsButtonState = false;
        ui->hideScriptsButton->setText("▽");
    }
}

void ImportEntry::hideLabel()
{
    if (ui->checkboxInternalMulti->isChecked()) {
        ui->labelLabelMulti->hide();
        ui->labelMulti->hide();
    } else {
        ui->labelLabelMulti->show();
        ui->labelMulti->show();
    }
}

void ImportEntry::hideLabelDesc()
{
    if (ui->checkboxInternal->isChecked()) {
        ui->labelLabel->hide();
        ui->label->hide();
    } else {
        ui->labelLabel->show();
        ui->label->show();
    }
}

void ImportEntry::changeImportDialog()
{
    if (ui->importScriptPubKeyRadio->isChecked()) {
        ui->scriptPubKeyLabel->show();
        ui->scriptPubKey->show();
        ui->hideScriptsButton->show();

        ui->redeemScriptLabel->hide();
        ui->redeemScript->hide();

        ui->witnessScriptLabel->hide();
        ui->witnessScript->hide();

        ui->publicKeyLabel->show();
        ui->publicKey->show();

        ui->descLabelMulti->hide();
        ui->descMulti->hide();
    } else {
        ui->scriptPubKeyLabel->hide();
        ui->scriptPubKey->hide();
        ui->hideScriptsButton->hide();

        ui->redeemScriptLabel->hide();
        ui->redeemScript->hide();

        ui->witnessScriptLabel->hide();
        ui->witnessScript->hide();

        ui->publicKeyLabel->hide();
        ui->publicKey->hide();
        ui->descLabelMulti->show();
        ui->descMulti->show();
    }
}

wallet::ImportMultiData ImportEntry::getMultiData()
{
    wallet::ImportMultiData multiData;

    if (ui->importScriptPubKeyRadio->isChecked()) {
        multiData.scriptPubKey = ui->scriptPubKey->text().toStdString();
        multiData.redeem_script = ui->redeemScript->text().toStdString();
        multiData.witness_script = ui->witnessScript->text().toStdString();
        if (!ui->publicKey->text().isEmpty()) {
            multiData.public_keys = SplitString(ui->publicKey->text().toStdString(), ',');
        }
    } else {
        multiData.range_start = ui->startRangeMulti->text().toInt();
        multiData.range_end = ui->endRangeMulti->text().toInt();
    }
    if (!ui->checkboxInternalMulti->isChecked()) {
        multiData.label = ui->labelMulti->text().toStdString();
    }
    if (!ui->privateKey->text().isEmpty()) {
        multiData.private_keys = SplitString(ui->privateKey->text().toStdString(), ',');
    }
    if (!ui->timestampMulti->text().toStdString().empty()) {
        multiData.timestamp = ui->timestampMulti->text().toInt();
    }

    multiData.internal = (ui->checkboxInternalMulti->checkState() == Qt::Checked);
    multiData.watch_only = (ui->checkboxWatchOnlyMulti->checkState() == Qt::Checked);
    multiData.keypool = (ui->checkboxKeyPoolMulti->checkState() == Qt::Checked);

    return multiData;
}

wallet::ImportDescriptorData ImportEntry::getDescriptorData()
{
    wallet::ImportDescriptorData descriptorData;
    descriptorData.label = ui->label->text().toStdString();
    if (ui->startRange->text().toInt() != 0) {
        descriptorData.range_start = ui->startRange->text().toInt();
    }
    if (ui->endRange->text().toInt() != 0) {
        descriptorData.range_end = ui->endRange->text().toInt();
    }
    if (ui->nextIndex->text().toInt() != 0) {
        descriptorData.next_index = ui->nextIndex->text().toInt();
    }
    if (!ui->timestamp->text().toStdString().empty()) {
        descriptorData.timestamp = ui->timestamp->text().toInt();
    }
    descriptorData.active = (ui->checkboxActive->checkState() == Qt::Checked);
    descriptorData.internal = (ui->checkboxInternal->checkState() == Qt::Checked);

    return descriptorData;
}

std::string ImportEntry::getDesc()
{
    if (entryPage == EntryPage::importMultiEntry) {
        return ui->desccMulti->text().toStdString();
    } else {
        return ui->descriptor->text().toStdString();
    }
}

void ImportEntry::deleteEntryClicked()
{
    Q_EMIT removeEntry(this);
}
