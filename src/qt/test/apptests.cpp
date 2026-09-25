// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <qt/test/apptests.h>

#include <chainparams.h>
#include <key.h>
#include <logging.h>
#include <qt/bitcoin.h>
#include <qt/bitcoingui.h>
#include <qt/networkstyle.h>
#include <qt/rpcconsole.h>
#include <test/util/setup_common.h>
#include <validation.h>

#include <QAction>
#include <QImageReader>
#include <QLineEdit>
#ifdef Q_OS_MACOS
#include <QOperatingSystemVersion>
#include <QSpinBox>
#ifdef ENABLE_WALLET
#include <QToolBar>
#endif
#endif
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTextEdit>
#include <QtGlobal>
#include <QtTest/QtTestWidgets>
#include <QtTest/QtTestGui>

namespace {
//! Regex find a string group inside of the console output
QString FindInConsole(const QString& output, const QString& pattern)
{
    const QRegularExpression re(pattern);
    return re.match(output).captured(1);
}

void CheckPngResource(const QString& path, const QSize& pixel_size, qreal device_pixel_ratio)
{
    QImageReader reader{path, QByteArrayLiteral("png")};
    reader.setAutoDetectImageFormat(false);
    QVERIFY(reader.canRead());
    const QImage image{reader.read()};
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), pixel_size);
    QCOMPARE(image.devicePixelRatio(), device_pixel_ratio);
    QVERIFY(image.hasAlphaChannel());
}

//! Call getblockchaininfo RPC and check first field of JSON output.
void TestRpcCommand(RPCConsole* console)
{
    QTextEdit* messagesWidget = console->findChild<QTextEdit*>("messagesWidget");
    QLineEdit* lineEdit = console->findChild<QLineEdit*>("lineEdit");

    CheckPngResource(QStringLiteral(":/icons/spin_up"), QSize(8, 5), 1.0);
    CheckPngResource(QStringLiteral(":/icons/spin_up@2x"), QSize(16, 10), 2.0);
    CheckPngResource(QStringLiteral(":/icons/spin_down"), QSize(8, 5), 1.0);
    CheckPngResource(QStringLiteral(":/icons/spin_down@2x"), QSize(16, 10), 2.0);
#ifdef Q_OS_MACOS
    const QOperatingSystemVersion macos_26{QOperatingSystemVersion::MacOS, 26};
    if (QOperatingSystemVersion::current() >= macos_26) {
        QVERIFY(!lineEdit->testAttribute(Qt::WA_MacShowFocusRect));
        QVERIFY(lineEdit->property("macos26RoundedTextField").toBool());

        QSpinBox spin_box;
        spin_box.ensurePolished();
        QVERIFY(!spin_box.testAttribute(Qt::WA_MacShowFocusRect));
        QVERIFY(spin_box.property("macos26RoundedTextField").toBool());
    }
#endif
    QSignalSpy mw_spy(messagesWidget, &QTextEdit::textChanged);
    QVERIFY(mw_spy.isValid());
    QTest::keyClicks(lineEdit, "getblockchaininfo");
    QTest::keyClick(lineEdit, Qt::Key_Return);
    QVERIFY(mw_spy.wait(1000));
    QCOMPARE(mw_spy.count(), 4);
    const QString output = messagesWidget->toPlainText();
    const QString pattern = QStringLiteral("\"chain\": \"(\\w+)\"");
    QCOMPARE(FindInConsole(output, pattern), QString("regtest"));
}
} // namespace

//! Entry point for BitcoinApplication tests.
void AppTests::appTests()
{
    qRegisterMetaType<interfaces::BlockAndHeaderTipInfo>("interfaces::BlockAndHeaderTipInfo");
    m_app.parameterSetup();
    QVERIFY(m_app.createOptionsModel(/*resetSettings=*/true));
    QScopedPointer<const NetworkStyle> style(NetworkStyle::instantiate(Params().GetChainType()));
    m_app.setupPlatformStyle();
    m_app.createWindow(style.data());
    connect(&m_app, &BitcoinApplication::windowShown, this, &AppTests::guiTests);
    expectCallback("guiTests");
    m_app.baseInitialize();
    m_app.requestInitialize();
    m_app.exec();
    m_app.requestShutdown();
    m_app.exec();

    // Reset global state to avoid interfering with later tests.
    LogInstance().DisconnectTestLogger();
}

//! Entry point for BitcoinGUI tests.
void AppTests::guiTests(BitcoinGUI* window)
{
    HandleCallback callback{"guiTests", *this};
#ifdef Q_OS_MACOS
    const QOperatingSystemVersion macos_26{QOperatingSystemVersion::MacOS, 26};
    const bool expects_unified_toolbar{QOperatingSystemVersion::current() >= macos_26};
    QCOMPARE(window->unifiedTitleAndToolBarOnMac(), expects_unified_toolbar);
#ifdef ENABLE_WALLET
    if (expects_unified_toolbar) {
        QToolBar* const toolbar{window->findChild<QToolBar*>()};
        QVERIFY(toolbar);
        QVERIFY(!toolbar->isFloatable());
        QCOMPARE(toolbar->allowedAreas(), Qt::TopToolBarArea);
    }
#endif
#endif
    connect(window, &BitcoinGUI::consoleShown, this, &AppTests::consoleTests);
    expectCallback("consoleTests");
    QAction* action = window->findChild<QAction*>("openRPCConsoleAction");
    action->activate(QAction::Trigger);
}

//! Entry point for RPCConsole tests.
void AppTests::consoleTests(RPCConsole* console)
{
    HandleCallback callback{"consoleTests", *this};
    TestRpcCommand(console);
}

//! Destructor to shut down after the last expected callback completes.
AppTests::HandleCallback::~HandleCallback()
{
    auto& callbacks = m_app_tests.m_callbacks;
    auto it = callbacks.find(m_callback);
    assert(it != callbacks.end());
    callbacks.erase(it);
    if (callbacks.empty()) {
        m_app_tests.m_app.exit(0);
    }
}
