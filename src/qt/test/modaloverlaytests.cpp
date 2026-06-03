// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/modaloverlaytests.h>

#include <qt/modaloverlay.h>

#include <QDateTime>
#include <QLabel>
#include <QTest>

void ModalOverlayTests::headersPresyncProgressStaysVisible()
{
    ModalOverlay overlay{/*enable_wallet=*/false, /*parent=*/nullptr};
    QLabel* blocks_left{overlay.findChild<QLabel*>("numberOfBlocksLeft")};
    QLabel* progress_label{overlay.findChild<QLabel*>("labelSyncDone")};
    QLabel* progress_value{overlay.findChild<QLabel*>("percentageProgress")};
    QVERIFY(blocks_left);
    QVERIFY(progress_label);
    QVERIFY(progress_value);

    const QDateTime presync_date{QDateTime::currentDateTime().addSecs(-3600)};
    overlay.setKnownBestHeight(/*count=*/1000, presync_date, /*presync=*/true);

    QVERIFY(blocks_left->text().contains("Pre-syncing Headers"));
    QVERIFY(blocks_left->text().contains("1000"));
    QCOMPARE(progress_label->text(), QString("Headers pre-sync progress"));
    QVERIFY(!progress_value->text().isEmpty());
    QVERIFY(progress_value->text() != QString("100.00%"));

    overlay.setKnownBestHeight(/*count=*/100, presync_date.addSecs(-1800), /*presync=*/false);
    QVERIFY(blocks_left->text().contains("Pre-syncing Headers"));
    QVERIFY(blocks_left->text().contains("1000"));
    QCOMPARE(progress_label->text(), QString("Headers pre-sync progress"));

    overlay.tipUpdate(/*count=*/0, presync_date.addSecs(-3600), /*nVerificationProgress=*/1.0);
    QVERIFY(blocks_left->text().contains("Pre-syncing Headers"));
    QCOMPARE(progress_label->text(), QString("Headers pre-sync progress"));
    QVERIFY(progress_value->text() != QString("100.00%"));

    overlay.setKnownBestHeight(/*count=*/1000, presync_date, /*presync=*/false);
    QVERIFY(blocks_left->text().contains("Syncing Headers"));
    QVERIFY(!blocks_left->text().contains("Pre-syncing Headers"));
    QCOMPARE(progress_label->text(), QString("Progress"));
}
