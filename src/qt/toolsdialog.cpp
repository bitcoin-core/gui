// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/toolsdialog.h>
#include <qt/forms/ui_toolsdialog.h>

#include <qt/guiutil.h>

#include <univalue.h>
#include <rpc/client.h>

#include <QString>
#include <QTextDocument>
#include <QTextCursor>

#include <string>
#include <vector>

/** Tools dialog box */
ToolsDialog::ToolsDialog(QWidget *parent, Mode _mode, interfaces::Node* _node) :
    QDialog(parent, GUIUtil::dialog_flags),
    ui(new Ui::ToolsDialog),
    mode(_mode),
    node(_node)
{
    ui->setupUi(this);

    switch (mode) {
        case GetRawTransactionMode:
        {
            setWindowTitle(tr("getrawtransaction"));
            break;
        }
    }

    GUIUtil::handleCloseWindowShortcut(this);
}

ToolsDialog::~ToolsDialog()
{
    delete ui;
}

QString ToolsDialog::txid() const
{
    return ui->txidEdit->text();
}

bool ToolsDialog::isVerboseChecked() const
{
    return ui->verboseCheckbox->isChecked();
}

QString ToolsDialog::blockhash() const
{
    return ui->blockHashEdit->text();
}

void ToolsDialog::onSubmitForm()
{
    std::string transaction_id = txid().toUtf8().constData();
    bool verbose = isVerboseChecked();
    std::string block_hash = blockhash().toUtf8().constData();
    std::string result;

    getrawtransactionRPC(transaction_id, verbose, block_hash, result);

    QTextDocument *document = ui->helpMessage->document();
    document->clear();
    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::Start);
    cursor.insertText(QString::fromStdString(result));
}

void ToolsDialog::getrawtransactionRPC(std::string txid, bool verbose, std::string blockhash, std::string& result)
{
    std::string command {"getrawtransaction"};
    std::vector<std::string> args {txid};
    args.emplace_back(verbose ? "true" : "false");
    if (!blockhash.empty()) {
        args.emplace_back(blockhash);
    }
    UniValue params {RPCConvertValues(command, args)};
    UniValue lastResult;

    try {
        assert(node);
        lastResult = node->executeRpc(command, params, "");

        if (lastResult.isStr())
            result = lastResult.get_str();
        else
            result = lastResult.write(2);
    } catch (std::exception& e) {
        result = "Error: " + std::string(e.what());
    } catch (...) {
        result = "No such mempool transaction. Use -txindex or provide a block hash to enable blockchain transaction queries. Use gettransaction for wallet transactions";
    }
}