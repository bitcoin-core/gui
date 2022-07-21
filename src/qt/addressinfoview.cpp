#include <qt/addressinfoview.h>

#include <qt/addressinfotablemodel.h>

#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QTableView>
#include <QScrollBar>
#include <QSettings>

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QList>

AddressInfoView::AddressInfoView(const PlatformStyle *platformStyle, QWidget *parent)
    : QWidget(parent), m_platform_style{platformStyle}
{
    setContentsMargins(0,0,0,0);

    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);

    if (platformStyle->getUseExtraSpacing()) {
        hlayout->setSpacing(5);
        hlayout->addSpacing(26);
    } else {
        hlayout->setSpacing(0);
        hlayout->addSpacing(23);
    }

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0,0,0,0);
    vlayout->setSpacing(0);

    m_address_view = new QTableView(this);
    m_address_view->setObjectName("m_address_view");

    vlayout->addLayout(hlayout);
    vlayout->addWidget(m_address_view);
    vlayout->setSpacing(0);

    int width = m_address_view->verticalScrollBar()->sizeHint().width();
    // Cover scroll bar width with spacing
    if (platformStyle->getUseExtraSpacing()) {
        hlayout->addSpacing(width+2);
    } else {
        hlayout->addSpacing(width);
    }
    m_address_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_address_view->setTabKeyNavigation(false);
    m_address_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_address_view->installEventFilter(this);
    m_address_view->setAlternatingRowColors(true);
    m_address_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_address_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_address_view->setSortingEnabled(true);
    m_address_view->verticalHeader()->hide();

    QSettings settings;
    if (!m_address_view->horizontalHeader()->restoreState(settings.value("AddressViewHeaderState").toByteArray())) {
        m_address_view->horizontalHeader()->setMinimumSectionSize(MINIMUM_COLUMN_WIDTH);
        // m_address_view->setColumnWidth(0, 23);
        // m_address_view->setColumnWidth(1, 230);
        // m_address_view->setColumnWidth(2, 120);
        // m_address_view->setColumnWidth(3, 120);
        // m_address_view->setColumnWidth(4, 80);

        m_address_view->horizontalHeader()->setStretchLastSection(true);
    }
}

void AddressInfoView::setModel(WalletModel *_model)
{
    this->m_wallet_model = _model;
    if(_model)
    {
        AddressInfoTableModel *address_list_table_model = _model->getAddressInfoTableModel();
        m_address_view->setModel(address_list_table_model);

        m_address_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_address_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    }
}

AddressInfoView::~AddressInfoView()
{
    QSettings settings;
    settings.setValue("AddressViewHeaderState", m_address_view->horizontalHeader()->saveState());
}
