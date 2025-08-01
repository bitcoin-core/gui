// Copyright (c) 2024 - present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SNAPSHOTMODEL_H
#define BITCOIN_QT_SNAPSHOTMODEL_H

#include <QObject>
#include <interfaces/node.h>
#include <node/context.h>
#include <node/utxo_snapshot.h>

/** Model for snapshot operations. */
class SnapshotModel : public QObject
{
    Q_OBJECT

public:
    SnapshotModel(interfaces::Node& node, QString path);

    bool processPath();

private:
    interfaces::Node& m_node;
    QString m_path;
};

#endif // BITCOIN_QT_SNAPSHOTMODEL_H
