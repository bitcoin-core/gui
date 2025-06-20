// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/node.h>
#include <qt/trafficgraphwidget.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>

#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QTimer>
#include <QToolTip>
#include <chrono>
#include <cmath>

#define DESIRED_SAMPLES         800

#define XMARGIN                 10
#define YMARGIN                 10

TrafficGraphWidget::TrafficGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TrafficGraphWidget::updateStuff);
    m_timer->setInterval(75);
    m_timer->start();
    setMouseTracking(true);
}

void TrafficGraphWidget::setClientModel(ClientModel *model)
{
    m_client_model = model;
    if(model) {
        m_data_dir = model->dataDir().toStdString();
        m_node = &model->node();  // Cache the node interface

        if (m_samples_in[0].empty() && m_samples_out[0].empty()) {
            loadData();
        }
    } else {
        // Save data when model is being disconnected during shutdown
        saveData();
    }
}

int TrafficGraphWidget::yValue(float value) const
{
    int h = height() - YMARGIN * 2;
    return YMARGIN + h - (h * 1.0 * (m_toggle ? (std::pow(value, 0.30102) / std::pow(m_fmax, 0.30102)) : (value / m_fmax)));
}

int TrafficGraphWidget::paintPath(QPainterPath& path, const QQueue<float>& samples)
{
    int sample_count = std::min(int(DESIRED_SAMPLES * m_range / m_values[m_value]), int(samples.size())) - 1;
    if (sample_count <= 0) return 0;
    int h = height() - YMARGIN * 2, w = width() - XMARGIN * 2;
    int x = XMARGIN + w, i;
    path.moveTo(x + 1, YMARGIN + h); // Overscan by 1 pixel to hide bright line
    for (i = 0; i <= sample_count; ++i) {
        if (i < 1) path.lineTo(x + 1, yValue(samples.at(0))); // Overscan by 1 pixel to the right
        double ratio = static_cast<double>(i) * m_values[m_value] / m_range / (DESIRED_SAMPLES - 1);
        x = XMARGIN + static_cast<int>(w - w * ratio + 0.5);
        if (i == sample_count && (sample_count < samples.size() - 1 || samples.size() >= DESIRED_SAMPLES)) {
            path.lineTo(x, yValue(samples.at(i)));
            x = XMARGIN - 1; // Overscan by one pixel to the left
        }
        path.lineTo(x, yValue(samples.at(i)));
    }
    path.lineTo(x, YMARGIN + h);

    return x;
}

void TrafficGraphWidget::focusSlider()
{
    QWidget* parent = parentWidget();
    if (parent) {
        QSlider* slider = parent->findChild<QSlider*>("sldGraphRange");
        if (slider) slider->setFocus(Qt::OtherFocusReason);
    }
}

void TrafficGraphWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    focusSlider();
    m_toggle = !m_toggle;
    m_update = true;
    update();
}

void TrafficGraphWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (!m_tt_point) return;
    m_tt_point = 0;
    m_update = true;
    update();
}

void TrafficGraphWidget::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
    static int last_x = -1, last_y = -1;
    QPointF pos = event->position();
    QPointF globalPos = event->globalPosition();
    int x = qRound(pos.x()), y = qRound(pos.y());
    m_x_offset = qRound(globalPos.x()) - x;
    m_y_offset = qRound(globalPos.y()) - y;
    if (last_x == x && last_y == y) return; // Do nothing if mouse hasn't moved
    int w = width() - XMARGIN * 2;
    int i = (w + XMARGIN - x) * (DESIRED_SAMPLES - 1) / w, closest_i = 0;
    int sampleSize = m_time_stamp[m_value].size();
    unsigned int smallest_distance = 50;
    bool is_in_series = true;
    for (int test_i = std::max(i - 3, 0); test_i < std::min(i + 9, sampleSize); test_i++) {
        float in_val = m_samples_in[m_value].at(test_i), out_val = m_samples_out[m_value].at(test_i);
        int y_in = yValue(in_val), y_out = yValue(out_val);
        unsigned int distance_in = abs(y - y_in), distance_out = abs(y - y_out);
        unsigned int min_distance = std::min(distance_in, distance_out) + abs(test_i - i);
        if (min_distance < smallest_distance) {
            smallest_distance = min_distance;
            closest_i = test_i + 1;
            is_in_series = (distance_in < distance_out);
        }
    }
    if (m_tt_point != closest_i || m_tt_in_series != is_in_series) {
        m_tt_point = closest_i;
        m_tt_in_series = is_in_series;
        m_update = true;
        update(); // Calls paintEvent() to draw or delete the highlighted point
    }
    last_x = x;
    last_y = y;
}

void TrafficGraphWidget::drawTooltipPoint(QPainter& painter)
{
    int w = width() - XMARGIN * 2;
    double ratio = static_cast<double>(m_tt_point-1) * m_values[m_value] / m_range / (DESIRED_SAMPLES-1);
    int x = XMARGIN + static_cast<int>(w - w * ratio + 0.5);
    float in_sample = m_samples_in[m_value].at(m_tt_point-1);
    float out_sample = m_samples_out[m_value].at(m_tt_point-1);
    float selected_sample = m_tt_in_series ? in_sample : out_sample;
    int y = yValue(selected_sample);
    painter.setPen(Qt::yellow);
    painter.drawEllipse(QPointF(x, y), 3, 3);
    QString str_tt;
    int64_t sample_time = 0;
    if (m_tt_point < m_time_stamp[m_value].size())
        sample_time = m_time_stamp[m_value].at(m_tt_point);
    if (!sample_time) // Either the oldest sample or the first ever sample
        sample_time = m_time_stamp[m_value].at(m_tt_point - 1);
    int age = TicksSinceEpoch<std::chrono::seconds>(SystemClock::now()) - sample_time / 1000;
    if (age < 60 * 60 * 23)
        str_tt += QString::fromStdString(FormatISO8601Time(sample_time / 1000));
    else
        str_tt += QString::fromStdString(FormatISO8601DateTime(sample_time / 1000));
    int duration = (m_time_stamp[m_value].at(m_tt_point - 1) - sample_time);
    if (duration > 0) {
        if (duration > 9999)
            str_tt += " +" + GUIUtil::formatDurationStr(std::chrono::seconds{(duration + 500) / 1000});
        else
            str_tt += " +" + GUIUtil::formatPingTime(std::chrono::microseconds{duration * 1000});
    }
    str_tt += "\n   " + tr("In") + " " + GUIUtil::formatBytesps(m_samples_in[m_value].at(m_tt_point-1) * 1000) +
                    "\n" + tr("Out") + " " + GUIUtil::formatBytesps(m_samples_out[m_value].at(m_tt_point-1) * 1000);

    // Line below allows ToolTip to move faster than the default ToolTip timeout (10 seconds).
    QToolTip::showText(QPoint(x + m_x_offset, y + m_y_offset), str_tt + ".");
    QToolTip::showText(QPoint(x + m_x_offset, y + m_y_offset), str_tt);
    m_tt_time = GetTime();
}

// Helper function to draw text with outline
void DrawOutlinedText(QPainter& painter, int y, const QString& text, int opacity)
{
    // Draw the outline by drawing the text multiple times with small offsets
    if (opacity) {
        painter.setPen(Qt::black);
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                if (dx != 0 || dy != 0)
                    painter.drawText(XMARGIN + dx, y + dy - 2, text);
    }

    // Draw the main text
    painter.setPen(Qt::white);
    painter.drawText(XMARGIN, y - 2, text);
}

void TrafficGraphWidget::paintEvent(QPaintEvent *)
{
    m_update = false;
    QPainter painter(this);
    int hgt = height(), wid = width();
    painter.fillRect(rect(), Qt::black);

    // decide what order of magnitude we are
    int base = std::floor(std::log10(m_fmax));
    float val = std::pow(10.0f, base); // kB/s

    // draw lines
    QColor axisCol(Qt::gray);
    painter.setPen(axisCol);
    for(float y = val; y < m_fmax; y += val) {
        int yy = yValue(y);
        painter.drawLine(XMARGIN, yy, wid - XMARGIN, yy);
    }

    // if we drew 10 (or 3 when toggles) or fewer lines, break them up at the next lower order of magnitude
    if (m_fmax / val <= (m_toggle ? 10.0f : 3.0f)) {
        val /= 10;
        painter.setPen(axisCol.darker());
        int count = 1;
        for (float y = val; y < (!m_toggle || m_fmax / val < 20 ? m_fmax : val*10); y += val, count++) {
            // don't overwrite lines drawn above
            if (count % 10 == 0) continue;
            int yy = yValue(y);
            painter.drawLine(XMARGIN, yy, wid - XMARGIN, yy);
        }
        if (m_toggle) {
            int yy = yValue(val * 0.1);
            painter.setPen(axisCol.darker().darker());
            painter.drawLine(XMARGIN, yy, wid - XMARGIN, yy);
        }
    }

    painter.setRenderHint(QPainter::Antialiasing);
    if (m_samples_in[m_value].size()) {
        QPainterPath p;
        paintPath(p, m_samples_in[m_value]);
        painter.fillPath(p, QColor(0, 255, 0, 128));
        painter.setPen(Qt::green);
        painter.drawPath(p);
    }
    int x = 0;
    if (m_samples_out[m_value].size()) {
        QPainterPath p;
        x = paintPath(p, m_samples_out[m_value]);
        painter.fillPath(p, QColor(255, 0, 0, 128));
        painter.setPen(Qt::red);
        painter.drawPath(p);
    }

    // Draw black bars and lines to mask the overscanned edges of the graph
    painter.fillRect(0, 0, XMARGIN - 1, hgt, Qt::black);
    painter.fillRect(wid - XMARGIN + 1, 0, XMARGIN, hgt, Qt::black);
    painter.setPen(Qt::black);
    painter.drawLine(XMARGIN - 1, 0, XMARGIN - 1, hgt); // Antialiased lines to create some blur
    painter.drawLine(wid - XMARGIN + 1, 0, wid - XMARGIN + 1, hgt);

    // Draw the bottom axis line after the graph
    painter.setPen(axisCol);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.drawLine(XMARGIN, hgt - YMARGIN, wid - XMARGIN, hgt - YMARGIN);

    int opacity = 0; // Opacity of the black outline around the text
    if (x < 70) opacity = 255;
    // Draw outlined text for speed labels
    DrawOutlinedText(painter, yValue(val*10), GUIUtil::formatBytesps(val * 10000), opacity);
    DrawOutlinedText(painter, yValue(val), GUIUtil::formatBytesps(val * 1000), opacity);
    if (m_toggle) DrawOutlinedText(painter, yValue(val/10), GUIUtil::formatBytesps(val * 100), opacity);

    if (m_tt_point && m_tt_point <= m_time_stamp[m_value].size()) drawTooltipPoint(painter);
    else QToolTip::hideText();
}

void TrafficGraphWidget::updateFmax()
{
    float tmax = 0.0f;
    for (const float f : m_samples_in[m_new_value])
        if (f > tmax) tmax = f;
    for (const float f : m_samples_out[m_new_value])
        if (f > tmax) tmax = f;
    m_new_fmax = std::max(tmax, 0.0001f);
}

/**
 * Smoothly updates a value with acceleration/deceleration for animation.
 *
 * @param target The target value to approach
 * @param current The current value that will be updated
 * @param increment The current rate of change (velocity), updated by this function
 * @param length The scale factor for controlling animation speed
 * @return true if the value was updated, false otherwise
 *
 * This implements a simple physics-based approach to animation:
 * - If moving too slowly, accelerate
 * - If moving too quickly, decelerate
 * - If close enough to target, snap to it
 */
bool UpdateNum(float target, float& current, float& increment, int length)
{
    if (current == target) return false;

    const float threshold = abs(0.8f * current) / length;
    const float diff = target - current;

    // Initialize or adjust increment based on current state
    if (abs(increment) <= threshold) { // allow equal to as current and increment could be zero
        increment = ((current + 1) * (diff > 0 ? 1.0f : -1.0f)) / length; // +1s are to get it started even if current is zero
        if (abs(increment) > abs(diff)) { // Only check this when creating an increment
            increment = 0; // We have arrived at the target
            current = target;
            return true;
        }
    } else {
        // Adjust increment based on distance to target
        if ((increment > 0 && current + increment * 2 > target) ||
                   (increment < 0 && current + increment * 2 < target)) {
            increment *= 0.5f;
        } else if ((increment > 0 && current + increment * 8 < target) ||
                   (increment < 0 && current + increment * 8 > target)) {
            increment *= 2.0f;
        }
    }

    // Update current value if increment is significant
    if (abs(increment) >= threshold) {
        current += increment;
    } else if ((increment >= 0 && target > current) || (increment <= 0 && target < current)) {
        current = target;
        increment = 0;
    }

    // Ensure minimum value for graph display
    if (current <= 0.0f) current = 0.0001f;

    return true;
}

void TrafficGraphWidget::updateStuff()
{
    if (!m_client_model) return;

    int64_t expected_gap = m_timer->interval();
    int64_t now = TicksSinceEpoch<std::chrono::milliseconds>(SystemClock::now());
    bool latest_bytes = false;
    quint64 bytes_in = 0, bytes_out = 0;

    // Check for new sample and update display if a new sample is taken for current range
    for (int i = 0; i < VALUES_SIZE; i++) {
        int64_t msecs_per_sample = static_cast<int64_t>(m_values[i]) * 60000 / DESIRED_SAMPLES;
        if (now > (m_last_time[i] + msecs_per_sample - expected_gap / 2)) {
            if (!latest_bytes) {
                latest_bytes = true;
                bytes_in = m_client_model->node().getTotalBytesRecv() + m_baseline_bytes_recv;
                bytes_out = m_client_model->node().getTotalBytesSent() + m_baseline_bytes_sent;
            }
            updateRates(i, now, bytes_in, bytes_out);
            if (i == m_value) {
                if (m_tt_point && m_tt_point <= DESIRED_SAMPLES) {
                    m_tt_point++; // Move the selected point to the left
                    if (m_tt_point > DESIRED_SAMPLES) m_tt_point = 0;
                }
                m_update = true;
            }
            if (i == m_new_value) updateFmax();
        }
    }

    // Update display due to transition between ranges or new fmax
    static float y_increment = 0, x_increment = 0;
    if (UpdateNum(m_new_fmax, m_fmax, y_increment, 300)) m_update = true;
    int next_m_value = m_value;
    if (UpdateNum(m_values[m_new_value], m_range, x_increment, 500)) {
        m_update = true;
        if (m_values[m_new_value] > m_range && m_values[m_value] < m_range) {
            next_m_value = m_value + 1;
        } else if (m_new_value < m_value && m_values[m_value - 1] > m_range * 0.99)
            next_m_value = m_value - 1;
    } else if (m_value != m_new_value) {
        m_update = true;
        next_m_value = m_new_value;
    }

    if (next_m_value != m_value) {
        m_tt_point = findClosestPointByTimestamp(next_m_value);
        m_value = next_m_value;
    }

    static bool last_m_toggle = m_toggle;
    if (!QToolTip::isVisible()) {
        if (m_tt_point) { // Remove the yellow circle if the ToolTip has gone due to mouse moving elsewhere.
            if (last_m_toggle == m_toggle) m_tt_point = 0;
            else last_m_toggle = m_toggle;
            m_update = true;
        }
    } else if (m_tt_point && GetTime() >= m_tt_time + 9) m_update = true;

    if (m_update) update();
    static bool graph_visible = false;
    if (isVisible() && !window()->isMinimized()) {
        if (!graph_visible) focusSlider();
        graph_visible = true;
    } else graph_visible = false;
}

void TrafficGraphWidget::updateRates(int i, int64_t now, quint64 bytes_in, quint64 bytes_out)
{
    int64_t actual_gap = now - m_last_time[i];
    float in_rate_kilobytes_per_msec = static_cast<float>(bytes_in - m_last_bytes_in[i]) / actual_gap;
    float out_rate_kilobytes_per_msec = static_cast<float>(bytes_out - m_last_bytes_out[i]) / actual_gap;
    m_samples_in[i].push_front(in_rate_kilobytes_per_msec);
    m_samples_out[i].push_front(out_rate_kilobytes_per_msec);
    m_time_stamp[i].push_front(now);
    m_last_bytes_in[i] = bytes_in;
    m_last_bytes_out[i] = bytes_out;
    m_last_time[i] = now;
    static int8_t full[VALUES_SIZE] = {};
    if (full[i] == 0 && m_time_stamp[i].size() <= DESIRED_SAMPLES) full[i] = -1;
    while (m_time_stamp[i].size() > DESIRED_SAMPLES) {
        if (m_value == i && i < VALUES_SIZE - 1 && full[i] < 0) m_bump = true;
        full[i] = 1;
        m_samples_in[i].pop_back();
        m_samples_out[i].pop_back();
        m_time_stamp[i].pop_back();
    }
}

int TrafficGraphWidget::setGraphRange(int value)
{
    // value is the array marker plus 1 (as zero is reserved for bumping up)
    if (!value) { // bump
        m_bump = false; // Clear the bump flag
        value = m_value + 1;
    } else
        value--; // get the array marker
    int old_value = m_new_value;
    m_new_value = std::min(value, VALUES_SIZE - 1);
    if (m_new_value != old_value) updateFmax();

    return m_values[m_new_value];
}

void TrafficGraphWidget::saveData()
{
    if (m_time_stamp[0].empty() || m_data_dir.empty()) return;
    try {
        fs::path pathTrafficGraph = fs::path(m_data_dir.c_str()) / "trafficgraph.dat";
        FILE* file = fsbridge::fopen(pathTrafficGraph, "wb");
        if (!file) {
            LogPrintf("TrafficGraphWidget: Failed to open file for writing: %s\n", pathTrafficGraph.generic_string());
            throw std::runtime_error("Failed to open file");
        }
        AutoFile fileout(file);
        if (fileout.IsNull()) throw std::runtime_error("File stream is null");
        fileout << static_cast<uint32_t>(1); // Version 1

        // Get current node values and add them to our baseline
        if (m_node) {
            m_baseline_bytes_recv += m_node->getTotalBytesRecv();
            m_baseline_bytes_sent += m_node->getTotalBytesSent();
        }

        fileout << VARINT(m_baseline_bytes_recv) << VARINT(m_baseline_bytes_sent);

        for (unsigned int i = 0; i < VALUES_SIZE; i++) {
            fileout << VARINT(m_last_bytes_in[i]) << VARINT(m_last_bytes_out[i]);

            fileout << VARINT(static_cast<uint16_t>(m_time_stamp[i].size()));

            for (int j = 0; j < m_time_stamp[i].size(); j++) {
                fileout << static_cast<uint64_t>(m_time_stamp[i].at(j));
            }

            for (int j = 0; j < m_samples_in[i].size(); j++) {
                float value = m_samples_in[i].at(j);
                uint32_t uint_value;
                memcpy(&uint_value, &value, sizeof(float)); // IEEE 754
                fileout << uint_value;
            }

            for (int j = 0; j < m_samples_out[i].size(); j++) {
                float value = m_samples_out[i].at(j);
                uint32_t uint_value;
                memcpy(&uint_value, &value, sizeof(float)); // IEEE 754
                fileout << uint_value;
            }
        }

        fileout.fclose();
        LogPrintf("TrafficGraphWidget: Successfully saved traffic graph data to %s\n", pathTrafficGraph.generic_string());
    } catch (const std::exception& e) {
        LogPrintf("TrafficGraphWidget: Error saving data: %s (path: %s)\n",
                 e.what(), m_data_dir);
    }
}

bool TrafficGraphWidget::loadDataFromBinary()
{
    try {
        fs::path pathTrafficGraph = fs::path(m_data_dir.c_str()) / "trafficgraph.dat";
        LogPrintf("TrafficGraphWidget: Attempting to load data from %s\n", pathTrafficGraph.generic_string());

        FILE* file = fsbridge::fopen(pathTrafficGraph, "rb");
        if (!file) {
            LogPrintf("TrafficGraphWidget: File not found or could not be opened\n");
            return false;
        }
        AutoFile filein(file);
        if (filein.IsNull()) return false;

        int version;
        filein >> version;
        if (version < 1 || version > 1) return false;

        filein >> VARINT(m_baseline_bytes_recv) >> VARINT(m_baseline_bytes_sent);

        uint64_t current_time = TicksSinceEpoch<std::chrono::milliseconds>(SystemClock::now());

        for (unsigned int i = 0; i < VALUES_SIZE; i++) {
            filein >> VARINT(m_last_bytes_in[i]) >> VARINT(m_last_bytes_out[i]);

            uint16_t samplesSize;
            filein >> VARINT(samplesSize);

            for (unsigned int j = 0; j < samplesSize; j++) {
                static uint64_t last_time_ms;
                uint64_t time_ms;
                filein >> time_ms;
                if (!j) m_last_time[i] = last_time_ms = time_ms;
                if (time_ms > last_time_ms || time_ms > current_time) return false; // Abort load if data invalid or in future
                m_time_stamp[i].push_back(static_cast<int64_t>(time_ms));
                last_time_ms = time_ms;
            }

            for (unsigned int j = 0; j < samplesSize; j++) {
                uint32_t uint_value;
                filein >> uint_value;
                float value;
                memcpy(&value, &uint_value, sizeof(float));
                m_samples_in[i].push_back(value);
            }

            for (unsigned int j = 0; j < samplesSize; j++) {
                uint32_t uint_value;
                filein >> uint_value;
                float value;
                memcpy(&value, &uint_value, sizeof(float));
                m_samples_out[i].push_back(value);
            }
        }
        filein.fclose();
        return true;

    } catch (const std::exception& e) {
        LogPrintf("TrafficGraphWidget: Error loading data: %s\n", e.what());
        return false;
    }
}

bool TrafficGraphWidget::loadData()
{
    bool success = loadDataFromBinary();

    if (!success) { // Zero the values
        LogPrintf("TrafficGraphWidget: Saved traffic data was invalid.\n");
        m_baseline_bytes_recv = m_baseline_bytes_sent = 0;
        for (int i = 0; i < VALUES_SIZE; i++) {
            m_last_bytes_in[i] = m_last_bytes_out[i] = m_last_time[i] = 0;
            m_samples_in[i].clear();
            m_samples_out[i].clear();
            m_time_stamp[i].clear();
        }
        return false;
    }

    // If we successfully loaded data, determine the correct band to use
    int firstNonFullBand = VALUES_SIZE - 1;

    for (int i = 0; i < VALUES_SIZE; i++) {
        if (m_time_stamp[i].size() < DESIRED_SAMPLES) {
            firstNonFullBand = i;
            break;
        }
    }

    if (firstNonFullBand) { // not the first band
        m_value = firstNonFullBand - 1; // Minus one as we're bumping it
        m_bump = true; // Set the slider to the new range
    }

    return true;
}

int TrafficGraphWidget::findClosestPointByTimestamp(int dst_range) const
{
    if (!m_tt_point || m_tt_point > m_time_stamp[m_value].size() ||
        m_time_stamp[dst_range].empty()) {
        return 0;
    }

    int src_point = m_tt_point - 1;
    bool is_peak = false, is_dip = false;
    float src_value = m_tt_in_series ? m_samples_in[m_value].at(src_point) :
                m_samples_out[m_value].at(src_point);
    int64_t src_timestamp = m_time_stamp[m_value].at(src_point);

    if (src_point > 0 && src_point < m_time_stamp[m_value].size() - 1) {
        float prev_value = m_tt_in_series ? m_samples_in[m_value].at(src_point - 1) :
                    m_samples_out[m_value].at(src_point - 1);
        float next_value = m_tt_in_series ? m_samples_in[m_value].at(src_point + 1) :
                    m_samples_out[m_value].at(src_point + 1);

        is_peak = src_value > prev_value && src_value > next_value;
        is_dip = src_value < prev_value && src_value < next_value;
    }

    int dst_point = 0;
    uint64_t avg_sample_interval = (m_values[dst_range] * 60 * 1000) / DESIRED_SAMPLES;
    int64_t time_window = avg_sample_interval * 3;
    int64_t min_difference = time_window * 2;

    // Find the nearest point timestamp-wise
    for (int i = 0; i < m_time_stamp[dst_range].size(); ++i) {
        auto diff = std::abs(m_time_stamp[dst_range].at(i) - src_timestamp);
        if (diff < min_difference) {
            min_difference = diff;
            dst_point = i;
        }
    }

    // Exit early if no point found or not a peak nor a dip
    if (!dst_point || (!is_peak && !is_dip)) return dst_point;

    // If a peak/dip, snap to a nearby peak/dip if one exists
    float dst_value = m_tt_in_series ? m_samples_in[dst_range].at(dst_point - 1) :
                m_samples_out[dst_range].at(dst_point - 1);
    float best_value = dst_value;
    int best_point = dst_point - 1;

    for (int i = best_point - 3; i <= best_point + 3; ++i) {
        if (i < 0 || i >= m_time_stamp[dst_range].size()) continue;
        if (std::abs(m_time_stamp[dst_range].at(i) - src_timestamp) > time_window) continue;
        float value = m_tt_in_series ? m_samples_in[dst_range].at(i) : m_samples_out[dst_range].at(i);
        if (is_peak && value > best_value) {
            dst_point = i + 1;
            best_value = value;
        } else if (is_dip && value < best_value) {
            dst_point = i + 1;
            best_value = value;
        }
    }

    return dst_point;
}
