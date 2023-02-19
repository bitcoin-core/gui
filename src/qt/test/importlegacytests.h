// Copyright (c) 2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_IMPORTLEGACYTESTS_H
#define BITCOIN_QT_TEST_IMPORTLEGACYTESTS_H

#include <QObject>
#include <QTest>

namespace interfaces {
class Node;
} // namespace interfaces

class ImportLegacyTests : public QObject
{
public:
    explicit ImportLegacyTests(interfaces::Node& node) : m_node(node) {}
    interfaces::Node& m_node;

    Q_OBJECT

private Q_SLOTS:
    void importLegacyTests();
};

#endif // BITCOIN_QT_TEST_IMPORTLEGACYTESTS_H
