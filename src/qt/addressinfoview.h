// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ADDRESSINFOVIEW_H
#define BITCOIN_QT_ADDRESSINFOVIEW_H

#include <qt/guiutil.h>
#include <QWidget>
#include <QAbstractTableModel>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QTableView;
QT_END_NAMESPACE

/**
 * Widget showing the address list for a wallet.
 */
class AddressInfoView : public QWidget
{
    Q_OBJECT

public:
    explicit AddressInfoView(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~AddressInfoView();

    void setModel(WalletModel *model);

    enum ColumnWidths {
        INDEX_COLUMN_WIDTH = 25,
        ADDRESS_COLUMN_WIDTH = 523,
        LABEL_COLUMN_WIDTH = 50,
        SOURCE_COLUMN_WIDTH = 120,
        TYPE_COLUMN_WIDTH = 120,
        HD_PATH_COLUMN_WIDTH = 120,
        BALANCE_MINIMUM_COLUMN_WIDTH = 120,
        TX_MINIMUM_COLUMN_WIDTH = 25,
        MINIMUM_COLUMN_WIDTH = 25
    };

    WalletModel *m_wallet_model{nullptr};
    QTableView *m_address_view{nullptr};

    const PlatformStyle* m_platform_style;
};

#endif // BITCOIN_QT_ADDRESSINFOVIEW_H
