// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRAFFICGRAPHWIDGET_H
#define BITCOIN_QT_TRAFFICGRAPHWIDGET_H

#include <QWidget>
#include <QQueue>
#include <QFile>
#include <QKeyEvent>

#include <chrono>

class ClientModel;

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QTimer;
QT_END_NAMESPACE

#define VALUES_SIZE 13

class TrafficGraphWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrafficGraphWidget(QWidget *parent = nullptr);
    void setClientModel(ClientModel *model);
    bool GraphRangeBump() const;
    void exportData();
    unsigned int getCurrentRangeIndex() const;

protected:
    void paintEvent(QPaintEvent *) override;
    int y_value(float value) const;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    int findClosestPoint(int x, int y, int rangeIndex) const;
    int findClosestPointByTimestamp(int sourceRange, int sourcePoint, int targetRange) const;

public Q_SLOTS:
    void updateStuff();
    std::chrono::minutes setGraphRange(unsigned int value);

private:
    void saveData();
    bool loadDataFromBinary();
    bool loadData();
    void update_fMax();
    void paintPath(QPainterPath &path, QQueue<float> &samples);
    void updateRates(int value);
    void focusSlider(Qt::FocusReason reason);
    void drawTooltipPoint(QPainter& painter);

    QTimer *m_timer{nullptr};
    float m_fmax{0};
    float m_new_fmax{0};
    float m_range{0};
    int m_value{0};
    int m_new_value{0};
    bool m_bump_value{false};
    bool m_toggle{true};
    int m_tt_point{-1};
    bool m_tt_in_series{true}; // true = in series, false = out series
    int m_x_offset{0};
    int m_y_offset{0};
    int64_t m_tt_time{0};
    QQueue<float> m_samples_in[VALUES_SIZE] = {};
    QQueue<float> m_samples_out[VALUES_SIZE] = {};
    QQueue<std::chrono::milliseconds> m_time_stamp[VALUES_SIZE] = {};
    quint64 m_last_bytes_in[VALUES_SIZE] = {};
    quint64 m_last_bytes_out[VALUES_SIZE] = {};
    std::chrono::milliseconds m_last_time[VALUES_SIZE] = {};
    unsigned int m_values[VALUES_SIZE] = {5, 10, 20, 45, 90, 3*60, 6*60, 12*60, 24*60, 3*24*60, 7*24*60, 14*24*60, 28*24*60};
    ClientModel *m_client_model{nullptr};
    uint64_t m_total_bytes_recv{0};
    uint64_t m_total_bytes_sent{0};
    uint64_t m_offset[VALUES_SIZE] = {};
};

#endif // BITCOIN_QT_TRAFFICGRAPHWIDGET_H
