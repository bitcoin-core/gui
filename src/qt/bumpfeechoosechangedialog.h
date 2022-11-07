// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BUMPFEECHOOSECHANGEDIALOG_H
#define BITCOIN_QT_BUMPFEECHOOSECHANGEDIALOG_H

#include <QDialog>
#include <optional>

#include <primitives/transaction_identifier.h>

class WalletModel;
class uint256;

namespace Ui {
    class BumpfeeChooseChangeDialog;
}

/** Dialog for choosing the change output when bumping fee
 */
class BumpfeeChooseChangeDialog : public QDialog
{
    Q_OBJECT

public:

    explicit BumpfeeChooseChangeDialog(WalletModel *model, QWidget *parent, const Txid& txid);
    std::optional<uint32_t> GetSelectedOutput();

private:
    Ui::BumpfeeChooseChangeDialog *ui;
    WalletModel *model;
};

#endif // BITCOIN_QT_BUMPFEECHOOSECHANGEDIALOG_H
