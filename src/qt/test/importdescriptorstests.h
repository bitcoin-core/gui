// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_IMPORTDESCRIPTORSTESTS_H
#define BITCOIN_QT_TEST_IMPORTDESCRIPTORSTESTS_H

#include <QObject>
#include <QTest>

namespace interfaces {
class Node;
} // namespace interfaces

class ImportDescriptorsTests : public QObject
{
public:
    explicit ImportDescriptorsTests(interfaces::Node& node) : m_node(node) {}
    interfaces::Node& m_node;

    Q_OBJECT

private Q_SLOTS:
    void importDescriptorsTests();
};

#endif // BITCOIN_QT_TEST_IMPORTDESCRIPTORSTESTS_H
