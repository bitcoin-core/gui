// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TOOLSDIALOG_H
#define BITCOIN_QT_TOOLSDIALOG_H

#include <QDialog>
#include <QWidget>
#include <interfaces/node.h>

namespace Ui {
    class ToolsDialog;
}

/** Tools dialog box */
class ToolsDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        GetRawTransactionMode
    };

    explicit ToolsDialog(QWidget *parent, Mode _mode, interfaces::Node* _node = nullptr);
    ~ToolsDialog();

    QString txid() const;
    bool isVerboseChecked() const;
    QString blockhash() const;
    void getrawtransactionRPC(std::string txid, bool verbose, std::string blockhash, std::string& result);

private:
    Ui::ToolsDialog *ui;
    QString text;
    Mode mode;
    interfaces::Node* node;

private Q_SLOTS:
    void onSubmitForm();
};

#endif // BITCOIN_QT_TOOLSDIALOG_H
