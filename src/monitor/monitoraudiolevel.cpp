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
#include <QStylePainter>
#include <QVBoxLayout>

//----------------------------------------------------------------------------
// IEC standard dB scaling -- as borrowed from meterbridge (c) Steve Harris

static inline double IEC_Scale(double dB)
{
    double fScale = 1.0f;

    if (dB < -70.0f)
        fScale = 0.0f;
    else if (dB < -60.0f)
        fScale = (dB + 70.0f) * 0.0025f;
    else if (dB < -50.0f)
        fScale = (dB + 60.0f) * 0.005f + 0.025f;
    else if (dB < -40.0)
        fScale = (dB + 50.0f) * 0.0075f + 0.075f;
    else if (dB < -30.0f)
        fScale = (dB + 40.0f) * 0.015f + 0.15f;
    else if (dB < -20.0f)
        fScale = (dB + 30.0f) * 0.02f + 0.3f;
    else if (dB < -0.001f || dB > 0.001f)  /* if (dB < 0.0f) */
        fScale = (dB + 20.0f) * 0.025f + 0.5f;

    return fScale;
}

static inline double IEC_ScaleMax(double dB, double max)
{
    return IEC_Scale(dB) / IEC_Scale(max);
}


MyProgressBar::MyProgressBar(int height, QWidget *parent) : QProgressBar(parent)
  , m_peak(0)
{
    setMaximumHeight(height);
    m_gradient.setColorAt(0.0, QColor(Qt::darkGreen));
    m_gradient.setColorAt(0.7142, QColor(Qt::green));
    m_gradient.setColorAt(0.7143, Qt::yellow);
    m_gradient.setColorAt(0.881, Qt::darkYellow);
    m_gradient.setColorAt(0.9525, Qt::red);
}

void MyProgressBar::setAudioValue(int value)
{
    m_peak -= 1;
    if (value >= m_peak && value <= 100) {
        m_peak = value;
    }
    setValue(value);
}

void MyProgressBar::paintEvent(QPaintEvent *pe)
{
    QStylePainter p(this);
    QStyleOptionProgressBarV2 opt;
    initStyleOption(&opt);
    QRect r = opt.rect;
    int pos = QStyle::sliderPositionFromValue(minimum(), maximum(), value(), r.width());
    int peak = QStyle::sliderPositionFromValue(minimum(), maximum(), m_peak, r.width());
    r.setWidth(pos);
    m_gradient.setStart(opt.rect.topLeft());
    m_gradient.setFinalStop(opt.rect.topRight());
    QPainterPath path;
    path.addRoundedRect(opt.rect, 4, 4);
    QPalette pal = palette();
    p.fillPath(path, pal.color(QPalette::Dark));
    p.setClipPath(path);
    p.fillRect(r, QBrush(m_gradient));
    if (peak > 0) {
        r.setLeft(peak);
        r.setWidth(2);
        p.fillRect(r, pal.highlight());
    }
    p.setClipping(false);
    p.setBrush(Qt::NoBrush);
    p.setPen(pal.color(QPalette::Dark));
    p.setOpacity(0.4);
    p.drawPath(path);
}


MonitorAudioLevel::MonitorAudioLevel(QObject *parent) : QObject(parent)
  , m_pBar1(0)
  , m_pBar2(0)
{
}

QWidget *MonitorAudioLevel::createProgressBar(int height, QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    w->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    QVBoxLayout *lay = new QVBoxLayout;
    w->setLayout(lay);
    m_pBar1 = new MyProgressBar(height / 4, w);
    m_pBar1->setRange(0, 100);
    m_pBar1->setValue(0);
    m_pBar2 = new MyProgressBar(height / 4, w);
    m_pBar2->setRange(0, 100);
    m_pBar2->setValue(0);
    lay->addWidget(m_pBar1);
    lay->addWidget(m_pBar2);
    return w;
}

void MonitorAudioLevel::slotAudioLevels(const audioLevelVector &dbLevels)
{
    m_levels = dbLevels;
    int channels = m_levels.size();
    if (m_peaks.size() != channels) {
        m_peaks = m_levels;
        //calcGraphRect();
    } else {
        for (int i = 0; i < channels; i++)
        {
            m_peaks[i] = m_peaks[i] - 0.2;
            if (m_levels[i] >= m_peaks[i]) {
                m_peaks[i] = m_levels[i];
            }
        }
    }
    m_pBar1->setAudioValue(IEC_ScaleMax(m_levels.at(0), 0) * 100);
    if (channels > 1) m_pBar2->setAudioValue(IEC_ScaleMax(m_levels.at(1), 0) * 100);
}

void MonitorAudioLevel::setMonitorVisible(bool visible)
{
    return;
    if (m_pBar1)
        m_pBar1->setVisible(visible);
    if (m_pBar2)
        m_pBar2->setVisible(visible);
}
