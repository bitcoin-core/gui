// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_MODALOVERLAYTESTS_H
#define BITCOIN_QT_TEST_MODALOVERLAYTESTS_H

#include <QObject>

class ModalOverlayTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void headersPresyncProgressStaysVisible();
};

#endif // BITCOIN_QT_TEST_MODALOVERLAYTESTS_H
