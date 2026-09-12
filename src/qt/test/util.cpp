// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/util.h>

#include <chrono>

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QWidget>

void ConfirmMessage(QString* text, std::chrono::milliseconds msec)
{
    QTimer::singleShot(msec, [text]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->inherits("QMessageBox") && widget->isVisible()) {
                QMessageBox* messageBox = qobject_cast<QMessageBox*>(widget);
                if (text) *text = messageBox->text();
                messageBox->defaultButton()->click();
                return;
            }
        }
        // Dialog not found yet. Retry with a short delay so we fire in the next
        // event loop rather than spinning in the current one.
        QTimer::singleShot(std::chrono::milliseconds{10}, [text]() { ConfirmMessage(text, std::chrono::milliseconds{0}); });
    });
}
