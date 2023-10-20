// Copyright (c) 2022- The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DENIABILITYDIALOG_H
#define BITCOIN_QT_DENIABILITYDIALOG_H

#include <QDialog>

#include <interfaces/wallet.h>
#include <policy/feerate.h>
#include <qt/bitcoinunits.h>
#include <qt/walletmodel.h>
#include <uint256.h>
#include <util/chaintype.h>

#include <chrono>
#include <map>
#include <vector>

class ClientModel;
class WalletModel;
class PlatformStyle;
class COutPoint;
class WalletModelTransaction;
enum class SynchronizationState;
enum class SyncType;

QT_BEGIN_NAMESPACE
class QMenu;
class QPushButton;
class QTimer;
class QTableWidgetItem;
QT_END_NAMESPACE

namespace Ui {
class DeniabilityDialog;
}

class DeniabilityDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DeniabilityDialog(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    ~DeniabilityDialog();

    void setClientModel(ClientModel* model);
    void setModel(WalletModel* model);

    bool walletSupportsDeniabilization() const;
    bool hasDeniabilizationCandidates() const;
    void updateCoins();
    void updateCoinsIfVisible();

    enum {
        destinationRole = Qt::UserRole,
    };

private Q_SLOTS:
    void updateCheckState(QTableWidgetItem* itemCheck);
    void startDeniabilization();
    void stopDeniabilization();
    void deniabilizeProc();
    void contextualMenu(const QPoint& point);
    void updateNumberOfBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, SyncType synctype, SynchronizationState sync_state);
public Q_SLOTS:
    void reject() override;
    void accept() override;

private:
    Ui::DeniabilityDialog* m_ui;
    QMenu* m_contextMenu = nullptr;
    const PlatformStyle* m_platformStyle = nullptr;

    std::string m_walletName;
    ClientModel* m_clientModel = nullptr;
    WalletModel* m_model = nullptr;
    BitcoinUnit m_displayUnit = BitcoinUnit::BTC;

    enum class Deniabilizable : uint {
        YES,
        YES_BUT_BLOCK_REWARD,
        YES_BUT_COIN_LOCKED,
        YES_BUT_TX_NOT_MATURE,
        YES_BUT_AMOUNT_NOT_WORTHWHILE,
        NO_FULLY_DENIABILIZED,
        NO_PRIVATE_KEYS_DISABLED,
        NO_AMOUNT_TOO_SMALL,
        NO,
    };

    struct CoinState {
        Deniabilizable deniabilizable = Deniabilizable::NO;
        Qt::CheckState checkState = Qt::Unchecked;
        inline bool canBeDeniabilized() const
        {
            return deniabilizable == Deniabilizable::YES;
        }
        inline bool mayBeDeniabilized() const
        {
            switch (deniabilizable) {
            case Deniabilizable::YES:
            case Deniabilizable::YES_BUT_BLOCK_REWARD:
            case Deniabilizable::YES_BUT_COIN_LOCKED:
            case Deniabilizable::YES_BUT_TX_NOT_MATURE:
            case Deniabilizable::YES_BUT_AMOUNT_NOT_WORTHWHILE:
                return true;
            default:
                return false;
            }
        }
    };

    struct DeniabilizationStats {
        uint cycles = 0;
        bool blockReward = false;
        DeniabilizationStats() = default;
        explicit DeniabilizationStats(uint _cycles, bool _blockReward)
            : cycles(_cycles), blockReward(_blockReward)
        {
        }
    };

    struct CoinUTXO {
        COutPoint outpoint;
        interfaces::WalletTxOut walletTxOut;
        DeniabilizationStats deniabilizationStats;
    };

    struct CoinInfo {
        CoinState state;
        CAmount deniabilizationFeeEstimate = 0;
        std::vector<CoinUTXO> utxos;
        uint numUTXOs() const
        {
            return (uint)utxos.size();
        }
        CScript scriptPubKey() const;
        CTxDestination destination() const;
        uint256 hash() const;
        CAmount value() const;
        int depthInMainChain() const;
        uint deniabilizationCycles() const;
        bool allUTXOsAreBlockReward() const;
        bool anyLockedCoin(interfaces::Wallet& wallet) const;
    };
    std::vector<CoinInfo> m_coinsList;

    std::map<uint256, CoinState> m_coinStateMap;

    QTimer* m_deniabilizeProcTimer = nullptr;
    bool m_deniabilizationProcessAccepted = false;
    bool m_deniabilizationTxInProgress = false;
    std::optional<uint256> m_lastDeniabilizationTxHash;
    std::optional<std::chrono::system_clock::time_point> m_nextDeniabilizationCycle;
    std::chrono::seconds m_deniabilizationFrequency = std::chrono::seconds::zero();
    CAmount m_deniabilizationBudget = 0;

    void clear();
    void setupTableWidget();
    void loadSettings();
    void saveSettings();
    void updateStart();
    void updateStatus();
    void updateCoinTable();

    bool signExternalSigner(interfaces::Wallet& wallet, CTransactionRef& tx, const QString& message);
    void finalizeTxBroadcast(uint256 hash, CAmount txFee);

    enum DeniabilizationResult : uint {
        DENIABILIZATION_SUCCESS,
        DENIABILIZATION_SKIP_COIN,
        DENIABILIZATION_SKIP_ALL_COINS,
        DENIABILIZATION_DESELECT_COIN,
        DENIABILIZATION_STOP,
    };

    // deniabilize a given coin (passed by value to avoid crashes if m_coinsList gets updated while a tx is being built)
    DeniabilizationResult deniabilizeCoin(CoinInfo coin);

    // bump the fee of a deniabilization transaction
    bool bumpDeniabilizationTx(uint256 txid);

Q_SIGNALS:
    // Fired when a message should be reported to the user
    void message(const QString& title, const QString& message, unsigned int style);
};

#endif // BITCOIN_QT_DENIABILITYDIALOG_H
