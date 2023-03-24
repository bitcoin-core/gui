// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RECENTREQUESTSTABLEMODEL_H
#define BITCOIN_QT_RECENTREQUESTSTABLEMODEL_H

#include <interfaces/wallet.h>

#include <qt/platformstyle.h>
#include <qt/sendcoinsrecipient.h>

#include <optional>
#include <string>

#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>

class WalletModel;

class RecentRequestEntry
{
public:
    RecentRequestEntry() : nVersion(RecentRequestEntry::CURRENT_VERSION) {}

    static const int CURRENT_VERSION = 1;
    int nVersion;
    int64_t id{0};
    QDateTime date;
    SendCoinsRecipient recipient;

    bool m_is_active{false}; // memory only

    SERIALIZE_METHODS(RecentRequestEntry, obj) {
        unsigned int date_timet;
        SER_WRITE(obj, date_timet = obj.date.toSecsSinceEpoch());
        READWRITE(obj.nVersion, obj.id, date_timet, obj.recipient);
        SER_READ(obj, obj.date = QDateTime::fromSecsSinceEpoch(date_timet));
    }

    [[nodiscard]] QString GetAddressWarnings() const;
};

class RecentRequestEntryLessThan
{
public:
    RecentRequestEntryLessThan(int nColumn, Qt::SortOrder fOrder):
        column(nColumn), order(fOrder) {}
    bool operator()(const RecentRequestEntry& left, const RecentRequestEntry& right) const;

private:
    int column;
    Qt::SortOrder order;
};

/** Model for list of recently generated payment requests / bitcoin: URIs.
 * Part of wallet model.
 */
class RecentRequestsTableModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit RecentRequestsTableModel(const PlatformStyle* platformStyle, WalletModel* parent);
    ~RecentRequestsTableModel();

    enum ColumnIndex {
        Date = 0,
        Warnings,
        Label,
        Message,
        Amount,
        NUMBER_OF_COLUMNS
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    /*@}*/

    const RecentRequestEntry &entry(int row) const { return list[row]; }
    std::optional<RecentRequestEntry> addNewRequest(const SendCoinsRecipient& recipient);
    std::optional<RecentRequestEntry> addNewRequest(const interfaces::ReceiveRequest& recipient);
    std::optional<RecentRequestEntry> addNewRequest(RecentRequestEntry& recipient);

public Q_SLOTS:
    void updateDisplayUnit();

private:
    WalletModel *walletModel;
    QStringList columns;
    QList<RecentRequestEntry> list;
    int64_t nReceiveRequestsMaxId{0};
    const PlatformStyle* platformStyle;

    /** Updates the column title to "Amount (DisplayUnit)" and emits headerDataChanged() signal for table headers to react. */
    void updateAmountColumnTitle();
    /** Gets title for amount column including current display unit if optionsModel reference available. */
    QString getAmountTitle();
};

#endif // BITCOIN_QT_RECENTREQUESTSTABLEMODEL_H
