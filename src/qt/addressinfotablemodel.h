
// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ADDRESSINFOTABLEMODEL_H
#define BITCOIN_QT_ADDRESSINFOTABLEMODEL_H

#include <qt/guiutil.h>
#include <QWidget>
#include <QAbstractTableModel>

#include <wallet/wallet.h>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QTableView;
QT_END_NAMESPACE

namespace interfaces {
class Wallet;
}

namespace wallet {
struct AddressInfo;
}

class AddressInfoTableModel : public QAbstractTableModel
{
    Q_OBJECT;
public:
    explicit AddressInfoTableModel(WalletModel *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;

    int columnCount(const QModelIndex &parent) const override;

    QVariant data(const QModelIndex &index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addAddressInfo(const wallet::AddressInfo &address_info);

    void refresh();
private:
    QList<wallet::AddressInfo> m_address_list;

    WalletModel *m_wallet_model;
};

#endif // BITCOIN_QT_ADDRESSINFOTABLEMODEL_H
