// Copyright (c) 2022- The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/deniabilitydialog.h>
#include <qt/forms/ui_deniabilitydialog.h>

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <common/args.h>
#include <consensus/validation.h>
#include <external_signer.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <net.h>
#include <node/interface_ui.h>
#include <primitives/transaction.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactiontablemodel.h>
#include <random.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/coinselection.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>

#include <QCheckBox>
#include <QDateTime>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QString>
#include <QTimer>

#include <fstream>

using common::PSBTError;
using interfaces::Wallet;

enum {
    COLUMN_CHECKBOX,
    COLUMN_DESTINATION,
    COLUMN_UTXO_COUNT,
    COLUMN_AMOUNT,
    COLUMN_DENIABILIZATION_CYCLES,
    COLUMN_ESTIMATED_FEE,
    COLUMN_COUNT
};

constexpr CAmount MAX_DENIABILIZATION_BUDGET = 100000; // 1 mBTC

static bool hasExternalSigner(Wallet& wallet)
{
#ifdef ENABLE_EXTERNAL_SIGNER
    if (wallet.hasExternalSigner()) {
        return true;
    }
#endif
    return false;
}

static bool externalSignerConnected()
{
    std::vector<ExternalSigner> signers;
#ifdef ENABLE_EXTERNAL_SIGNER
    const std::string command = gArgs.GetArg("-signer", "");
    if (command.empty())
        return false;
    try {
        ExternalSigner::Enumerate(command, signers, Params().GetChainTypeString());
    } catch (const std::runtime_error& e) {
        (void)e.what();
    }
#endif
    return signers.size() == 1;
}

CScript DeniabilityDialog::CoinInfo::scriptPubKey() const
{
    Assert(!utxos.empty());
    return utxos.front().walletTxOut.txout.scriptPubKey;
}

CTxDestination DeniabilityDialog::CoinInfo::destination() const
{
    CTxDestination destination = CNoDestination();
    if (!ExtractDestination(scriptPubKey(), destination)) {
        // For backwards compatibility, we convert P2PK output scripts into PKHash destinations
        if (auto pubKeyDestination = std::get_if<PubKeyDestination>(&destination)) {
            destination = PKHash(pubKeyDestination->GetPubKey());
        }
    }
    return destination;
}

uint256 DeniabilityDialog::CoinInfo::hash() const
{
    HashWriter hasher{};
    for (const auto& utxo : utxos) {
        hasher << utxo.outpoint.hash;
        hasher << utxo.outpoint.n;
    }
    return hasher.GetSHA256();
}

CAmount DeniabilityDialog::CoinInfo::value() const
{
    CAmount amount = 0;
    for (const auto& utxo : utxos) {
        amount += utxo.walletTxOut.txout.nValue;
    }
    return amount;
}

int DeniabilityDialog::CoinInfo::depthInMainChain() const
{
    int depth = INT_MAX;
    for (const auto& utxo : utxos) {
        depth = std::min(depth, utxo.walletTxOut.depth_in_main_chain);
    }
    return depth;
}

uint DeniabilityDialog::CoinInfo::deniabilizationCycles() const
{
    uint cycles = UINT_MAX;
    for (const auto& utxo : utxos) {
        cycles = std::min(cycles, utxo.deniabilizationStats.cycles);
    }
    return cycles;
}

bool DeniabilityDialog::CoinInfo::allUTXOsAreBlockReward() const
{
    for (const auto& utxo : utxos) {
        if (!utxo.deniabilizationStats.blockReward) {
            return false;
        }
    }
    return true;
}

bool DeniabilityDialog::CoinInfo::anyLockedCoin(interfaces::Wallet& wallet) const
{
    for (const auto& utxo : utxos) {
        if (wallet.isLockedCoin(utxo.outpoint)) {
            return true;
        }
    }
    return false;
}

DeniabilityDialog::DeniabilityDialog(const PlatformStyle* platformStyle, QWidget* parent) : QDialog(parent, GUIUtil::dialog_flags),
                                                                                            m_ui(new Ui::DeniabilityDialog),
                                                                                            m_platformStyle(platformStyle)
{
    m_ui->setupUi(this);

    setupTableWidget();

    m_ui->budgetSpinner->setDisplayUnit(BitcoinUnit::SAT);
    m_ui->budgetSpinner->SetMaxValue(MAX_DENIABILIZATION_BUDGET);
    m_ui->budgetSpinner->setSingleStep(1000);

    m_deniabilizeProcTimer = new QTimer(this);
    connect(m_deniabilizeProcTimer, SIGNAL(timeout()), this, SLOT(deniabilizeProc()));

    m_contextMenu = new QMenu(this);
    m_contextMenu->setObjectName("contextMenuDeniability");

    QAction* copyAddress = new QAction(tr("Copy Address"), this);
    m_contextMenu->addAction(copyAddress);

    connect(copyAddress, &QAction::triggered, this, [this]() {
        auto selectionModel = m_ui->tableWidgetCoins->selectionModel();
        if (!selectionModel)
            return;
        QModelIndexList selection = selectionModel->selectedRows();
        if (!selection.isEmpty()) {
            GUIUtil::setClipboard(selection.at(0).data(destinationRole).toString());
        }
    });

    QAction* copyTxHash = new QAction(tr("Copy Transaction ID"), this);
    m_contextMenu->addAction(copyTxHash);

    connect(copyTxHash, &QAction::triggered, this, [this]() {
        auto selectionModel = m_ui->tableWidgetCoins->selectionModel();
        if (!selectionModel)
            return;
        QModelIndexList selection = selectionModel->selectedRows();
        if (!selection.isEmpty()) {
            std::string destinationStr = selection.at(0).data(destinationRole).toString().toStdString();
            CTxDestination destination = DecodeDestination(destinationStr);
            for (auto& coin : m_coinsList) {
                if (coin.destination() == destination) {
                    QString hashStr = QString::fromStdString(coin.utxos.front().outpoint.hash.GetHex());
                    GUIUtil::setClipboard(hashStr);
                    break;
                }
            }
        }
    });
}

DeniabilityDialog::~DeniabilityDialog()
{
    m_deniabilizeProcTimer->stop();
    delete m_deniabilizeProcTimer;
    m_deniabilizeProcTimer = nullptr;

    saveSettings();

    delete m_ui;
}

void DeniabilityDialog::setupTableWidget()
{
    static_assert(COLUMN_COUNT == 6, "Update the header names below for any change in columns");
    QStringList headerLables;
    headerLables << "";
    headerLables << tr("Address");
    headerLables << tr("UTXO Count");
    headerLables << tr("Amount") + " (" + BitcoinUnits::shortName(m_displayUnit) + ")";
    headerLables << tr("Cycles");
    headerLables << tr("Estimated Fees") + " (" + BitcoinUnits::shortName(BitcoinUnits::Unit::SAT) + ")";

    // Setup coin table
    m_ui->tableWidgetCoins->setColumnCount(COLUMN_COUNT);
    m_ui->tableWidgetCoins->setHorizontalHeaderLabels(headerLables);
    m_ui->tableWidgetCoins->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

    // Resize cells (in a backwards compatible way)
#if QT_VERSION < 0x050000
    m_ui->tableWidgetCoins->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#else
    m_ui->tableWidgetCoins->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif
    m_ui->tableWidgetCoins->horizontalHeader()->setStretchLastSection(false);
    m_ui->tableWidgetCoins->verticalHeader()->setVisible(false);

    // Select rows
    m_ui->tableWidgetCoins->setSelectionBehavior(QAbstractItemView::SelectRows);

    // connect selection/deselection
    connect(m_ui->tableWidgetCoins, &QTableWidget::itemClicked, this, &DeniabilityDialog::updateCheckState);

    // Apply custom context menu
    m_ui->tableWidgetCoins->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect context menus
    connect(m_ui->tableWidgetCoins, &QWidget::customContextMenuRequested, this, &DeniabilityDialog::contextualMenu);
}

void DeniabilityDialog::contextualMenu(const QPoint& point)
{
    QModelIndex index = m_ui->tableWidgetCoins->indexAt(point);
    if (index.isValid() && index.column() == COLUMN_DESTINATION) {
        m_contextMenu->popup(m_ui->tableWidgetCoins->viewport()->mapToGlobal(point));
    }
}

void DeniabilityDialog::loadSettings()
{
    // we don't store settings if there's no wallet name to avoid contaminating settings between different unnamed wallets
    if (m_walletName.empty()) {
        m_deniabilizationBudget = 0;
        m_ui->budgetSpinner->setValue(m_deniabilizationBudget);
        m_deniabilizationFrequency = std::chrono::hours(24);
        m_ui->dailyRadioButton->setChecked(true);
        m_nextDeniabilizationCycle.reset();
        m_lastDeniabilizationTxHash.reset();
        m_coinStateMap.clear();
        return;
    }

    Assert(m_model);
    Wallet& wallet = m_model->wallet();

    QSettings settings;
    settings.beginGroup("Deniability[" + QString::fromStdString(m_walletName) + "]");

    if (!settings.contains("nDeniabilizationBudget")) {
        settings.setValue("nDeniabilizationBudget", (qint64)0);
    }
    if (!settings.contains("nDeniabilizationFrequency")) {
        settings.setValue("nDeniabilizationFrequency", (quint64)(60 * 60 * 24)); // 60 seconds to a minute, 60 minutes to an hour, 24 hours to a day
    }
    if (!settings.contains("nNextDeniabilizationCycle")) {
        settings.setValue("nNextDeniabilizationCycle", (quint64)0);
    }
    if (!settings.contains("fDeniabilizationProcessAccepted")) {
        settings.setValue("fDeniabilizationProcessAccepted", false);
    }
    if (!settings.contains("sLastDeniabilizationTxHash")) {
        settings.setValue("sLastDeniabilizationTxHash", "");
    }

    CAmount nDeniabilizationBudget = settings.value("nDeniabilizationBudget").toLongLong();
    if (nDeniabilizationBudget < 0) {
        nDeniabilizationBudget = 0;
    } else if (nDeniabilizationBudget > MAX_DENIABILIZATION_BUDGET) {
        nDeniabilizationBudget = MAX_DENIABILIZATION_BUDGET;
    }
    m_deniabilizationBudget = nDeniabilizationBudget;
    m_ui->budgetSpinner->setValue(m_deniabilizationBudget);

    uint64_t nDeniabilizationFrequency = settings.value("nDeniabilizationFrequency").toULongLong();
    m_deniabilizationFrequency = std::chrono::seconds(nDeniabilizationFrequency);

    if (m_deniabilizationFrequency == std::chrono::hours(1)) {
        m_ui->hourlyRadioButton->setChecked(true);
    } else if (m_deniabilizationFrequency == std::chrono::hours(24)) {
        m_ui->dailyRadioButton->setChecked(true);
    } else if (m_deniabilizationFrequency == std::chrono::hours(24 * 7)) {
        m_ui->weeklyRadioButton->setChecked(true);
    } else {
        m_deniabilizationFrequency = std::chrono::hours(24);
        m_ui->dailyRadioButton->setChecked(true);
    }

    m_nextDeniabilizationCycle.reset();
    uint64_t nNextDeniabilizationCycle = settings.value("nNextDeniabilizationCycle").toULongLong();
    if (nNextDeniabilizationCycle) {
        m_nextDeniabilizationCycle = std::chrono::system_clock::time_point(std::chrono::system_clock::duration(nNextDeniabilizationCycle));
    }

    m_deniabilizationProcessAccepted = settings.value("fDeniabilizationProcessAccepted").toBool();

    m_lastDeniabilizationTxHash.reset();
    QString hashStr = settings.value("sLastDeniabilizationTxHash").toString();
    if (!hashStr.isEmpty()) {
        std::optional<uint256> optHash = uint256::FromHex(hashStr.toStdString());
        if (optHash) {
            uint256 hash = optHash.value();
            if (wallet.getTx(hash)) {
                m_lastDeniabilizationTxHash = hash;
            }
        }
    }

    m_coinStateMap.clear();
    int coinCount = settings.beginReadArray("coinCheckStateArray");
    for (int coinIndex = 0; coinIndex < coinCount; coinIndex++) {
        settings.setArrayIndex(coinIndex);
        QString hashStr = settings.value("hash").toString();
        std::optional<uint256> optHash = uint256::FromHex(hashStr.toStdString());
        if (!optHash)
            continue;
        uint256 hash = optHash.value();
        CoinState coinState;
        coinState.deniabilizable = (Deniabilizable)settings.value("deniabilizable").toUInt();
        bool validStatus = false;
        switch (coinState.deniabilizable) {
        case Deniabilizable::YES:
        case Deniabilizable::YES_BUT_BLOCK_REWARD:
        case Deniabilizable::YES_BUT_COIN_LOCKED:
        case Deniabilizable::YES_BUT_TX_NOT_MATURE:
        case Deniabilizable::YES_BUT_AMOUNT_NOT_WORTHWHILE:
        case Deniabilizable::NO_FULLY_DENIABILIZED:
        case Deniabilizable::NO_PRIVATE_KEYS_DISABLED:
        case Deniabilizable::NO_AMOUNT_TOO_SMALL:
        case Deniabilizable::NO:
            validStatus = true;
            break;
        }
        if (!validStatus)
            continue;
        coinState.checkState = (Qt::CheckState)settings.value("checkState").toUInt();
        if (!(coinState.checkState == Qt::Checked || coinState.checkState == Qt::Unchecked))
            continue;
        m_coinStateMap[hash] = coinState;
    }
    settings.endArray();

    settings.endGroup();
}

void DeniabilityDialog::saveSettings()
{
    if (m_walletName.empty()) {
        return;
    }

    QSettings settings;
    settings.beginGroup("Deniability[" + QString::fromStdString(m_walletName) + "]");

    settings.setValue("nDeniabilizationBudget", (qint64)m_deniabilizationBudget);

    uint64_t nDeniabilizationFrequency = m_deniabilizationFrequency.count();
    settings.setValue("nDeniabilizationFrequency", (quint64)nDeniabilizationFrequency);

    uint64_t nNextDeniabilizationCycle = 0;
    if (m_nextDeniabilizationCycle.has_value()) {
        nNextDeniabilizationCycle = m_nextDeniabilizationCycle.value().time_since_epoch().count();
    }
    settings.setValue("nNextDeniabilizationCycle", (quint64)nNextDeniabilizationCycle);

    settings.setValue("fDeniabilizationProcessAccepted", m_deniabilizationProcessAccepted);

    if (m_lastDeniabilizationTxHash) {
        uint256 hash = m_lastDeniabilizationTxHash.value();
        QString hashStr = QString::fromStdString(hash.ToString());
        settings.setValue("sLastDeniabilizationTxHash", hashStr);
    } else {
        settings.setValue("sLastDeniabilizationTxHash", "");
    }

    settings.beginWriteArray("coinCheckStateArray");
    int coinIndex = 0;
    for (const auto& coin : m_coinsList) {
        // only store the the state of coins that may be deniabilized
        if (coin.state.mayBeDeniabilized()) {
            settings.setArrayIndex(coinIndex++);
            uint256 hash = coin.hash();
            QString hashStr = QString::fromStdString(hash.GetHex());
            settings.setValue("hash", hashStr);
            settings.setValue("deniabilizable", (uint)coin.state.deniabilizable);
            settings.setValue("checkState", (uint)coin.state.checkState);
        }
    }
    settings.endArray();

    settings.endGroup();
}

void DeniabilityDialog::updateCheckState(QTableWidgetItem* itemCheck)
{
    std::string destinationStr = itemCheck->data(destinationRole).toString().toStdString();
    CTxDestination destination = DecodeDestination(destinationStr);
    Qt::CheckState checkState = itemCheck->checkState();

    for (auto& coin : m_coinsList) {
        if (coin.destination() == destination) {
            coin.state.checkState = checkState;
            break;
        }
    }

    updateStart();
    updateStatus();
}

bool DeniabilityDialog::walletSupportsDeniabilization() const
{
    if (!m_model)
        return false;

    Wallet& wallet = m_model->wallet();
    if (wallet.privateKeysDisabled() && !hasExternalSigner(wallet)) {
        return false;
    }

    if (wallet.isLegacy()) {
        return false;
    }

    return true;
}


void DeniabilityDialog::updateStart()
{
    if (m_ui->stopButton->isEnabled()) {
        // stop button is active that means start button should not be
        Assert(!m_ui->startButton->isEnabled());
        return;
    }

    CAmount budgetValue = m_ui->budgetSpinner->value();
    bool hasCandidates = hasDeniabilizationCandidates();

    // disable or enable the start button depending on the budget provided and the availability of candidates
    if (m_ui->startButton->isEnabled()) {
        if (budgetValue == 0 || !hasCandidates) {
            m_ui->startButton->setEnabled(false);
        }
    } else {
        if (budgetValue > 0 && hasCandidates) {
            m_ui->startButton->setEnabled(true);
        }
    }
}

void DeniabilityDialog::updateStatus()
{
    if (!m_model) {
        m_ui->statusLabel->setText(tr("Deniabilization is not supported without a wallet"));
        return;
    }

    if (!m_clientModel || m_clientModel->node().isInitialBlockDownload()) {
        m_ui->statusLabel->setText(tr("Waiting for blockchain data to synchronize..."));
        return;
    }

    if (!walletSupportsDeniabilization()) {
        Wallet& wallet = m_model->wallet();
        if (wallet.privateKeysDisabled() && !hasExternalSigner(wallet)) {
            m_ui->statusLabel->setText(tr("Deniabilization is not supported without private keys"));
            return;
        }

        if (wallet.isLegacy()) {
            m_ui->statusLabel->setText(tr("Deniabilization is not supported on legacy wallets"));
            return;
        }
    }

    if (m_ui->startButton->isEnabled()) {
        Assert(!m_ui->stopButton->isEnabled());
        // start button is enabled which means a non-zero budget was entered
        if (hasDeniabilizationCandidates()) {
            m_ui->statusLabel->setText(tr("Deniabilization process is not active. Choose a frequency and press Start to begin."));
        } else {
            m_ui->statusLabel->setText(tr("No deniabilization candidates available."));
        }
        return;
    }

    if (m_ui->stopButton->isEnabled()) {
        if (m_deniabilizationTxInProgress) {
            m_ui->statusLabel->setText(tr("Deniabilization cycle in progress..."));
            return;
        }

        if (m_lastDeniabilizationTxHash) {
            m_ui->statusLabel->setText(tr("Waiting for the deniabilization transaction to be confirmed..."));
            return;
        }

        Assert(m_nextDeniabilizationCycle.has_value());
        auto timeNow = std::chrono::system_clock::now();
        if (timeNow < m_nextDeniabilizationCycle.value()) {
            auto deltaMinutes = std::chrono::duration_cast<std::chrono::minutes>(m_nextDeniabilizationCycle.value() - timeNow);
            QString deltaTimeStr;
            if (deltaMinutes < std::chrono::minutes(60)) {
                deltaTimeStr = QString::number(deltaMinutes.count()) + " " + tr("minutes");
            } else {
                auto deltaHours = std::chrono::duration_cast<std::chrono::hours>(deltaMinutes);
                deltaMinutes -= std::chrono::minutes(deltaHours);
                if (deltaHours < std::chrono::hours(24)) {
                    deltaTimeStr = QString::number(deltaHours.count()) + " " + tr("hours and") + " " + QString::number(deltaMinutes.count()) + " " + tr("minutes");
                } else {
                    auto deltaDays = deltaHours / 24;
                    deltaHours -= deltaDays * 24;
                    deltaTimeStr = QString::number(deltaDays.count()) + " " + tr("days") + ", " + QString::number(deltaHours.count()) + " " + tr("hours and") + " " + QString::number(deltaMinutes.count()) + " " + tr("minutes");
                }
            }
            m_ui->statusLabel->setText(tr("Next deniabilization cycle in") + " " + deltaTimeStr + ". " + tr("Press Stop to cancel."));
        } else {
            Wallet& wallet = m_model->wallet();
            if (hasExternalSigner(wallet)) {
                m_ui->statusLabel->setText(tr("Deniabilization process is active. Waiting on external signer to be connected..."));
            } else {
                m_ui->statusLabel->setText(tr("Deniabilization cycle is about to begin..."));
            }
        }
        return;
    }

    // both start and stop buttons are not active, so we're waiting for a budget to be entered
    if (hasDeniabilizationCandidates()) {
        m_ui->statusLabel->setText(tr("Deniabilization process is not active. Choose a frequency and a budget, and then press Start."));
    } else {
        m_ui->statusLabel->setText(tr("No deniabilization candidates available."));
    }
}

void DeniabilityDialog::updateCoinTable()
{
    m_ui->tableWidgetCoins->setUpdatesEnabled(false);

    m_ui->tableWidgetCoins->setRowCount(0);

    int nRow = 0;
    for (const auto& coin : m_coinsList) {
        m_ui->tableWidgetCoins->insertRow(nRow);

        QString destinationStr = QString::fromStdString(EncodeDestination(coin.destination()));

        static_assert(COLUMN_COUNT == 6, "Update the item logic below for any change in columns");

        {
            // Checkbox
            QTableWidgetItem* itemCheck = new QTableWidgetItem();
            itemCheck->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            itemCheck->setCheckState(Qt::Unchecked);
            itemCheck->setData(destinationRole, destinationStr);
            if (coin.state.mayBeDeniabilized()) {
                itemCheck->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            } else {
                itemCheck->setFlags(Qt::NoItemFlags);
            }
            itemCheck->setCheckState(coin.state.checkState);
            m_ui->tableWidgetCoins->setItem(nRow, COLUMN_CHECKBOX, itemCheck);
        }

        {
            // Destination
            QTableWidgetItem* itemDestination = new QTableWidgetItem();
            itemDestination->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            itemDestination->setText(destinationStr);
            // Keep the destination field always enabled so we can copy/paste the address
            itemDestination->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            if (!coin.state.mayBeDeniabilized()) {
                itemDestination->setForeground(Qt::gray);
            }
            switch (coin.state.deniabilizable) {
            case Deniabilizable::YES:
                itemDestination->setToolTip(tr("This coin can be deniabilized"));
                break;
            case Deniabilizable::YES_BUT_BLOCK_REWARD:
                itemDestination->setToolTip(tr("This coin can be deniabilized but it's from a block reward and likely not needed"));
                break;
            case Deniabilizable::YES_BUT_COIN_LOCKED:
                itemDestination->setToolTip(tr("This coin can be deniabilized but contains locked UTXOs, selecting it will unlock the coins during deniabilization"));
                break;
            case Deniabilizable::YES_BUT_TX_NOT_MATURE:
                itemDestination->setToolTip(tr("This coin can be deniabilized but waiting for more confirmations is recommended"));
                break;
            case Deniabilizable::YES_BUT_AMOUNT_NOT_WORTHWHILE:
                itemDestination->setToolTip(tr("This coin can be deniabilized but the amount is not worthwhile"));
                break;
            case Deniabilizable::NO_FULLY_DENIABILIZED:
                itemDestination->setToolTip(tr("This coin is already fully deniabilized"));
                break;
            case Deniabilizable::NO_PRIVATE_KEYS_DISABLED:
                itemDestination->setToolTip(tr("This coin can't be deniabilized because the wallet's private keys are disabled"));
                break;
            case Deniabilizable::NO_AMOUNT_TOO_SMALL:
                itemDestination->setToolTip(tr("This coin can't be deniabilized because the coin amount is less than the estimated fees"));
                break;
            case Deniabilizable::NO:
                itemDestination->setToolTip(tr("This coin can't be deniabilized"));
                break;
            }
            m_ui->tableWidgetCoins->setItem(nRow, COLUMN_DESTINATION, itemDestination);
        }

        {
            // UTXO Count
            QTableWidgetItem* itemUTXOCount = new QTableWidgetItem();
            itemUTXOCount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            itemUTXOCount->setText(QString::number(coin.numUTXOs()));
            if (coin.state.mayBeDeniabilized()) {
                itemUTXOCount->setFlags(Qt::ItemIsEnabled);
            } else {
                itemUTXOCount->setFlags(Qt::NoItemFlags);
                itemUTXOCount->setForeground(Qt::gray);
            }
            m_ui->tableWidgetCoins->setItem(nRow, COLUMN_UTXO_COUNT, itemUTXOCount);
        }

        {
            // Amount
            QTableWidgetItem* itemAmount = new QTableWidgetItem();
            itemAmount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            itemAmount->setText(BitcoinUnits::format(m_displayUnit, coin.value()));
            if (coin.state.mayBeDeniabilized()) {
                itemAmount->setFlags(Qt::ItemIsEnabled);
            } else {
                itemAmount->setFlags(Qt::NoItemFlags);
                itemAmount->setForeground(Qt::gray);
            }
            m_ui->tableWidgetCoins->setItem(nRow, COLUMN_AMOUNT, itemAmount);
        }

        {
            // Deniabilization status
            QTableWidgetItem* itemDeniabilization = new QTableWidgetItem();
            itemDeniabilization->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            itemDeniabilization->setText(QString::number(coin.deniabilizationCycles()));
            if (coin.state.mayBeDeniabilized()) {
                itemDeniabilization->setFlags(Qt::ItemIsEnabled);
            } else {
                itemDeniabilization->setFlags(Qt::NoItemFlags);
                itemDeniabilization->setForeground(Qt::gray);
            }
            // Set deniabilization cell highlight color
            if (coin.allUTXOsAreBlockReward()) {
                itemDeniabilization->setBackground(QColor(0, 128, 0, 128));
            } else {
                float deniabilizationProbability = wallet::CalculateDeniabilizationProbability(coin.deniabilizationCycles());
                itemDeniabilization->setBackground(QColor(deniabilizationProbability * 128, (1.0f - deniabilizationProbability) * 128, 0, 128));
            }
            m_ui->tableWidgetCoins->setItem(nRow, COLUMN_DENIABILIZATION_CYCLES, itemDeniabilization);
        }

        {
            // Estimated fee
            QTableWidgetItem* itemEstimatedFee = new QTableWidgetItem();
            itemEstimatedFee->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            itemEstimatedFee->setText(QString::number(coin.deniabilizationFeeEstimate));
            if (coin.state.mayBeDeniabilized()) {
                itemEstimatedFee->setFlags(Qt::ItemIsEnabled);
            } else {
                itemEstimatedFee->setFlags(Qt::NoItemFlags);
                itemEstimatedFee->setForeground(Qt::gray);
            }
            m_ui->tableWidgetCoins->setItem(nRow, COLUMN_ESTIMATED_FEE, itemEstimatedFee);
        }

        nRow++;
    }

    m_ui->tableWidgetCoins->setUpdatesEnabled(true);
}

void DeniabilityDialog::startDeniabilization()
{
    Assert(m_model);

    if (!m_deniabilizationProcessAccepted) {
        Wallet& wallet = m_model->wallet();

        QString message = tr("Deniabilization is about to start:");
        message += "<br><br>";
        message += tr("A coin will be selected from the top of the candidate list.");
        message += "<br>";
        message += tr("A transaction will be prepared to split the coin into a pair of new addresses in your wallet.");
        message += "<br>";
        message += tr("This makes blockchain analysis harder and thus improves privacy with each \"deniabilization\" cycle.");
        message += "<br>";
        if (hasExternalSigner(wallet)) {
            message += tr("You'll be prompted to confirm the transaction on your hardware device, and it will be broadcast immediately.");
        } else {
            message += tr("The transaction will be broadcast immediately.");
        }
        message += "<br><br>";
        message += tr("If %1 is left running continuously, the above process will repeat at the selected frequency (with some amount of randomization).").arg(PACKAGE_NAME);
        message += "<br><br>";
        message += tr("If %1 is shutdown and later restarted, the process will resume, and if the elapsed time has exceeded the selected frequency, it will prepare and broadcast a transaction immediately.").arg(PACKAGE_NAME);
        message += "<br><br>";
        message += tr("You can exclude a particular candidate from being selected by unchecking the checkbox on the left.");
        message += "<br><br>";
        message += tr("The deniabilization process will stop when the specified budget is exhausted or if it runs out of suitable candidates.");
        message += "<br>";
        message += tr("You can also stop it at any time by pressing the Stop button.");
        message += "<br><br>";
        message += tr("If you'd like to change the budget or frequency, press Cancel now.");
        message += "<br>";
        message += tr("Otherwise, press Ok to continue.");
        QMessageBox::StandardButton resultButton = QMessageBox::information(nullptr, tr("Starting deniabilization"), message, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (resultButton == QMessageBox::Cancel) {
            return;
        }
        m_deniabilizationProcessAccepted = true;
    }

    // disable the start button
    m_ui->startButton->setEnabled(false);
    //  disable all settings UI
    m_ui->hourlyRadioButton->setEnabled(false);
    m_ui->dailyRadioButton->setEnabled(false);
    m_ui->weeklyRadioButton->setEnabled(false);
    m_ui->budgetSpinner->setEnabled(false);
    // enable the stop button
    m_ui->stopButton->setEnabled(true);

    // if this is the first time we're running, schedule a deniabilization right away
    if (!m_nextDeniabilizationCycle.has_value()) {
        m_nextDeniabilizationCycle = std::chrono::system_clock::now();
    }
    updateStatus();
    // update status every 10 seconds
    m_deniabilizeProcTimer->start(std::chrono::seconds(10));
    deniabilizeProc();
}

void DeniabilityDialog::stopDeniabilization()
{
    Assert(m_model);

    m_deniabilizeProcTimer->stop();
    m_nextDeniabilizationCycle.reset();

    // disable the stop button
    m_ui->stopButton->setEnabled(false);
    //  enable back all settings UI
    m_ui->hourlyRadioButton->setEnabled(true);
    m_ui->dailyRadioButton->setEnabled(true);
    m_ui->weeklyRadioButton->setEnabled(true);
    m_ui->budgetSpinner->setEnabled(true);

    updateStart();
    updateStatus();
}

bool DeniabilityDialog::hasDeniabilizationCandidates() const
{
    // if the last tx hasn't confirmed yet,
    // consider it a candidate to prevent the deniabilization process from stopping prematurely
    if (m_lastDeniabilizationTxHash)
        return true;
    for (const auto& coin : m_coinsList) {
        // if a coin is not selected but may become deniabilizable,
        // consider it a candidate to prevent the deniabilization process from stopping prematurely
        if (coin.state.checkState == Qt::Checked || coin.state.mayBeDeniabilized()) {
            return true;
        }
    }
    return false;
}

enum TxStatus {
    TX_UNKNOWN,
    TX_CONFLICTING,
    TX_ABANDONED,
    TX_IN_MEMPOOL,
    TX_CONFIRMED
};

static TxStatus getTxStatus(interfaces::Wallet& wallet, uint256 hash)
{
    int numBlocks;
    interfaces::WalletTxStatus status;
    interfaces::WalletOrderForm orderForm;
    bool inMempool;
    interfaces::WalletTx wtx = wallet.getWalletTxDetails(hash, status, orderForm, inMempool, numBlocks);
    if (!wtx.tx) {
        // transaction not found
        return TX_UNKNOWN;
    } else if (status.depth_in_main_chain < 0) {
        // conflicting transaction
        return TX_CONFLICTING;
    } else if (status.depth_in_main_chain == 0) {
        if (status.is_abandoned || !inMempool) {
            // abandoned or dropped from the mempool
            return TX_ABANDONED;
        } else {
            // still in the mempool
            return TX_IN_MEMPOOL;
        }
    } else {
        Assert(status.depth_in_main_chain > 0);
        // the tx was included in a block
        return TX_CONFIRMED;
    }
}

void DeniabilityDialog::deniabilizeProc()
{
    if (!m_model)
        return;

    if (!m_clientModel || m_clientModel->node().isInitialBlockDownload()) {
        updateStatus();
        return;
    }

    Assert(m_nextDeniabilizationCycle.has_value());
    auto timeNow = std::chrono::system_clock::now();
    if (timeNow < m_nextDeniabilizationCycle.value()) {
        updateStatus();
        return;
    }

    if (m_deniabilizationTxInProgress) {
        updateStatus();
        return;
    }

    Wallet& wallet = m_model->wallet();
    if (hasExternalSigner(wallet)) {
        if (!externalSignerConnected()) {
            updateStatus();
            return;
        }
    }

    m_deniabilizationTxInProgress = true;

    updateCoins();

    // check up on the last TX and make sure it's not still in the mempool
    if (m_lastDeniabilizationTxHash) {
        TxStatus txStatus = getTxStatus(wallet, m_lastDeniabilizationTxHash.value());
        if (txStatus == TX_IN_MEMPOOL) {
            // if it's still in the mempool, try a fee bump
            QString hashStr = QString::fromStdString(m_lastDeniabilizationTxHash.value().ToString());
            if (bumpDeniabilizationTx(m_lastDeniabilizationTxHash.value())) {
                Assert(m_lastDeniabilizationTxHash.has_value());
                LogPrintf("Deniability[%s]: Fee bump transaction (%s) broadcasted successfully.\n", m_walletName, m_lastDeniabilizationTxHash.value().GetHex());
                // Update the table
                m_model->getTransactionTableModel()->updateTransaction(hashStr, CT_UPDATED, true);
            } else {
                LogPrintf("Deniability[%s]: Skipping fee bump transaction.\n", m_walletName);
            }
        }
    }

    // Check which coin can be deniabilized
    bool stop = false;
    if (!m_lastDeniabilizationTxHash) {
        for (auto& coin : m_coinsList) {
            if (coin.state.checkState == Qt::Unchecked) {
                continue;
            }

            DeniabilizationResult result = deniabilizeCoin(coin);
            if (result == DENIABILIZATION_SKIP_COIN) {
                // try the next coin now, but retry this coin at the next cycle
                LogPrintf("Deniability[%s]: Skipping coin.\n", m_walletName);
                continue;
            } else if (result == DENIABILIZATION_SKIP_ALL_COINS) {
                // don't try any other coins now, but retry the selected coins at the next cycle
                LogPrintf("Deniability[%s]: Skipping all coins.\n", m_walletName);
                break;
            } else if (result == DENIABILIZATION_DESELECT_COIN) {
                // deselect this coin so it won't retry at the next cycle (the user can still re-select manually)
                coin.state.checkState = Qt::Unchecked;
                LogPrintf("Deniability[%s]: Deselecting coin.\n", m_walletName);
                // try the next coin now
                continue;
            } else if (result == DENIABILIZATION_STOP) {
                // don't try any more coins and stop the processing, eg due to out of budget or other fatal error
                stop = true;
                LogPrintf("Deniability[%s]: Stopping the deniabilization process.\n", m_walletName);
                break;
            } else {
                Assert(result == DENIABILIZATION_SUCCESS);
                Assert(m_lastDeniabilizationTxHash.has_value());
                LogPrintf("Deniability[%s]: Transaction (%s) broadcasted successfully.\n", m_walletName, m_lastDeniabilizationTxHash.value().GetHex());
                updateCoins();
                break;
            }
        }
    }

    if (!stop && m_deniabilizationBudget > 0 && hasDeniabilizationCandidates()) {
        // Update the next deniabilization cycle time with the desired frequency with some randomization
        uint64_t frequency = m_deniabilizationFrequency.count();
        uint64_t randomizedFrequency = frequency + FastRandomContext().randrange(frequency);

        timeNow = std::chrono::system_clock::now();
        m_nextDeniabilizationCycle = timeNow + std::chrono::seconds(randomizedFrequency);
    } else {
        // if stop-processing was requested or there's no coins left to deniabilize, turn off the deniabilization process
        stopDeniabilization();
    }

    m_deniabilizationTxInProgress = false;
    updateStatus();
}

void DeniabilityDialog::clear()
{
    // if deniabilization is running don't do anything
    if (m_ui->stopButton->isEnabled()) {
        return;
    }

    // reset the UI to default values
    m_ui->budgetSpinner->setValue(0);
    m_ui->dailyRadioButton->setChecked(true);
    updateStart();
    updateStatus();
}

void DeniabilityDialog::reject()
{
    clear();
}

void DeniabilityDialog::accept()
{
    clear();
}

void DeniabilityDialog::updateCoinsIfVisible()
{
    if (this->isVisible()) {
        updateCoins();
    }
}

void DeniabilityDialog::updateNumberOfBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, SyncType synctype, SynchronizationState sync_state)
{
    if (sync_state == SynchronizationState::POST_INIT) {
        updateCoinsIfVisible();
    }
}

void DeniabilityDialog::setClientModel(ClientModel* clientModel)
{
    m_clientModel = clientModel;

    if (m_clientModel) {
        connect(m_clientModel, &ClientModel::numBlocksChanged, this, &DeniabilityDialog::updateNumberOfBlocks);
    }
}

void DeniabilityDialog::setModel(WalletModel* model)
{
    m_model = model;

    if (walletSupportsDeniabilization()) {
        Assert(m_model);
        OptionsModel* optionsModel = m_model->getOptionsModel();
        if (optionsModel) {
            connect(optionsModel, &OptionsModel::displayUnitChanged, this, [this, optionsModel]() {
                m_displayUnit = optionsModel->getDisplayUnit();
                setupTableWidget();
                updateCoinsIfVisible();
            });
        }

        connect(m_model, &WalletModel::balanceChanged, this, &DeniabilityDialog::updateCoinsIfVisible);

        connect(m_ui->startButton, &QPushButton::clicked, this, &DeniabilityDialog::startDeniabilization);
        connect(m_ui->stopButton, &QPushButton::clicked, this, &DeniabilityDialog::stopDeniabilization);

        connect(m_ui->budgetSpinner, &BitcoinAmountField::valueChanged, this, [this]() {
            m_deniabilizationBudget = m_ui->budgetSpinner->value();
            updateStart();
            updateStatus();
        });

        connect(m_ui->hourlyRadioButton, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                m_deniabilizationFrequency = std::chrono::hours(1);
                updateCoinsIfVisible();
            }
        });
        connect(m_ui->dailyRadioButton, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                m_deniabilizationFrequency = std::chrono::hours(24);
                updateCoinsIfVisible();
            }
        });
        connect(m_ui->weeklyRadioButton, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) {
                m_deniabilizationFrequency = std::chrono::hours(24 * 7);
                updateCoinsIfVisible();
            }
        });
    } else {
        // disable all settings UI
        m_ui->startButton->setEnabled(false);
        m_ui->hourlyRadioButton->setEnabled(false);
        m_ui->dailyRadioButton->setEnabled(false);
        m_ui->weeklyRadioButton->setEnabled(false);
        m_ui->budgetSpinner->setEnabled(false);
        m_ui->stopButton->setEnabled(false);
    }

    if (m_model) {
        m_walletName = m_model->wallet().getWalletName();
        loadSettings();
    } else {
        m_walletName.clear();
        m_deniabilizationBudget = 0;
        m_deniabilizationFrequency = std::chrono::seconds::zero();
        m_nextDeniabilizationCycle.reset();
    }

    updateCoins();

    // if the start button is enabled and we have a valid deniabilization time
    // that means deniabilization was active at shutdown, so restart it right away
    if (m_nextDeniabilizationCycle.has_value()) {
        if (m_ui->startButton->isEnabled()) {
            startDeniabilization();
        } else {
            m_nextDeniabilizationCycle.reset();
        }
    }
}

void DeniabilityDialog::updateCoins()
{
    if (!m_model) {
        m_coinsList.clear();
        updateCoinTable();
        updateStart();
        updateStatus();
        return;
    }

    // wait for at least 6 confirmations before deniabilizing a coin
    const int minChainDepth = 6;

    Wallet& wallet = m_model->wallet();

    // check up on the last TX and clear it if was confirmed, abandoned or dropped from the pool
    if (m_lastDeniabilizationTxHash) {
        TxStatus txStatus = getTxStatus(wallet, m_lastDeniabilizationTxHash.value());
        switch (txStatus) {
        case TX_IN_MEMPOOL:
            // still in mempool
            break;
        case TX_CONFIRMED:
            LogPrintf("Deniability[%s]: Deniabilization transaction (%s) was confirmed.\n", m_walletName, m_lastDeniabilizationTxHash.value().GetHex());
            m_lastDeniabilizationTxHash.reset();
            break;
        case TX_CONFLICTING:
        case TX_ABANDONED:
            LogPrintf("Deniability[%s]: Deniabilization transaction (%s) was abandoned or dropped.\n", m_walletName, m_lastDeniabilizationTxHash.value().GetHex());
            m_lastDeniabilizationTxHash.reset();
            break;
        case TX_UNKNOWN:
            LogPrintf("Deniability[%s]: Deniabilization transaction (%s) was not found.\n", m_walletName, m_lastDeniabilizationTxHash.value().GetHex());
            m_lastDeniabilizationTxHash.reset();
            break;
        }
    }

    Assert(m_deniabilizationFrequency.count() > 0);
    uint confirmTarget = m_deniabilizationFrequency.count() / (60 * 10); // 60 seconds per minute, 10 minutes per block
    CFeeRate deniabilizationFeeRate = wallet.getDeniabilizationFeeRate(confirmTarget);

    CFeeRate dustRelayFee = m_model->node().getDustRelayFee();

    // Before we reset the table, keep track of the coin state
    for (const auto& coin : m_coinsList) {
        // stash the coin state in the state map unless it's already there (eg from load settings)
        uint256 coinHash = coin.hash();
        auto mapIter = m_coinStateMap.find(coinHash);
        if (mapIter == m_coinStateMap.end()) {
            m_coinStateMap[coinHash] = coin.state;
        }
    }

    m_coinsList.clear();

    // group UTXOs that share the same scriptPubKey into a CoinInfo
    std::map<CScript, CoinInfo> coinInfoMap;
    {
        auto coinsListMap = wallet.listCoins();
        for (const auto& coinsPair : coinsListMap) {
            const auto& coinsTuples = coinsPair.second;
            for (const auto& coinTuple : coinsTuples) {
                CoinUTXO output;
                output.outpoint = std::get<0>(coinTuple);
                output.walletTxOut = std::get<1>(coinTuple);
                // skip spent outputs
                if (output.walletTxOut.is_spent)
                    continue;
                CScript scriptPubKey = output.walletTxOut.txout.scriptPubKey;
                if (scriptPubKey.IsUnspendable())
                    continue;
                auto result = wallet.calculateDeniabilizationCycles(output.outpoint);
                output.deniabilizationStats = DeniabilizationStats(result.first, result.second);
                coinInfoMap[scriptPubKey].utxos.push_back(std::move(output));
            }
        }
    }

    // fill in the rest of the CoinInfo data and store into m_coinsList
    m_coinsList.reserve(coinInfoMap.size());
    for (const auto& coinInfoPair : coinInfoMap) {
        CoinInfo coin = coinInfoPair.second;
        Assert(!coin.utxos.empty());

        // sort the outputs by outpoint so the order matches between runs
        auto utxoCompare = [](const CoinUTXO& utxoA, const CoinUTXO& utxoB) -> bool {
            return utxoA.outpoint < utxoB.outpoint;
        };

        std::sort(coin.utxos.begin(), coin.utxos.end(), utxoCompare);

        CAmount coinValue = coin.value();
        CScript coinScript = coin.scriptPubKey();
        CAmount dustThreshold = GetDustThreshold(CTxOut(coinValue, coinScript), dustRelayFee);
        uint deniabilizationCycles = coin.deniabilizationCycles();
        float deniabilizationProbability = wallet::CalculateDeniabilizationProbability(deniabilizationCycles);
        uint deniabilizationProbabilityPercent = deniabilizationProbability * 100;
        coin.deniabilizationFeeEstimate = wallet::CalculateDeniabilizationFeeEstimate(coinScript, coinValue, coin.numUTXOs(), deniabilizationCycles, deniabilizationFeeRate);

        coin.state.deniabilizable = Deniabilizable::YES;
        if (wallet.privateKeysDisabled() && !hasExternalSigner(wallet)) {
            // disable coins that don't have private keys (unless it's an external signer)
            coin.state.deniabilizable = Deniabilizable::NO_PRIVATE_KEYS_DISABLED;
        } else if (deniabilizationProbabilityPercent == 0) {
            // disable coins that are already fully deniabilized
            coin.state.deniabilizable = Deniabilizable::NO_FULLY_DENIABILIZED;
        } else if (coinValue < coin.deniabilizationFeeEstimate + dustThreshold) {
            // disable coins that are too small (eg after full deniabilization won't leave any more than dust)
            coin.state.deniabilizable = Deniabilizable::NO_AMOUNT_TOO_SMALL;
        } else if (coin.allUTXOsAreBlockReward()) {
            // deselect the coin if all UTXOs are from a block reward (thus probably not necessary to deniabilize)
            coin.state.deniabilizable = Deniabilizable::YES_BUT_BLOCK_REWARD;
        } else if (coin.depthInMainChain() < minChainDepth) {
            // deselect non-mature coins
            coin.state.deniabilizable = Deniabilizable::YES_BUT_TX_NOT_MATURE;
        } else if (coin.anyLockedCoin(wallet)) {
            // deselect locked coins
            coin.state.deniabilizable = Deniabilizable::YES_BUT_COIN_LOCKED;
        } else if (!wallet::IsDeniabilizationWorthwhile(coinValue, coin.deniabilizationFeeEstimate)) {
            // deselect coins that are too small to be worth obuscation (eg fees are more than 10% of the amount)
            coin.state.deniabilizable = Deniabilizable::YES_BUT_AMOUNT_NOT_WORTHWHILE;
        }

        if (coin.state.mayBeDeniabilized()) {
            const CoinState* coinState = nullptr;
            {
                uint256 coinHash = coin.hash();
                auto mapStateIter = m_coinStateMap.find(coinHash);
                if (mapStateIter != m_coinStateMap.end()) {
                    coinState = &mapStateIter->second;
                }
            }

            if (coinState && coinState->deniabilizable == coin.state.deniabilizable) {
                coin.state.checkState = coinState->checkState;
            } else {
                if (coin.state.canBeDeniabilized()) {
                    coin.state.checkState = Qt::Checked;
                } else {
                    coin.state.checkState = Qt::Unchecked;
                }
            }
        } else {
            coin.state.checkState = Qt::Unchecked;
        }

        m_coinsList.push_back(std::move(coin));
    }

    // all state is now transferred to the coin list so we can clear the state map
    m_coinStateMap.clear();

    auto coinCompare = [](const CoinInfo& coinA, const CoinInfo& coinB) -> bool {
        // coins that can be deniabilized go first
        if (coinA.state.canBeDeniabilized() != coinB.state.canBeDeniabilized())
            return coinA.state.canBeDeniabilized() > coinB.state.canBeDeniabilized();
        // coins that may be deniabilized go first
        if (coinA.state.mayBeDeniabilized() != coinB.state.mayBeDeniabilized())
            return coinA.state.mayBeDeniabilized() > coinB.state.mayBeDeniabilized();

        // calculate a compound "value and probability" and sort larger values first
        // this way bigger coins that are more likely to deniabilize will be tried first
        CAmount valueProbabilityA = coinA.value() * wallet::CalculateDeniabilizationProbability(coinA.deniabilizationCycles());
        CAmount valueProbabilityB = coinB.value() * wallet::CalculateDeniabilizationProbability(coinB.deniabilizationCycles());
        if (valueProbabilityA != valueProbabilityB)
            return valueProbabilityA > valueProbabilityB;

        // coins with more confirmations go first
        return coinA.depthInMainChain() > coinB.depthInMainChain();
    };

    std::sort(m_coinsList.begin(), m_coinsList.end(), coinCompare);

    updateCoinTable();
    updateStart();
    updateStatus();
}

bool DeniabilityDialog::signExternalSigner(interfaces::Wallet& wallet, CTransactionRef& tx, const QString& message)
{
    // the wallet must be unlocked before calling this function
    Assert(m_model && m_model->getEncryptionStatus() != WalletModel::Locked);

    QMessageBox::StandardButton resultButton = QMessageBox::question(nullptr, tr("Confirm on device"), message, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (resultButton == QMessageBox::Cancel) {
        // skip all coins to avoid spamming the user
        LogPrintf("Deniability[%s]: External signing cancelled.\n", m_walletName);
        return false;
    }
    Assert(resultButton == QMessageBox::Yes);

    CMutableTransaction mtx(*tx);
    PartiallySignedTransaction psbtx(mtx);
    bool complete = false;
    // Always fill without signing first. This prevents an external signer
    // from being called prematurely and is not expensive.
    std::optional<PSBTError> err = wallet.fillPSBT(SIGHASH_ALL, /*sign=*/false, /*bip32derivs=*/true, /*n_signed=*/nullptr, psbtx, complete);
    Assert(!complete);
    Assert(!err);

    try {
        err = m_model->wallet().fillPSBT(SIGHASH_ALL, /*sign=*/true, /*bip32derivs=*/true, /*n_signed=*/nullptr, psbtx, complete);
    } catch (const std::runtime_error& e) {
        LogPrintf("Deniability[%s]: External sign failed (%s).\n", m_walletName, e.what());
        QMessageBox::critical(nullptr, tr("Sign failed"), e.what());
        return false;
    }
    if (err == PSBTError::EXTERNAL_SIGNER_NOT_FOUND) {
        //: "External signer" means using devices such as hardware wallets.
        LogPrintf("Deniability[%s]: External signer not found.\n", m_walletName);
        QMessageBox::critical(nullptr, tr("External signer not found"), "External signer not found");
        return false;
    }
    if (err == PSBTError::EXTERNAL_SIGNER_FAILED) {
        //: "External signer" means using devices such as hardware wallets.
        LogPrintf("Deniability[%s]: External signer failure.\n", m_walletName);
        QMessageBox::critical(nullptr, tr("External signer failure"), "External signer failure");
        return false;
    }
    if (err) {
        LogPrintf("Deniability[%s]: PSBT failure. Failed to create transaction!\n", m_walletName);
        QMessageBox::critical(nullptr, tr("PSBT failure"), "Failed to create transaction!");
        return false;
    }
    // fillPSBT does not always properly finalize
    complete = FinalizeAndExtractPSBT(psbtx, mtx);
    if (!complete) {
        LogPrintf("Deniability[%s]: External signing failed.\n", m_walletName);
        return false;
    }
    // Prepare transaction for broadcast transaction if complete
    tx = MakeTransactionRef(mtx);
    return true;
}

void DeniabilityDialog::finalizeTxBroadcast(uint256 hash, CAmount txFee)
{
    // store the transaction hash so we can check up on it later
    m_lastDeniabilizationTxHash = hash;

    // update the deniabilization budget with the amount spent on tx fees
    Assert(m_deniabilizationBudget >= txFee);
    m_deniabilizationBudget -= txFee;
    // if the remaining budget is below a single tx fee, then zero it out so the deniabilization process stops
    if (m_deniabilizationBudget < txFee) {
        m_deniabilizationBudget = 0;
    }
    m_ui->budgetSpinner->setValue(m_deniabilizationBudget);
    Assert(m_deniabilizationBudget == m_ui->budgetSpinner->value());
}

DeniabilityDialog::DeniabilizationResult DeniabilityDialog::deniabilizeCoin(CoinInfo coin)
{
    Assert(walletSupportsDeniabilization());
    Assert(coin.state.mayBeDeniabilized());
    Assert(m_deniabilizationTxInProgress);

    // draw a random percent to decide if we should split this coin
    // randomizing the split decision makes the deniabilized transaction tree non-uniform and thus harder to identify
    uint deniabilizationCycles = coin.deniabilizationCycles();
    float deniabilizationProbability = wallet::CalculateDeniabilizationProbability(deniabilizationCycles);
    uint deniabilizationProbabilityPercent = deniabilizationProbability * 100;
    Assert(deniabilizationProbabilityPercent > 0);
    uint randomPercent = FastRandomContext().randrange(100u);
    LogPrintf("Deniability[%s]: Random probability (%u%%), coin probability (%u%%).\n", m_walletName, randomPercent, deniabilizationProbabilityPercent);
    if (randomPercent >= deniabilizationProbabilityPercent) {
        // skip this coin and retry next cycle
        return DENIABILIZATION_SKIP_COIN;
    }

    // we need to unlock the wallet to get new addresses and prepare/sign transactions
    WalletModel::UnlockContext ctx(m_model->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        LogPrintf("Deniability[%s]: Wallet unlock cancelled.\n", m_walletName);
        return DENIABILIZATION_SKIP_ALL_COINS;
    }

    Wallet& wallet = m_model->wallet();

    if (coin.anyLockedCoin(wallet)) {
        // locked coins are not automatically selected
        // so if we got here it means the user manually selected it
        // and we can go ahead an unlock it
        for (const auto& utxo : coin.utxos) {
            if (wallet.isLockedCoin(utxo.outpoint)) {
                if (!wallet.unlockCoin(utxo.outpoint)) {
                    // unlock failed so we'll skip the coin for this cycle
                    LogPrintf("Deniability[%s]: Coin unlock failed.\n", m_walletName);
                    return DENIABILIZATION_SKIP_COIN;
                }
            }
        }
    }

    Assert(m_deniabilizationFrequency.count() > 0);
    uint confirmTarget = m_deniabilizationFrequency.count() / (60 * 10); // 60 seconds per minute, 10 minutes per block

    std::set<COutPoint> inputs;
    for (const auto& utxo : coin.utxos) {
        inputs.insert(utxo.outpoint);
    }

    CTransactionRef newTx;
    CAmount txFee = 0;
    try {
        bool sign = !wallet.privateKeysDisabled();
        bool insufficientAmount = false;
        auto res = wallet.createDeniabilizationTransaction(inputs, OutputTypeFromDestination(coin.destination()), confirmTarget, deniabilizationCycles, sign, insufficientAmount, txFee);
        if (res) {
            newTx = *res;
        } else if (insufficientAmount) {
            // The amount is not enough for a split, so we disable this coin from further deniabilization
            LogPrintf("Deniability[%s]: %s\n", m_walletName, util::ErrorString(res).original);
            return DENIABILIZATION_DESELECT_COIN;
        } else {
            LogPrintf("Deniability[%s]: Creating the deniabilization transaction failed (%s).\n", m_walletName, util::ErrorString(res).original);
            Q_EMIT message(tr("Deniability"), tr("Creating the deniabilization transaction failed. ") + QString::fromStdString(util::ErrorString(res).translated), CClientUIInterface::MSG_ERROR);
            return DENIABILIZATION_STOP;
        }
    } catch (const std::runtime_error& err) {
        // Something unexpected happened, instruct user to report this bug.
        LogPrintf("Deniability[%s]: Creating the deniabilization transaction failed (%s).\n", m_walletName, err.what());
        Q_EMIT message(tr("Deniability"), tr("Creating the deniabilization transaction failed. ") + QString::fromStdString(err.what()), CClientUIInterface::MSG_ERROR);
        return DENIABILIZATION_STOP;
    }

    if (txFee > m_deniabilizationBudget) {
        LogPrintf("Deniability[%s]: Deniabilization budget (%d) exhausted (tx fee %d).\n", m_walletName, m_deniabilizationBudget, txFee);
        Q_EMIT message(tr("Deniability"), tr("Not enough budget left for a deniabilization transaction!"), CClientUIInterface::MSG_WARNING);
        return DENIABILIZATION_STOP;
    }

    if (hasExternalSigner(wallet)) {
        QString message = tr("Prepare to confirm the deniabilization transaction on your device.<br><br>Ready?");
        if (!signExternalSigner(wallet, newTx, message)) {
            // skip all coins to avoid spamming the user
            return DENIABILIZATION_SKIP_ALL_COINS;
        }
    }

    // Broadcast the transaction
    wallet.commitTransaction(newTx, /*value_map=*/{}, /*order_form=*/{});

    finalizeTxBroadcast(newTx->GetHash(), txFee);
    return DENIABILIZATION_SUCCESS;
}

bool DeniabilityDialog::bumpDeniabilizationTx(uint256 txid)
{
    Assert(m_model);
    WalletModel::UnlockContext ctx(m_model->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        LogPrintf("Deniability[%s]: Wallet unlock cancelled.\n", m_walletName);
        return false;
    }
    Wallet& wallet = m_model->wallet();

    Assert(m_deniabilizationFrequency.count() > 0);
    uint confirmTarget = m_deniabilizationFrequency.count() / (60 * 10); // 60 seconds per minute, 10 minutes per block

    CTransactionRef newTx;
    CAmount oldTxFee = 0;
    CAmount newTxFee = 0;
    try {
        bool sign = !wallet.privateKeysDisabled();
        auto res = wallet.createBumpDeniabilizationTransaction(txid, confirmTarget, sign, oldTxFee, newTxFee);
        if (res) {
            newTx = *res;
        } else {
            LogPrintf("Deniability[%s]: Creating the deniabilization bump transaction failed (%s).\n", m_walletName, util::ErrorString(res).original);
            Q_EMIT message(tr("Deniability"), tr("Creating the deniabilization bump transaction failed. ") + QString::fromStdString(util::ErrorString(res).translated), CClientUIInterface::MSG_ERROR);
            return false;
        }
    } catch (const std::runtime_error& err) {
        // Something unexpected happened, instruct user to report this bug.
        LogPrintf("Deniability[%s]: Creating the deniabilization bump transaction failed (%s).\n", m_walletName, err.what());
        Q_EMIT message(tr("Deniability"), tr("Creating the deniabilization bump transaction failed. ") + QString::fromStdString(err.what()), CClientUIInterface::MSG_ERROR);
        return false;
    }

    if (newTxFee <= oldTxFee) {
        // no point broadcasting a fee bump tx unless it's larger than the old fee
        LogPrintf("Deniability[%s]: New tx fee (%d) is not larger than the old tx fee (%d).\n", m_walletName, newTxFee, oldTxFee);
        return false;
    }

    CAmount txFee = newTxFee - oldTxFee;
    if (txFee > m_deniabilizationBudget) {
        Q_EMIT message(tr("Deniability"), tr("Not enough budget left for a fee bump!"), CClientUIInterface::MSG_WARNING);
        LogPrintf("Deniability[%s]: Not enough budget (%d) for a fee bump (%d).\n", m_walletName, m_deniabilizationBudget, txFee);
        return false;
    }

    if (hasExternalSigner(wallet)) {
        QString message = tr("Prepare to confirm the fee bump of the deniabilization transaction on your device.<br><br>Ready?");
        if (!signExternalSigner(wallet, newTx, message)) {
            return false;
        }
    }

    // commit the bumped transaction
    std::vector<bilingual_str> errors;
    uint256 new_hash;
    if (!wallet.commitBumpTransaction(txid, CMutableTransaction(*newTx), errors, new_hash)) {
        LogPrintf("Deniability[%s]: Failed to commit transaction (%s).\n", m_walletName, errors.front().original);
        QMessageBox::critical(nullptr, tr("Fee bump error"), tr("Failed to commit transaction") + "<br />(" + QString::fromStdString(errors.front().translated) + ")");
        return false;
    }

    finalizeTxBroadcast(new_hash, txFee);
    return true;
}
