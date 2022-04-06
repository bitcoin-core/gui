// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/notificator.h>

#include <QApplication>
#include <QByteArray>
#include <QImageWriter>
#include <QMessageBox>
#include <QMetaType>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QVariant>
#ifdef Q_OS_MAC
#include <qt/macnotificationhandler.h>
#endif


Notificator::Notificator(const QString &_programName, QSystemTrayIcon *_trayIcon, QWidget *_parent) :
    QObject(_parent),
    parent(_parent),
    programName(_programName),
    mode(None),
    trayIcon(_trayIcon)
{
    if(_trayIcon && _trayIcon->supportsMessages())
    {
        mode = QSystemTray;
    }
#ifdef Q_OS_MAC
    // check if users OS has support for NSUserNotification
    if( MacNotificationHandler::instance()->hasUserNotificationCenterSupport()) {
        mode = UserNotificationCenter;
    }
#endif
}

Notificator::~Notificator()
{
}

void Notificator::notifySystray(Class cls, const QString &title, const QString &text, int millisTimeout)
{
    QSystemTrayIcon::MessageIcon sicon = QSystemTrayIcon::NoIcon;
    switch(cls) // Set icon based on class
    {
    case Information: sicon = QSystemTrayIcon::Information; break;
    case Warning: sicon = QSystemTrayIcon::Warning; break;
    case Critical: sicon = QSystemTrayIcon::Critical; break;
    }
    trayIcon->showMessage(title, text, sicon, millisTimeout);
}

#ifdef Q_OS_MAC
void Notificator::notifyMacUserNotificationCenter(const QString &title, const QString &text)
{
    // icon is not supported by the user notification center yet. OSX will use the app icon.
    MacNotificationHandler::instance()->showNotification(title, text);
}
#endif

void Notificator::notify(Class cls, const QString &title, const QString &text, int millisTimeout)
{
    switch(mode)
    {
    case QSystemTray:
        notifySystray(cls, title, text, millisTimeout);
        break;
#ifdef Q_OS_MAC
    case UserNotificationCenter:
        notifyMacUserNotificationCenter(title, text);
        break;
#endif
    default:
        if(cls == Critical)
        {
            // Fall back to old fashioned pop-up dialog if critical and no other notification available
            QMessageBox::critical(parent, title, text, QMessageBox::Ok, QMessageBox::Ok);
        }
        break;
    }
}
