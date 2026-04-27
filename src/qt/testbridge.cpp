// Copyright (c) 2026 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/testbridge.h>

#include <qt/bitcoingui.h>
#include <qt/bitcoinamountfield.h>
#include <qt/bitcoinunits.h>
#ifdef ENABLE_WALLET
#include <qt/walletframe.h>
#include <qt/walletview.h>
#endif // ENABLE_WALLET

#include <QAction>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScopedValueRollback>
#include <QScreen>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <optional>

namespace {
QString ObjectLabel(const QObject* object)
{
    if (!object) return {};
    return object->objectName().isEmpty() ? QString::fromLatin1(object->metaObject()->className())
                                          : object->objectName();
}

QString WindowLabel(const QWidget* window)
{
    return ObjectLabel(window);
}

bool WindowMatches(const QWidget* window, const QString& name)
{
    if (!window || name.isEmpty()) return false;
    return window->objectName() == name || QString::fromLatin1(window->metaObject()->className()) == name;
}

bool IsVisibleObject(const QObject* object)
{
    if (const auto* widget = qobject_cast<const QWidget*>(object)) return widget->isVisible();
    return true;
}

template <typename Fn>
bool InvokeMethodByName(QObject* object, const char* signature, Fn&& invoker)
{
    const int index = object->metaObject()->indexOfMethod(signature);
    if (index < 0) return false;
    return invoker(object->metaObject()->method(index));
}

QObject* FindObjectInSubtree(QObject* root, const QString& object_name)
{
    if (!root || object_name.isEmpty()) return nullptr;
    if (root->objectName() == object_name) return root;

    const QList<QObject*> matches = root->findChildren<QObject*>(object_name);
    if (matches.isEmpty()) return nullptr;

    for (QObject* match : matches) {
        if (IsVisibleObject(match)) return match;
    }
    return matches.first();
}

QLineEdit* SpinBoxLineEdit(QAbstractSpinBox* spin_box)
{
    return spin_box ? spin_box->findChild<QLineEdit*>() : nullptr;
}

bool WriteTextToObject(QObject* object, const QString& text)
{
    if (auto* amount_field = qobject_cast<BitcoinAmountField*>(object)) {
        CAmount amount{0};
        if (!BitcoinUnits::parse(BitcoinUnit::BTC, text, &amount)) return false;
        amount_field->setValue(amount);
        return true;
    }

    if (auto* line_edit = qobject_cast<QLineEdit*>(object)) {
        line_edit->setText(text);
        InvokeMethodByName(line_edit, "textEdited(QString)", [&](const QMetaMethod& method) {
            return method.invoke(line_edit, Qt::DirectConnection, Q_ARG(QString, text));
        });
        InvokeMethodByName(line_edit, "editingFinished()", [&](const QMetaMethod& method) {
            return method.invoke(line_edit, Qt::DirectConnection);
        });
        return true;
    }

    if (auto* combo_box = qobject_cast<QComboBox*>(object)) {
        if (combo_box->isEditable()) {
            combo_box->setEditText(text);
        } else {
            const int index = combo_box->findText(text);
            if (index >= 0) {
                combo_box->setCurrentIndex(index);
            } else {
                combo_box->setCurrentText(text);
            }
        }
        return true;
    }

    if (auto* text_edit = qobject_cast<QTextEdit*>(object)) {
        text_edit->setPlainText(text);
        return true;
    }

    if (auto* plain_text_edit = qobject_cast<QPlainTextEdit*>(object)) {
        plain_text_edit->setPlainText(text);
        return true;
    }

    if (auto* spin_box = qobject_cast<QAbstractSpinBox*>(object)) {
        QLineEdit* line_edit = SpinBoxLineEdit(spin_box);
        if (!line_edit) return false;
        line_edit->setText(text);
        spin_box->interpretText();
        InvokeMethodByName(spin_box, "editingFinished()", [&](const QMetaMethod& method) {
            return method.invoke(spin_box, Qt::DirectConnection);
        });
        return true;
    }

    if (auto* child_spin_box = object->findChild<QAbstractSpinBox*>()) {
        QLineEdit* line_edit = SpinBoxLineEdit(child_spin_box);
        if (!line_edit) return false;
        line_edit->setText(text);
        child_spin_box->interpretText();
        InvokeMethodByName(child_spin_box, "editingFinished()", [&](const QMetaMethod& method) {
            return method.invoke(child_spin_box, Qt::DirectConnection);
        });
        return true;
    }

    if (object->property("text").isValid()) {
        object->setProperty("text", text);
        InvokeMethodByName(object, "textEdited(QString)", [&](const QMetaMethod& method) {
            return method.invoke(object, Qt::DirectConnection, Q_ARG(QString, text));
        });
        InvokeMethodByName(object, "editingFinished()", [&](const QMetaMethod& method) {
            return method.invoke(object, Qt::DirectConnection);
        });
        return true;
    }

    return false;
}

std::optional<QString> ReadTextFromObject(QObject* object)
{
    if (!object) return std::nullopt;

    if (auto* line_edit = qobject_cast<QLineEdit*>(object)) return line_edit->text();
    if (auto* label = qobject_cast<QLabel*>(object)) return label->text();
    if (auto* button = qobject_cast<QAbstractButton*>(object)) return button->text();
    if (auto* combo_box = qobject_cast<QComboBox*>(object)) return combo_box->currentText();
    if (auto* text_edit = qobject_cast<QTextEdit*>(object)) return text_edit->toPlainText();
    if (auto* plain_text_edit = qobject_cast<QPlainTextEdit*>(object)) return plain_text_edit->toPlainText();
    if (auto* amount_field = qobject_cast<BitcoinAmountField*>(object)) {
        return BitcoinUnits::format(BitcoinUnit::BTC, amount_field->value(), false, BitcoinUnits::SeparatorStyle::NEVER);
    }
    if (auto* spin_box = qobject_cast<QAbstractSpinBox*>(object)) {
        if (QLineEdit* line_edit = SpinBoxLineEdit(spin_box)) return line_edit->text();
    }
    if (auto* action = qobject_cast<QAction*>(object)) return action->text();

    if (const QVariant text_value = object->property("text"); text_value.isValid()) {
        return text_value.toString();
    }

    if (auto* child_spin_box = object->findChild<QAbstractSpinBox*>()) {
        if (QLineEdit* line_edit = SpinBoxLineEdit(child_spin_box)) return line_edit->text();
    }

    return std::nullopt;
}
} // namespace

TestBridge::TestBridge(BitcoinGUI* gui, const QString& socket_path, QObject* parent)
    : QObject(parent), m_gui(gui), m_server(new QLocalServer(this))
{
    QLocalServer::removeServer(socket_path);

    if (!m_server->listen(socket_path)) {
        qWarning("TestBridge: failed to listen on %s: %s",
                 qPrintable(socket_path),
                 qPrintable(m_server->errorString()));
        return;
    }

    connect(m_server, &QLocalServer::newConnection, this, &TestBridge::handleNewConnection);
    qInfo("TestBridge: listening on %s", qPrintable(socket_path));
}

TestBridge::~TestBridge()
{
    for (auto* client : m_clients) {
        client->disconnectFromServer();
        client->deleteLater();
    }
    m_server->close();
}

void TestBridge::handleNewConnection()
{
    while (QLocalSocket* client = m_server->nextPendingConnection()) {
        m_clients.push_back(client);
        m_read_buffers.insert(client, QByteArray{});
        connect(client, &QLocalSocket::readyRead, this, &TestBridge::handleClientData);
        connect(client, &QLocalSocket::disconnected, this, &TestBridge::handleClientDisconnected);
        qInfo("TestBridge: client connected");
    }
}

void TestBridge::handleClientData()
{
    if (m_processing_client_data) {
        m_pending_client_data = true;
        return;
    }

    QScopedValueRollback<bool> processing_guard(m_processing_client_data, true);

    do {
        m_pending_client_data = false;
        std::vector<QPointer<QLocalSocket>> clients_snapshot;
        clients_snapshot.reserve(m_clients.size());
        for (QLocalSocket* client : m_clients) {
            clients_snapshot.emplace_back(client);
        }
        for (const QPointer<QLocalSocket>& client : clients_snapshot) {
            if (!client) continue;
            processClientCommands(client.data());
        }
    } while (m_pending_client_data);
}

void TestBridge::processClientCommands(QLocalSocket* client)
{
    if (!client) return;

    QByteArray& read_buffer = m_read_buffers[client];
    if (client->bytesAvailable() > 0) {
        read_buffer.append(client->readAll());
    }

    int newline_pos;
    while ((newline_pos = read_buffer.indexOf('\n')) != -1) {
        QByteArray line = read_buffer.left(newline_pos);
        read_buffer.remove(0, newline_pos + 1);

        if (line.trimmed().isEmpty()) continue;

        QByteArray response = processCommand(line);
        if (client->state() != QLocalSocket::ConnectedState) {
            return;
        }
        response.append('\n');
        client->write(response);
        client->flush();
    }
}

void TestBridge::handleClientDisconnected()
{
    auto* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;

    const auto it = std::find(m_clients.begin(), m_clients.end(), client);
    if (it != m_clients.end()) {
        m_clients.erase(it);
    }
    m_read_buffers.remove(client);
    client->deleteLater();
    qInfo("TestBridge: client disconnected");
}

QWidget* TestBridge::defaultWindow() const
{
    if (QWidget* modal = QApplication::activeModalWidget()) return modal;
    if (QWidget* active = QApplication::activeWindow()) return active;
    return m_gui;
}

std::vector<TestBridge::WindowEntry> TestBridge::listWindows() const
{
    std::vector<WindowEntry> windows;
    windows.reserve(QApplication::topLevelWidgets().size());

    QWidget* active_window = QApplication::activeWindow();
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (!widget || !widget->isWindow() || qobject_cast<QMenu*>(widget)) continue;

        windows.push_back(WindowEntry{
            .window_name = WindowLabel(widget),
            .class_name = QString::fromLatin1(widget->metaObject()->className()),
            .title = widget->windowTitle(),
            .active = widget == active_window,
            .visible = widget->isVisible(),
        });
    }

    return windows;
}

QWidget* TestBridge::findWindow(const QString& name) const
{
    if (name.isEmpty()) return defaultWindow();

    QWidget* default_window = defaultWindow();
    if (WindowMatches(default_window, name)) return default_window;

    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (!widget || !widget->isWindow() || qobject_cast<QMenu*>(widget)) continue;
        if (WindowMatches(widget, name)) return widget;
    }

    return nullptr;
}

QWidget* TestBridge::currentView(QWidget* window) const
{
    if (!window) return nullptr;

#ifdef ENABLE_WALLET
    if (auto* gui = qobject_cast<BitcoinGUI*>(window)) {
        if (auto* wallet_frame = qobject_cast<WalletFrame*>(gui->centralWidget())) {
            if (auto* wallet_view = wallet_frame->currentWalletView()) {
                if (QWidget* current = wallet_view->currentWidget()) return current;
                return wallet_view;
            }
            return wallet_frame;
        }
    }
#endif // ENABLE_WALLET

    return window;
}

QObject* TestBridge::findObject(const QString& object_name, const QString& window_name) const
{
    QWidget* window = findWindow(window_name);
    if (!window) return nullptr;

    if (QObject* match = FindObjectInSubtree(currentView(window), object_name)) {
        return match;
    }
    return FindObjectInSubtree(window, object_name);
}

void TestBridge::collectNamedObjects(QObject* root, std::vector<NamedObjectEntry>& results, QSet<const QObject*>& visited, int depth) const
{
    if (!root || visited.contains(root)) return;
    visited.insert(root);

    if (!root->objectName().isEmpty()) {
        results.push_back(NamedObjectEntry{
            .object_name = root->objectName(),
            .class_name = QString::fromLatin1(root->metaObject()->className()),
            .depth = depth,
            .visible = IsVisibleObject(root),
        });
    }

    for (QObject* child : root->children()) {
        collectNamedObjects(child, results, visited, depth + 1);
    }
}

QByteArray TestBridge::processCommand(const QByteArray& json_cmd)
{
    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(json_cmd, &parse_error);
    if (doc.isNull()) {
        return errorResponse(QStringLiteral("JSON parse error: %1").arg(parse_error.errorString()));
    }

    const QJsonObject obj = doc.object();
    const QString cmd = obj.value(QStringLiteral("cmd")).toString();
    const QString window_name = obj.value(QStringLiteral("window")).toString();

    if (cmd == QLatin1String("list_windows")) {
        return cmdListWindows();
    } else if (cmd == QLatin1String("get_active_window")) {
        return cmdGetActiveWindow();
    } else if (cmd == QLatin1String("get_current_view") || cmd == QLatin1String("get_current_page")) {
        return cmdGetCurrentView(window_name);
    } else if (cmd == QLatin1String("get_property")) {
        return cmdGetProperty(
            window_name,
            obj.value(QStringLiteral("objectName")).toString(),
            obj.value(QStringLiteral("prop")).toString());
    } else if (cmd == QLatin1String("click")) {
        return cmdClick(window_name, obj.value(QStringLiteral("objectName")).toString());
    } else if (cmd == QLatin1String("set_text")) {
        return cmdSetText(
            window_name,
            obj.value(QStringLiteral("objectName")).toString(),
            obj.value(QStringLiteral("text")).toString());
    } else if (cmd == QLatin1String("wait_for_window")) {
        return cmdWaitForWindow(
            obj.value(QStringLiteral("window")).toString(),
            obj.value(QStringLiteral("timeout")).toInt(5000));
    } else if (cmd == QLatin1String("wait_for_view") || cmd == QLatin1String("wait_for_page")) {
        return cmdWaitForView(
            window_name,
            obj.value(QStringLiteral("view")).toString(obj.value(QStringLiteral("page")).toString()),
            obj.value(QStringLiteral("timeout")).toInt(5000));
    } else if (cmd == QLatin1String("wait_for_property")) {
        return cmdWaitForProperty(
            window_name,
            obj.value(QStringLiteral("objectName")).toString(),
            obj.value(QStringLiteral("prop")).toString(),
            obj.value(QStringLiteral("timeout")).toInt(5000),
            obj.value(QStringLiteral("value")),
            obj.contains(QStringLiteral("value")),
            obj.value(QStringLiteral("contains")).toString(),
            obj.value(QStringLiteral("nonEmpty")).toBool(false));
    } else if (cmd == QLatin1String("get_text")) {
        return cmdGetText(window_name, obj.value(QStringLiteral("objectName")).toString());
    } else if (cmd == QLatin1String("save_screenshot")) {
        return cmdSaveScreenshot(window_name, obj.value(QStringLiteral("path")).toString());
    } else if (cmd == QLatin1String("list_objects")) {
        return cmdListObjects(window_name);
    }

    return errorResponse(QStringLiteral("Unknown command: %1").arg(cmd));
}

QByteArray TestBridge::cmdListWindows()
{
    const auto windows = listWindows();

    QJsonArray arr;
    for (const auto& entry : windows) {
        QJsonObject json_entry;
        json_entry[QStringLiteral("window")] = entry.window_name;
        json_entry[QStringLiteral("className")] = entry.class_name;
        json_entry[QStringLiteral("title")] = entry.title;
        json_entry[QStringLiteral("active")] = entry.active;
        json_entry[QStringLiteral("visible")] = entry.visible;
        arr.append(json_entry);
    }

    QJsonObject resp;
    resp[QStringLiteral("windows")] = arr;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdGetActiveWindow()
{
    QWidget* window = defaultWindow();
    if (!window) {
        return errorResponse(QStringLiteral("No active window"));
    }

    QJsonObject resp;
    resp[QStringLiteral("window")] = WindowLabel(window);
    resp[QStringLiteral("className")] = QString::fromLatin1(window->metaObject()->className());
    resp[QStringLiteral("title")] = window->windowTitle();
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdGetCurrentView(const QString& window_name)
{
    QWidget* window = findWindow(window_name);
    if (!window) {
        return errorResponse(QStringLiteral("Window not found: %1").arg(window_name));
    }

    QWidget* view = currentView(window);
    if (!view) {
        return errorResponse(QStringLiteral("Could not determine current view for window: %1").arg(WindowLabel(window)));
    }

    QJsonObject resp;
    resp[QStringLiteral("window")] = WindowLabel(window);
    resp[QStringLiteral("view")] = ObjectLabel(view);
    resp[QStringLiteral("className")] = QString::fromLatin1(view->metaObject()->className());
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdGetProperty(const QString& window_name, const QString& object_name, const QString& prop)
{
    if (object_name.isEmpty() || prop.isEmpty()) {
        return errorResponse(QStringLiteral("objectName and prop are required"));
    }

    QObject* object = findObject(object_name, window_name);
    if (!object) {
        return errorResponse(QStringLiteral("Object not found: %1").arg(object_name));
    }

    const QVariant value = object->property(prop.toLatin1().constData());
    if (!value.isValid()) {
        return errorResponse(QStringLiteral("Property not found: %1.%2").arg(object_name, prop));
    }

    QJsonObject resp;
    resp[QStringLiteral("value")] = QJsonValue::fromVariant(value);
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdClick(const QString& window_name, const QString& object_name)
{
    if (object_name.isEmpty()) {
        return errorResponse(QStringLiteral("objectName is required"));
    }

    QObject* object = findObject(object_name, window_name);
    if (!object) {
        return errorResponse(QStringLiteral("Object not found: %1").arg(object_name));
    }

    auto okResponse = []() {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    };

    auto queueInvoke = [](QObject* target, auto&& fn) {
        QTimer::singleShot(0, target, std::forward<decltype(fn)>(fn));
    };

    if (auto* action = qobject_cast<QAction*>(object)) {
        queueInvoke(action, [action] { action->trigger(); });
        return okResponse();
    }

    if (auto* button = qobject_cast<QAbstractButton*>(object)) {
        if (!button->isEnabled()) {
            return errorResponse(QStringLiteral("Object is disabled: %1").arg(object_name));
        }
        queueInvoke(button, [button] { button->click(); });
        return okResponse();
    }

    if (InvokeMethodByName(object, "click()", [&](const QMetaMethod& method) {
            queueInvoke(object, [object, method] { method.invoke(object, Qt::DirectConnection); });
            return true;
        })) {
        return okResponse();
    }

    if (InvokeMethodByName(object, "trigger()", [&](const QMetaMethod& method) {
            queueInvoke(object, [object, method] { method.invoke(object, Qt::DirectConnection); });
            return true;
        })) {
        return okResponse();
    }

    auto* widget = qobject_cast<QWidget*>(object);
    if (!widget) {
        return errorResponse(QStringLiteral("Cannot click object: %1").arg(object_name));
    }
    if (!widget->isVisible() || !widget->isEnabled()) {
        return errorResponse(QStringLiteral("Object is not clickable: %1").arg(object_name));
    }

    const QPoint local_pos = widget->rect().center();
    const QPoint global_pos = widget->mapToGlobal(local_pos);
    queueInvoke(widget, [widget, local_pos, global_pos] {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(local_pos), QPointF(global_pos),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(local_pos), QPointF(global_pos),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(widget, &press);
        QCoreApplication::sendEvent(widget, &release);
    });
    return okResponse();
}

QByteArray TestBridge::cmdSetText(const QString& window_name, const QString& object_name, const QString& text)
{
    if (object_name.isEmpty()) {
        return errorResponse(QStringLiteral("objectName is required"));
    }

    QObject* object = findObject(object_name, window_name);
    if (!object) {
        return errorResponse(QStringLiteral("Object not found: %1").arg(object_name));
    }

    if (!WriteTextToObject(object, text)) {
        return errorResponse(QStringLiteral("Object %1 does not support text entry").arg(object_name));
    }

    QJsonObject resp;
    resp[QStringLiteral("ok")] = true;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdWaitForWindow(const QString& window_name, int timeout_ms)
{
    if (window_name.isEmpty()) {
        return errorResponse(QStringLiteral("window is required"));
    }

    auto conditionMatched = [&]() {
        QWidget* window = findWindow(window_name);
        return window && window->isVisible();
    };

    if (conditionMatched()) {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    }

    QEventLoop wait_loop;
    QTimer timeout_timer;
    timeout_timer.setSingleShot(true);
    timeout_timer.setInterval(std::max(0, timeout_ms));

    QTimer poll_timer;
    poll_timer.setInterval(50);

    QObject::connect(&timeout_timer, &QTimer::timeout, &wait_loop, &QEventLoop::quit);
    QObject::connect(&poll_timer, &QTimer::timeout, &wait_loop, [&]() {
        if (conditionMatched()) wait_loop.quit();
    });

    timeout_timer.start();
    poll_timer.start();
    wait_loop.exec();

    if (conditionMatched()) {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    }

    return errorResponse(QStringLiteral("Timed out waiting for window: %1").arg(window_name));
}

QByteArray TestBridge::cmdWaitForView(const QString& window_name, const QString& view_name, int timeout_ms)
{
    if (view_name.isEmpty()) {
        return errorResponse(QStringLiteral("view is required"));
    }

    auto conditionMatched = [&]() {
        QWidget* view = currentView(findWindow(window_name));
        return view && ObjectLabel(view) == view_name && view->isVisible();
    };

    if (conditionMatched()) {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    }

    QEventLoop wait_loop;
    QTimer timeout_timer;
    timeout_timer.setSingleShot(true);
    timeout_timer.setInterval(std::max(0, timeout_ms));

    QTimer poll_timer;
    poll_timer.setInterval(50);

    QObject::connect(&timeout_timer, &QTimer::timeout, &wait_loop, &QEventLoop::quit);
    QObject::connect(&poll_timer, &QTimer::timeout, &wait_loop, [&]() {
        if (conditionMatched()) wait_loop.quit();
    });

    timeout_timer.start();
    poll_timer.start();
    wait_loop.exec();

    if (conditionMatched()) {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    }

    return errorResponse(QStringLiteral("Timed out waiting for view: %1").arg(view_name));
}

QByteArray TestBridge::cmdWaitForProperty(const QString& window_name, const QString& object_name, const QString& prop, int timeout_ms, const QJsonValue& expected, bool has_expected, const QString& contains, bool non_empty)
{
    if (object_name.isEmpty() || prop.isEmpty()) {
        return errorResponse(QStringLiteral("objectName and prop are required"));
    }

    auto conditionMatched = [&](QVariant* out_value) {
        QObject* object = findObject(object_name, window_name);
        if (!object) return false;

        const QVariant value = object->property(prop.toLatin1().constData());
        if (!value.isValid()) return false;

        bool matched = true;
        if (!contains.isEmpty()) {
            matched = value.toString().contains(contains);
        } else if (non_empty) {
            matched = !value.toString().trimmed().isEmpty();
        } else if (has_expected) {
            matched = (QJsonValue::fromVariant(value) == expected);
        }

        if (!matched) return false;
        if (out_value) *out_value = value;
        return true;
    };

    QVariant matched_value;
    if (conditionMatched(&matched_value)) {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        resp[QStringLiteral("value")] = QJsonValue::fromVariant(matched_value);
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    }

    QEventLoop wait_loop;
    QTimer timeout_timer;
    timeout_timer.setSingleShot(true);
    timeout_timer.setInterval(std::max(0, timeout_ms));

    QTimer poll_timer;
    poll_timer.setInterval(50);

    QObject::connect(&timeout_timer, &QTimer::timeout, &wait_loop, &QEventLoop::quit);
    QObject::connect(&poll_timer, &QTimer::timeout, &wait_loop, [&]() {
        if (conditionMatched(nullptr)) wait_loop.quit();
    });

    timeout_timer.start();
    poll_timer.start();
    wait_loop.exec();

    if (conditionMatched(&matched_value)) {
        QJsonObject resp;
        resp[QStringLiteral("ok")] = true;
        resp[QStringLiteral("value")] = QJsonValue::fromVariant(matched_value);
        return QJsonDocument(resp).toJson(QJsonDocument::Compact);
    }

    return errorResponse(QStringLiteral("Timed out waiting for property: %1.%2").arg(object_name, prop));
}

QByteArray TestBridge::cmdGetText(const QString& window_name, const QString& object_name)
{
    if (object_name.isEmpty()) {
        return errorResponse(QStringLiteral("objectName is required"));
    }

    QObject* object = findObject(object_name, window_name);
    if (!object) {
        return errorResponse(QStringLiteral("Object not found: %1").arg(object_name));
    }

    const auto text = ReadTextFromObject(object);
    if (!text.has_value()) {
        return errorResponse(QStringLiteral("Object %1 has no readable text").arg(object_name));
    }

    QJsonObject resp;
    resp[QStringLiteral("text")] = *text;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdSaveScreenshot(const QString& window_name, const QString& path)
{
    if (path.isEmpty()) {
        return errorResponse(QStringLiteral("path is required"));
    }

    QWidget* window = findWindow(window_name);
    if (!window) {
        return errorResponse(QStringLiteral("Window not found: %1").arg(window_name));
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const QImage image = window->grab().toImage();
    if (image.isNull()) {
        return errorResponse(QStringLiteral("Failed to capture screenshot"));
    }

    if (!image.save(path, "PNG")) {
        return errorResponse(QStringLiteral("Failed to save screenshot: %1").arg(path));
    }

    QJsonObject resp;
    resp[QStringLiteral("ok")] = true;
    resp[QStringLiteral("path")] = path;
    resp[QStringLiteral("width")] = image.width();
    resp[QStringLiteral("height")] = image.height();
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::cmdListObjects(const QString& window_name)
{
    QWidget* window = findWindow(window_name);
    if (!window) {
        return errorResponse(QStringLiteral("Window not found: %1").arg(window_name));
    }

    std::vector<NamedObjectEntry> objects;
    QSet<const QObject*> visited;
    collectNamedObjects(window, objects, visited, 0);

    QJsonArray arr;
    for (const auto& entry : objects) {
        QJsonObject json_entry;
        json_entry[QStringLiteral("objectName")] = entry.object_name;
        json_entry[QStringLiteral("className")] = entry.class_name;
        json_entry[QStringLiteral("depth")] = entry.depth;
        json_entry[QStringLiteral("visible")] = entry.visible;
        arr.append(json_entry);
    }

    QJsonObject resp;
    resp[QStringLiteral("window")] = WindowLabel(window);
    resp[QStringLiteral("objects")] = arr;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray TestBridge::errorResponse(const QString& message)
{
    QJsonObject resp;
    resp[QStringLiteral("error")] = message;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}
