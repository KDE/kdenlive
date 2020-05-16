/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "audiolevelwidget.hpp"
#include "core.h"
#include "profiles/profilemodel.hpp"

#include "mlt++/Mlt.h"

#include <cmath>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QFontDatabase>

AudioLevelWidget::AudioLevelWidget(int width, QWidget *parent)
    : QWidget(parent)
    , audioChannels(pCore->audioChannels())
    , m_width(width)
    , m_channelWidth(width / 2)
    , m_channelDistance(2)
    , m_channelFillWidth(m_channelWidth)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    QFont ft(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    ft.setPointSizeF(ft.pointSize() * 0.8);
    setFont(ft);
    setMinimumWidth(4);
}

AudioLevelWidget::~AudioLevelWidget()
{
}

void AudioLevelWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    drawBackground(m_peaks.size());
}

void AudioLevelWidget::refreshPixmap()
{
    drawBackground(m_peaks.size());
}

void AudioLevelWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        drawBackground(m_peaks.size());
    }
    QWidget::changeEvent(event);
}

void AudioLevelWidget::drawBackground(int channels)
{
    if (height() == 0) {
        return;
    }
    QSize newSize = QWidget::size();
    if (!newSize.isValid()) {
        return;
    }
    m_offset = fontMetrics().boundingRect(QStringLiteral("-45")).width() + 5;
    newSize.setWidth(newSize.width() - m_offset - 1);
    QLinearGradient gradient(0, newSize.height(), 0, 0);
    gradient.setColorAt(0.0, Qt::darkGreen);
    gradient.setColorAt(0.379, Qt::darkGreen);
    gradient.setColorAt(0.38, Qt::green); // -20db
    gradient.setColorAt(0.868, Qt::green);
    gradient.setColorAt(0.869, Qt::yellow); // -2db
    gradient.setColorAt(0.95, Qt::yellow);
    gradient.setColorAt(0.951, Qt::red); // 0db
    m_pixmap = QPixmap(QWidget::size());
    if (m_pixmap.isNull()) {
        return;
    }
    m_pixmap.fill(Qt::transparent);
    int totalWidth;
    if (channels < 2) {
        m_channelWidth = newSize.width() / 2;
        totalWidth = m_channelWidth;
    } else {
        m_channelWidth = (newSize.width() - (channels - 1)) / channels;
        totalWidth= channels * m_channelWidth + (channels - 1);
    }
    QRect rect(m_offset, 0, newSize.width(), newSize.height());
    QPainter p(&m_pixmap);
    p.setOpacity(isEnabled() ? 0.8 : 0.4);
    p.setFont(font());
    p.fillRect(rect, QBrush(gradient));

    // Channel labels are vertical along the bottom.
    QVector<int> dbscale;
    dbscale << 0 << -2 << -5 << -10 << -15 << -20 << -30 << -45;
    int dbLabelCount = dbscale.size();
    // dB scale is vertical along the bottom
    int labelHeight = fontMetrics().ascent();
    for (int i = 0; i < dbLabelCount; i++) {
        int value = dbscale.at(i);
        QString label = QString::number(value);
        //int labelWidth = fontMetrics().width(label);
        double xf = m_pixmap.height() - pow(10.0, (double)dbscale.at(i) / 50.0) * m_pixmap.height() * 40.0 / 42;
        /*if (xf + labelWidth / 2 > m_pixmap.height()) {
            xf = height() - labelWidth / 2;
        }*/
        p.setPen(palette().dark().color());
        p.drawLine(m_offset, xf, m_offset + totalWidth - 1, xf);
        xf -= labelHeight * 2 / 3;
        p.setPen(palette().text().color().rgb());
        p.drawText(QRectF(0, xf, m_offset - 5, labelHeight), label, QTextOption(Qt::AlignRight));
    }
    p.setOpacity(isEnabled() ? 1 : 0.5);
    p.setPen(palette().dark().color());
    // Clear space between the channels
    p.setCompositionMode(QPainter::CompositionMode_Source);
    if (m_channelWidth < 4) {
        // too many audio channels, simple line between channels
        m_channelDistance = 1;
        m_channelFillWidth = m_channelWidth;
        for (int i = 0; i < channels; i++) {
            p.drawLine(m_offset + i * (m_channelWidth + m_channelDistance), 0, i * (m_channelWidth + m_channelDistance), rect.height() - 1);
        }
    } else {
        m_channelDistance = 2;
        m_channelFillWidth = m_channelWidth - 2;
        for (int i = 0; i < channels; i++) {
            p.drawRect(m_offset + i * (m_channelWidth + m_channelDistance), 0, m_channelWidth - 1, rect.height() - 1);
            if (i > 0) {
                p.fillRect(m_offset + i * (m_channelWidth + m_channelDistance) - 2, 0, 2, rect.height(), Qt::transparent);
            }
        }
    }
    p.end();
}

// cppcheck-suppress unusedFunction
void AudioLevelWidget::setAudioValues(const QVector<double> &values)
{
    m_values = values;
    if (m_peaks.size() != m_values.size()) {
        m_peaks = values;
        drawBackground(values.size());
    } else {
        for (int i = 0; i < m_values.size(); i++) {
            m_peaks[i] -= .003;
            if (m_values.at(i) > m_peaks.at(i)) {
                m_peaks[i] = m_values.at(i);
            }
        }
    }
    update();
}

void AudioLevelWidget::setVisibility(bool enable)
{
    if (enable) {
        setVisible(true);
        setFixedWidth(m_width);
    } else {
        // set height to 0 so the toolbar layout is not affected
        setFixedHeight(0);
        setVisible(false);
    }
}

void AudioLevelWidget::paintEvent(QPaintEvent *pe)
{
    if (!isVisible()) {
        return;
    }
    QPainter p(this);
    p.setClipRect(pe->rect());
    QRect rect(0, 0, width(), height());
    if (m_values.isEmpty()) {
        p.setOpacity(0.2);
        p.drawPixmap(rect, m_pixmap);
        return;
    }
    p.drawPixmap(rect, m_pixmap);
    p.setPen(palette().dark().color());
    p.setOpacity(0.9);
    for (int i = 0; i < m_values.count(); i++) {
        if (m_values.at(i) >= 100) {
            continue;
        }
        //int val = (50 + m_values.at(i)) / 150.0 * rect.height();
        p.fillRect(m_offset + i * (m_channelWidth + m_channelDistance) + 1, 0, m_channelFillWidth, height() - (m_values.at(i) * rect.height()), palette().dark());
        p.fillRect(m_offset + i * (m_channelWidth + m_channelDistance) + 1, height() - (m_peaks.at(i) * rect.height()), m_channelFillWidth, 1, palette().text());
    }
}
