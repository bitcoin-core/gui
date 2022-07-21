#include <qt/addressinfotablemodel.h>

#include <qt/walletmodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <QWidget>
#include <QAbstractTableModel>

#include <wallet/wallet.h>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QTableView;
QT_END_NAMESPACE

AddressInfoTableModel::AddressInfoTableModel(WalletModel *parent) :
        QAbstractTableModel(parent), m_wallet_model(parent)
{
    // Load entries from wallet
    for (const wallet::AddressInfo& address_info : parent->wallet().listAddresses()) {
        m_address_list.append(address_info);
    }
}

int AddressInfoTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_address_list.length();
}

int AddressInfoTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 8;
}

QVariant AddressInfoTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return  QString::number(m_address_list[index.row()].index);
        } else if (index.column() == 1) {
            return QString::fromStdString(m_address_list[index.row()].address);
        } else if (index.column() == 2) {
            auto label = QString::fromStdString(m_address_list[index.row()].label);
            if(label.isEmpty()) {
                return tr("(no label)");
            } else {
                return label;
            }
        } else if (index.column() == 3) {
            return QString::fromStdString(m_address_list[index.row()].internal ? "change" : "receiving");
        } else if (index.column() == 4) {
            return QString::fromStdString(m_address_list[index.row()].output_type);
        } else if (index.column() == 5) {
            return QString::fromStdString(m_address_list[index.row()].hdkeypath);
        } else if (index.column() == 6) {
            BitcoinUnit display_unit = m_wallet_model->getOptionsModel()->getDisplayUnit();
            CAmount balance = m_address_list[index.row()].amount;
            return BitcoinUnits::format(display_unit, balance, false, BitcoinUnits::SeparatorStyle::ALWAYS);
            //  QString::number(m_address_list[index.row()].balance);
        } else if (index.column() == 7) {
            return QString::number(m_address_list[index.row()].tx_count);
        }
    }

    if (role == Qt::ForegroundRole) {
        if(index.column() == 2)
        {
            auto label = QString::fromStdString(m_address_list[index.row()].label);
            if(label.isEmpty()) return COLOR_BAREADDRESS;
            return QVariant();
        }
    }

    if (role == Qt::BackgroundRole) {

        auto tx_count = m_address_list[index.row()].tx_count;
        auto color = (tx_count > 0) ? QColor(255, 0, 0, 64) : QColor(0, 255, 0, 64);

        if(index.column() == 1 || index.column() == 7)
        {
            return color;
        }

    }

    return QVariant();
}

QVariant AddressInfoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section == 0) {
            return {};
        } else if (section == 1) {
            return QString("Address");
        } else if (section == 2) {
            return QString("Label");
        } else if (section == 3) {
            return QString("Source");
        } else if (section == 4) {
            return QString("Type");
        } else if (section == 5) {
            return QString("HD Path");
        } else if (section == 6) {
            return QString("Balance");
        } else if (section == 7) {
            return QString("Tx");
        }
    }
    return QVariant();
}


void AddressInfoTableModel::addAddressInfo(const wallet::AddressInfo &address_info)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_address_list.prepend(address_info);
    endInsertRows();
}

void AddressInfoTableModel::refresh()
{
    beginRemoveRows( QModelIndex(), 0, m_address_list.size() - 1);
    m_address_list.clear();
    endRemoveRows();

    std::vector<wallet::AddressInfo> addr_info_vector = m_wallet_model->wallet().listAddresses();
    beginInsertRows(QModelIndex(), 0, addr_info_vector.size() - 1);
    for (const wallet::AddressInfo& address_info : addr_info_vector) {
        m_address_list.append(address_info);
    }
    endInsertRows();
}
