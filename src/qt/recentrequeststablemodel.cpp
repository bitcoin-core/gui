// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/recentrequeststablemodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <clientversion.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <streams.h>
#include <util/string.h>

#include <utility>

#include <QIcon>
#include <QLatin1Char>
#include <QLatin1String>

[[nodiscard]] QString RecentRequestEntry::GetAddressWarnings() const
{
    QString warnings;
    if (!m_is_active) {
        warnings += QObject::tr("This address should not be used. It was derived from an inactive seed, was imported, or may have been stored unencrypted.\n");
    }

    return warnings;
}

RecentRequestsTableModel::RecentRequestsTableModel(const PlatformStyle* platformStyle, WalletModel* parent) : QAbstractTableModel(parent), walletModel(parent)
{
    // Load entries from wallet
    for (const interfaces::ReceiveRequest& request : parent->wallet().getAddressReceiveRequests()) {
        addNewRequest(request);
    }

    /* These columns must match the indices in the ColumnIndex enumeration */
    columns << tr("Date") << tr("Warnings") << tr("Label") << tr("Message") << getAmountTitle();

    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &RecentRequestsTableModel::updateDisplayUnit);
}

RecentRequestsTableModel::~RecentRequestsTableModel() = default;

int RecentRequestsTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return list.length();
}

int RecentRequestsTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return columns.length();
}

QVariant RecentRequestsTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= list.length())
        return QVariant();

    const RecentRequestEntry* rec = &list[index.row()];
    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Warnings:
            return {};
        case Date:
            return GUIUtil::dateTimeStr(rec->date);
        case Label:
            if(rec->recipient.label.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->recipient.label;
            }
        case Message:
            if(rec->recipient.message.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no message)");
            }
            else
            {
                return rec->recipient.message;
            }
        case Amount:
            if (rec->recipient.amount == 0 && role == Qt::DisplayRole)
                return tr("(no amount requested)");
            else if (role == Qt::EditRole)
                return BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), rec->recipient.amount, false, BitcoinUnits::SeparatorStyle::NEVER);
            else
                return BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), rec->recipient.amount);
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == Amount)
            return (int)(Qt::AlignRight|Qt::AlignVCenter);
    } else if (role == Qt::DecorationRole) {
        if (index.column() == Warnings) {
            if (rec->GetAddressWarnings().isEmpty()) {
                return {};
            } else {
                return platformStyle->TextColorIcon(QIcon(":/icons/warning"));
            }
        }
    } else if (role == Qt::ToolTipRole) {
        if (index.column() == Warnings) return rec->GetAddressWarnings();
    }
    return QVariant();
}

bool RecentRequestsTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return true;
}

QVariant RecentRequestsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

/** Updates the column title to "Amount (DisplayUnit)" and emits headerDataChanged() signal for table headers to react. */
void RecentRequestsTableModel::updateAmountColumnTitle()
{
    columns[Amount] = getAmountTitle();
    Q_EMIT headerDataChanged(Qt::Horizontal,Amount,Amount);
}

/** Gets title for amount column including current display unit if optionsModel reference available. */
QString RecentRequestsTableModel::getAmountTitle()
{
    if (!walletModel->getOptionsModel()) return {};
    return tr("Requested") +
           QLatin1String(" (") +
           BitcoinUnits::shortName(this->walletModel->getOptionsModel()->getDisplayUnit()) +
           QLatin1Char(')');
}

QModelIndex RecentRequestsTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return createIndex(row, column);
}

bool RecentRequestsTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);

    if(count > 0 && row >= 0 && (row+count) <= list.size())
    {
        for (int i = 0; i < count; ++i)
        {
            const RecentRequestEntry* rec = &list[row+i];
            if (!walletModel->wallet().setAddressReceiveRequest(DecodeDestination(rec->recipient.address.toStdString()), ToString(rec->id), ""))
                return false;
        }

        beginRemoveRows(parent, row, row + count - 1);
        list.erase(list.begin() + row, list.begin() + row + count);
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}

Qt::ItemFlags RecentRequestsTableModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

// called when adding a request from the GUI
std::optional<RecentRequestEntry> RecentRequestsTableModel::addNewRequest(const SendCoinsRecipient& recipient)
{
    RecentRequestEntry newEntry;
    newEntry.id = ++nReceiveRequestsMaxId;
    newEntry.date = QDateTime::currentDateTime();
    newEntry.recipient = recipient;

    DataStream ss{};
    ss << newEntry;

    if (!walletModel->wallet().setAddressReceiveRequest(DecodeDestination(recipient.address.toStdString()), ToString(newEntry.id), ss.str()))
        return std::nullopt;

    // If we are using the GUI to get a new receive address,
    // the key must be active (derived from active seed or descriptor)
    newEntry.m_is_active = true;

    addNewRequest(newEntry);
    return newEntry;
}

// called from ctor when loading from wallet
std::optional<RecentRequestEntry> RecentRequestsTableModel::addNewRequest(const interfaces::ReceiveRequest& recipient)
{
    std::vector<uint8_t> data(recipient.m_data.begin(), recipient.m_data.end());
    DataStream ss{data};

    RecentRequestEntry entry;
    ss >> entry;

    if (entry.id == 0) // should not happen
        return std::nullopt;

    if (entry.id > nReceiveRequestsMaxId)
        nReceiveRequestsMaxId = entry.id;

    entry.m_is_active = recipient.m_is_active;

    addNewRequest(entry);
    return entry;
}

// actually add to table in GUI
std::optional<RecentRequestEntry> RecentRequestsTableModel::addNewRequest(RecentRequestEntry& recipient)
{
    beginInsertRows(QModelIndex(), 0, 0);
    list.prepend(recipient);
    endInsertRows();
    return recipient;
}

void RecentRequestsTableModel::sort(int column, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(), RecentRequestEntryLessThan(column, order));
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(list.size() - 1, NUMBER_OF_COLUMNS - 1, QModelIndex()));
}

void RecentRequestsTableModel::updateDisplayUnit()
{
    updateAmountColumnTitle();
}

bool RecentRequestEntryLessThan::operator()(const RecentRequestEntry& left, const RecentRequestEntry& right) const
{
    const RecentRequestEntry* pLeft = &left;
    const RecentRequestEntry* pRight = &right;
    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case RecentRequestsTableModel::Date:
        return pLeft->date.toSecsSinceEpoch() < pRight->date.toSecsSinceEpoch();
    case RecentRequestsTableModel::Label:
        return pLeft->recipient.label < pRight->recipient.label;
    case RecentRequestsTableModel::Message:
        return pLeft->recipient.message < pRight->recipient.message;
    case RecentRequestsTableModel::Amount:
        return pLeft->recipient.amount < pRight->recipient.amount;
    default:
        return pLeft->id < pRight->id;
    }
}
