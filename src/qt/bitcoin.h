// Copyright (c) 2011-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOIN_H
#define BITCOIN_QT_BITCOIN_H

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <interfaces/node.h>
#include <qt/initexecutor.h>

#include <cassert>
#include <memory>
#include <optional>

#include <QApplication>

enum class SynchronizationState;

class BitcoinGUI;
class ClientModel;
class NetworkStyle;
class OptionsModel;
class PaymentServer;
class PlatformStyle;
class SplashScreen;
class WalletController;
class WalletModel;
namespace interfaces {
class Init;
} // namespace interfaces


/** Main Bitcoin application object */
class BitcoinApplication: public QApplication
{
    Q_OBJECT
public:
    explicit BitcoinApplication();
    ~BitcoinApplication();

#ifdef ENABLE_WALLET
    /// Create payment server
    void createPaymentServer();
#endif
    /// parameter interaction/setup based on rules
    void parameterSetup();
    /// Create options model
    [[nodiscard]] bool createOptionsModel(bool resetSettings);
    /// Initialize prune setting
    void InitPruneSetting(int64_t prune_MiB);
    /// Set snapshot path for loading after node initialization
    void setSnapshotPath(const QString& snapshot_path);
    /// Test method to manually trigger snapshot loading (for debugging)
    void testLoadSnapshot();
    /// Force load snapshot regardless of sync status (for debugging)
    void forceLoadSnapshot();
    /// Create main window
    void createWindow(const NetworkStyle *networkStyle);
    /// Create splash screen
    void createSplashScreen(const NetworkStyle *networkStyle);
    /// Create or spawn node
    void createNode(interfaces::Init& init);
    /// Basic initialization, before starting initialization/shutdown thread. Return true on success.
    bool baseInitialize();

    /// Request core initialization
    void requestInitialize();

    /// Get window identifier of QMainWindow (BitcoinGUI)
    WId getMainWinId() const;

    /// Setup platform style
    void setupPlatformStyle();

    void loadSnapshotIfNeeded();
    bool areHeadersSynced() const;
    void onHeaderTipChanged(SynchronizationState sync_state, interfaces::BlockTip tip, bool presync);

    interfaces::Node& node() const { assert(m_node); return *m_node; }

public Q_SLOTS:
    void initializeResult(bool success, interfaces::BlockAndHeaderTipInfo tip_info);
    /// Request core shutdown
    void requestShutdown();
    /// Handle runaway exceptions. Shows a message box with the problem and quits the program.
    void handleRunawayException(const QString &message);

    /**
     * A helper function that shows a message box
     * with details about a non-fatal exception.
     */
    void handleNonFatalException(const QString& message);

Q_SIGNALS:
    void requestedInitialize();
    void requestedShutdown();
    void windowShown(BitcoinGUI* window);

protected:
    bool event(QEvent* e) override;

private:
    std::optional<InitExecutor> m_executor;
    OptionsModel* optionsModel{nullptr};
    ClientModel* clientModel{nullptr};
    BitcoinGUI* window{nullptr};
    QTimer* pollShutdownTimer{nullptr};
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer{nullptr};
    WalletController* m_wallet_controller{nullptr};
#endif
    const PlatformStyle* platformStyle{nullptr};
    std::unique_ptr<QWidget> shutdownWindow;
    SplashScreen* m_splash = nullptr;
    std::unique_ptr<interfaces::Node> m_node;
    QString m_snapshot_path;
    std::unique_ptr<interfaces::Handler> m_header_tip_handler;
    QTimer* m_sync_check_timer;

    void startThread();
};

int GuiMain(int argc, char* argv[]);

#endif // BITCOIN_QT_BITCOIN_H
