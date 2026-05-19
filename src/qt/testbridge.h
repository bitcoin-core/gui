// Copyright (c) 2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TESTBRIDGE_H
#define BITCOIN_QT_TESTBRIDGE_H

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QString>

#include <vector>

class BitcoinGUI;
class QWidget;

/// Exposes the Qt Widgets object tree to external test scripts over a local
/// socket. Enabled only when compiled with ENABLE_TEST_AUTOMATION and launched
/// with --test-automation=<socket_path>.
///
/// Supported commands (JSON over newline-delimited stream):
///   {"cmd": "list_windows"}
///   {"cmd": "get_active_window"}
///   {"cmd": "get_current_view", "window": "<windowName>"}
///   {"cmd": "get_property", "window": "<windowName>", "objectName": "<name>", "prop": "<property>"}
///   {"cmd": "click", "window": "<windowName>", "objectName": "<name>"}
///   {"cmd": "set_text", "window": "<windowName>", "objectName": "<name>", "text": "<value>"}
///   {"cmd": "wait_for_window", "window": "<windowName>", "timeout": <ms>}
///   {"cmd": "wait_for_view", "window": "<windowName>", "view": "<viewName>", "timeout": <ms>}
///   {"cmd": "wait_for_property", "window": "<windowName>", "objectName": "<name>", "prop": "<property>", ...}
///   {"cmd": "get_text", "window": "<windowName>", "objectName": "<name>"}
///   {"cmd": "save_screenshot", "window": "<windowName>", "path": "<png_path>"}
///   {"cmd": "list_objects", "window": "<windowName>"}
class TestBridge : public QObject
{
public:
    explicit TestBridge(BitcoinGUI* gui, const QString& socket_path, QObject* parent = nullptr);
    ~TestBridge() override;

private:
    void handleNewConnection();
    void handleClientData();
    void handleClientDisconnected();
    struct NamedObjectEntry {
        QString object_name;
        QString class_name;
        int depth;
        bool visible;
    };

    struct WindowEntry {
        QString window_name;
        QString class_name;
        QString title;
        bool active;
        bool visible;
    };

    QWidget* defaultWindow() const;
    QWidget* findWindow(const QString& name) const;
    QObject* findObject(const QString& object_name, const QString& window_name) const;
    QWidget* currentView(QWidget* window) const;
    void collectNamedObjects(QObject* root, std::vector<NamedObjectEntry>& results, QSet<const QObject*>& visited, int depth) const;
    std::vector<WindowEntry> listWindows() const;
    QByteArray processCommand(const QByteArray& json_cmd);
    void processClientCommands(QLocalSocket* client);

    QByteArray cmdListWindows();
    QByteArray cmdGetActiveWindow();
    QByteArray cmdGetCurrentView(const QString& window_name);
    QByteArray cmdGetProperty(const QString& window_name, const QString& object_name, const QString& prop);
    QByteArray cmdClick(const QString& window_name, const QString& object_name);
    QByteArray cmdSetText(const QString& window_name, const QString& object_name, const QString& text);
    QByteArray cmdWaitForWindow(const QString& window_name, int timeout_ms);
    QByteArray cmdWaitForView(const QString& window_name, const QString& view_name, int timeout_ms);
    QByteArray cmdWaitForProperty(const QString& window_name, const QString& object_name, const QString& prop, int timeout_ms, const QJsonValue& expected, bool has_expected, const QString& contains, bool non_empty);
    QByteArray cmdGetText(const QString& window_name, const QString& object_name);
    QByteArray cmdSaveScreenshot(const QString& window_name, const QString& path);
    QByteArray cmdListObjects(const QString& window_name);

    static QByteArray errorResponse(const QString& message);

    BitcoinGUI* m_gui;
    QLocalServer* m_server;
    std::vector<QLocalSocket*> m_clients;
    QHash<QLocalSocket*, QByteArray> m_read_buffers;
    bool m_processing_client_data{false};
    bool m_pending_client_data{false};
};

#endif // BITCOIN_QT_TESTBRIDGE_H
