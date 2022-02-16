// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRAFFICGRAPHWIDGET_H
#define BITCOIN_QT_TRAFFICGRAPHWIDGET_H

#include <QWidget>
#include <QQueue>

#include <chrono>

class ClientModel;

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QTimer;
QT_END_NAMESPACE

#define VALUES_SIZE 12

class TrafficGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrafficGraphWidget(QWidget *parent = nullptr);
    void setClientModel(ClientModel *model);
    bool GraphRangeBump() const;

protected:
    void paintEvent(QPaintEvent *) override;
    int y_value(float value);
    void mouseMoveEvent(QMouseEvent *event) override;
    int ttpoint = -1;
    int x_offset = 0;
    int y_offset = 0;
    int64_t tt_time = 0;
    void mousePressEvent(QMouseEvent *event) override;
    bool fToggle = true;

public Q_SLOTS:
    void updateStuff();
    std::chrono::minutes setGraphRange(unsigned int value);

private:
    void update_fMax();
    void paintPath(QPainterPath &path, QQueue<float> &samples);
    void updateRates(int value);

    QTimer *timer;
    float fMax{0};
    float new_fMax{0};
    float m_range{0};
    int m_value{0};
    int m_new_value{0};
    bool m_bump_value{false};
    QQueue<float> vSamplesIn[VALUES_SIZE];
    QQueue<float> vSamplesOut[VALUES_SIZE];
    QQueue<std::chrono::milliseconds> vTimeStamp[VALUES_SIZE];
    quint64 nLastBytesIn[VALUES_SIZE];
    quint64 nLastBytesOut[VALUES_SIZE];
    std::chrono::milliseconds nLastTime[VALUES_SIZE];
    unsigned int values[VALUES_SIZE] = {5, 10, 20, 45, 90, 3*60, 6*60, 12*60, 24*60, 3*24*60, 7*24*60, 28*24*60};
    //unsigned int values[VALUES_SIZE] = {5, 15, 60, 3*60, 12*60, 2*24*60, 7*24*60, 28*24*60};
    ClientModel *clientModel;
};

#endif // BITCOIN_QT_TRAFFICGRAPHWIDGET_H
