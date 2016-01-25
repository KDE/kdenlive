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

#include "monitoraudiolevel.h"

#include <QPainter>
#include <QPaintEvent>
#include <QStylePainter>
#include <QVBoxLayout>
#include <QFont>
#include <QDebug>

static inline double levelToDB(double dB)
{
    if (dB == 0) return 0;
    return 100 * (1.0 - log10(dB)/log10(1.0/127));
}

MyAudioWidget::MyAudioWidget(int height, QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(height);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
}


void MyAudioWidget::resizeEvent ( QResizeEvent * event )
{
    drawBackground(m_peaks.size());
    QWidget::resizeEvent(event);
}

void MyAudioWidget::refreshPixmap()
{
    drawBackground(m_peaks.size());
}

void MyAudioWidget::drawBackground(int channels)
{
    QSize newSize = QWidget::size();
    if (!newSize.isValid()) return;
    QFont ft = font();
    ft.setPixelSize(newSize.height() / 3);
    setFont(ft);
    int textHeight = fontMetrics().ascent();
    newSize.setHeight(newSize.height() - textHeight);
    QLinearGradient gradient(0, 0, newSize.width(), 0);
    gradient.setColorAt(0.0, Qt::darkGreen);
    gradient.setColorAt(0.755, Qt::green);
    gradient.setColorAt(0.756, Qt::yellow); // -2db
    gradient.setColorAt(0.93, Qt::yellow);
    gradient.setColorAt(0.94, Qt::red); // -1db
    m_pixmap = QPixmap(QWidget::size());
    if (m_pixmap.isNull()) return;
    m_pixmap.fill(Qt::transparent);
    int totalHeight;
    if (channels < 2) {
        m_channelHeight = newSize.height() / 2;
        totalHeight = m_channelHeight;
    } else {
        m_channelHeight = (newSize.height() - (channels -1)) / channels;
        totalHeight = channels * m_channelHeight + (channels -1);
    }
    QRect rect(0, 0, newSize.width(), totalHeight);
    QPainter p(&m_pixmap);
    p.setOpacity(0.4);
    p.setFont(ft);
    p.fillRect(rect, QBrush(gradient));

    // Channel labels are horizontal along the bottom.
    QVector<int> dbscale;
    dbscale << 0 << -1 << -2 << -3 << -4 << -5 << -7 << -9 << -13 << -20 << -30 << -45;
    int dbLabelCount = dbscale.size();
    // dB scale is horizontal along the bottom
    int prevX = m_pixmap.width() * 2;
    int y = totalHeight + textHeight;
    for (int i = 0; i < dbLabelCount; i++) {
        int value = dbscale.at(i);
        QString label = QString().sprintf("%d", value);
        int labelWidth = fontMetrics().width(label);
        double xf=pow(10.0,(double)dbscale.at(i) / 50.0 )*m_pixmap.width()*40.0/42;
        if (xf + labelWidth / 2 > m_pixmap.width()) {
            xf = width() - labelWidth / 2;
        }
        if (prevX - (xf + labelWidth / 2) >= 2) {
            p.setPen(palette().dark().color());
            p.drawLine(xf, 0, xf, totalHeight - 1);
            xf -= labelWidth / 2;
            p.setPen(palette().text().color().rgb());
            p.drawText((int) xf, y, label);
            prevX = xf;
        }
    }
    p.setOpacity(1);
    p.setPen(palette().dark().color());
    // Clear space between the 2 channels
    p.setCompositionMode(QPainter::CompositionMode_Source);
    for (int i = 0; i < channels; i++) {
        p.drawRect(0, i * m_channelHeight + (2 * i), rect.width() - 1, m_channelHeight - 1);
        if (i > 0) p.fillRect(0, i * m_channelHeight + (2 * i) - 2, rect.width(), 2, Qt::transparent);
    }
    p.end();
}

void MyAudioWidget::setAudioValues(const QList <int>& values)
{
    m_values = values;
    if (m_peaks.size() != m_values.size()) {
        m_peaks = values;
        drawBackground(values.size());
    } else {
        for (int i = 0; i < m_values.size(); i++) {
            m_peaks[i] --;
            if (m_values.at(i) > m_peaks.at(i)) {
                m_peaks[i] = m_values.at(i);
            }
        }
    }
    update();
}

void MyAudioWidget::paintEvent(QPaintEvent *pe)
{
    QPainter p(this);
    p.setClipRect(pe->rect());
    QRect rect(0, 0, width(), height());
    QList <int> vals = m_values;
    if (vals.isEmpty()) {
        p.setOpacity(0.2);
        p.drawPixmap(rect, m_pixmap);
        return;
    }
    p.drawPixmap(rect, m_pixmap);
    p.setPen(palette().dark().color());
    p.setOpacity(0.9);
    for (int i = 0; i < m_values.count(); i++) {
        if (m_values.at(i) >= 100) continue;
        int val = (50 + m_values.at(i)) / 150.0 * rect.width();
        p.fillRect(val, i * m_channelHeight + (2 * i) + 1, rect.width() - 1 - val, m_channelHeight - 2, palette().dark());
        p.fillRect((50 + m_peaks.at(i)) / 150.0 * rect.width(), i * m_channelHeight + 1 + (2 * i), 1, m_channelHeight - 2, palette().text());
    }
}


MonitorAudioLevel::MonitorAudioLevel(QObject *parent) : QObject(parent)
  , m_pBar1(0)
{
}

QWidget *MonitorAudioLevel::createProgressBar(int height, QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    w->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    QVBoxLayout *lay = new QVBoxLayout;
    lay->setContentsMargins(2, 0, 2, 0);
    w->setLayout(lay);
    m_pBar1 = new MyAudioWidget(height, w);
    w->setMinimumHeight(height);
    lay->addWidget(m_pBar1);
    return w;
}

void MonitorAudioLevel::slotAudioLevels(const QVector<double> &dbLevels)
{
    QList <int> levels;
    if (!dbLevels.isEmpty()) {
        for (int i = 0; i < dbLevels.count(); i++) {
            levels << (int) levelToDB(dbLevels.at(i));
        }
    }
    m_pBar1->setAudioValues(levels);
}

void MonitorAudioLevel::setMonitorVisible(bool visible)
{
    if (m_pBar1) {
        m_pBar1->setVisible(visible);
    }
}

void MonitorAudioLevel::refreshPixmap()
{
    if (m_pBar1) {
        m_pBar1->refreshPixmap();
    }
}
