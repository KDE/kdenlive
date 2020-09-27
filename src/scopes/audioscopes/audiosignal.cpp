/***************************************************************************
 *   Copyright (C) 2010 by Marco Gittler (g.marco@freenet.de)              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "audiosignal.h"

#include <QPainter>
#include <QElapsedTimer>

#include <cmath>

AudioSignal::AudioSignal(QWidget *parent)
    : AbstractAudioScopeWidget(false, parent)
{
    setMinimumHeight(10);
    setMinimumWidth(10);
    m_dbscale << 0 << -1 << -2 << -3 << -4 << -5 << -6 << -8 << -10 << -20 << -40;
    m_menu->removeAction(m_aRealtime);
    connect(&m_timer, &QTimer::timeout, this, &AudioSignal::slotNoAudioTimeout);
    init();
}

AudioSignal::~AudioSignal() = default;

QImage AudioSignal::renderAudioScope(uint, const audioShortVector &audioFrame, const int, const int num_channels, const int samples, const int)
{
    QElapsedTimer timer;
    timer.start();

    int num_samples = samples > 200 ? 200 : samples;

    QByteArray chanAvg;
    for (int i = 0; i < num_channels; ++i) {
        long val = 0;
        for (int s = 0; s < num_samples; s++) {
            val += abs(audioFrame[i + s * num_channels] / 128);
        }
        chanAvg.append(char(val / num_samples));
    }

    if (m_peeks.count() != chanAvg.count()) {
        m_peeks = QByteArray(chanAvg.count(), 0);
        m_peekage = QByteArray(chanAvg.count(), 0);
    }
    for (int chan = 0; chan < m_peeks.count(); chan++) {
        m_peekage[chan] = char(m_peekage[chan] + 1);
        if (m_peeks.at(chan) < chanAvg.at(chan) || m_peekage.at(chan) > 50) {
            m_peekage[chan] = 0;
            m_peeks[chan] = chanAvg[chan];
        }
    }

    QImage image(m_scopeRect.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setPen(Qt::white);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    p.setRenderHint(QPainter::Antialiasing, false);

    int numchan = chanAvg.size();
    bool horiz = width() > height();
    int dbsize = 20;
    if (!horiz) {
        // calculate actual width of lowest=longest db scale mark based on drawing font
        dbsize = p.fontMetrics().horizontalAdvance(QString::number(m_dbscale.at(m_dbscale.size() - 1)));
    }
    bool showdb = width() > (dbsize + 40);
    // valpixel=1.0 for 127, 1.0+(1/40) for 1 short oversample, 1.0+(2/40) for longer oversample
    for (int i = 0; i < numchan; ++i) {
        // int maxx= (unsigned char)m_channels[i] * (horiz ? width() : height() ) / 127;
        double valpixel = valueToPixel((double)(unsigned char)chanAvg[i] / 127.0);
        int maxx = height() * valpixel;
        int xdelta = height() / 42;
        int _y2 = (showdb ? width() - dbsize : width()) / numchan - 1;
        int _y1 = (showdb ? width() - dbsize : width()) * i / numchan;
        int _x2 = maxx > xdelta ? xdelta - 3 : maxx - 3;
        if (horiz) {
            dbsize = 9;
            showdb = height() > (dbsize);
            maxx = width() * valpixel;
            xdelta = width() / 42;
            _y2 = (showdb ? height() - dbsize : height()) / numchan - 1;
            _y1 = (showdb ? height() - dbsize : height()) * i / numchan;
            _x2 = maxx > xdelta ? xdelta - 1 : maxx - 1;
        }

        for (int x = 0; x <= 42; ++x) {
            int _x1 = x * xdelta;
            QColor sig = Qt::green;
            // value of actual painted digit
            double ival = (double)_x1 / (double)xdelta / 42.0;
            if (ival > 40.0 / 42.0) {
                sig = Qt::red;
            } else if (ival > 37.0 / 42.0) {
                sig = Qt::darkYellow;
            } else if (ival > 30.0 / 42.0) {
                sig = Qt::yellow;
            }
            if (maxx > 0) {
                if (horiz) {
                    p.fillRect(_x1, _y1, _x2, _y2, QBrush(sig, Qt::SolidPattern));
                } else {
                    p.fillRect(_y1, height() - _x1, _y2, -_x2, QBrush(sig, Qt::SolidPattern));
                }
                maxx -= xdelta;
            }
        }
        int xp = valueToPixel((double)m_peeks.at(i) / 127.0) * (horiz ? width() : height()) - 2;
        p.fillRect(horiz ? xp : _y1, horiz ? _y1 : height() - xdelta - xp, horiz ? 3 : _y2, horiz ? _y2 : 3, QBrush(Qt::gray, Qt::SolidPattern));
    }
    if (showdb) {
        // draw db value at related pixel
        for (int l : qAsConst(m_dbscale)) {
            if (!horiz) {
                double xf = pow(10.0, (double)l / 20.0) * (double)height();
                p.drawText(width() - dbsize, height() - xf * 40.0 / 42.0 + 20, QString::number(l));
            } else {
                double xf = pow(10.0, (double)l / 20.0) * (double)width();
                p.drawText(xf * 40 / 42 - 10, height() - 2, QString::number(l));
            }
        }
    }
    p.end();
    emit signalScopeRenderingFinished((uint)timer.elapsed(), 1);
    return image;
}

QRect AudioSignal::scopeRect()
{
    return {0, 0, width(), height()};
}

QImage AudioSignal::renderHUD(uint)
{
    return QImage();
}
QImage AudioSignal::renderBackground(uint)
{
    return QImage();
}

void AudioSignal::slotReceiveAudio(audioShortVector audioSamples, int, int num_channels, int samples)
{

    int num_samples = samples > 200 ? 200 : samples;

    QByteArray chanSignal;
    int num_oversample = 0;
    for (int i = 0; i < num_channels; ++i) {
        long val = 0;
        double over1 = 0.0;
        double over2 = 0.0;
        for (int s = 0; s < num_samples; s++) {
            int sample = abs(audioSamples[i + s * num_channels] / 128);
            val += sample;
            if (sample == 128) {
                num_oversample++;
            } else {
                num_oversample = 0;
            }
            // if 3 samples over max => 1 peak over 0 db (0db=40.0)
            if (num_oversample > 3) {
                over1 = 41.0 / 42.0 * 127;
            }
            // 10 samples @max => show max signal
            if (num_oversample > 10) {
                over2 = 127;
            }
        }
        // max amplitude = 40/42, 3to10  oversamples=41, more then 10 oversamples=42
        if (over2 > 0.0) {
            chanSignal.append(over2);
        } else if (over1 > 0.0) {
            chanSignal.append(over1);
        } else {
            chanSignal.append(char((double)val / (double)num_samples * 40.0 / 42.0));
        }
    }
    showAudio(chanSignal);
    m_timer.start(1000);
}

void AudioSignal::slotNoAudioTimeout()
{
    m_peeks.fill(0);
    showAudio(QByteArray(2, 0));
    m_timer.stop();
}

void AudioSignal::showAudio(const QByteArray &arr)
{
    m_channels = arr;
    if (m_peeks.count() != m_channels.count()) {
        m_peeks = QByteArray(m_channels.count(), 0);
        m_peekage = QByteArray(m_channels.count(), 0);
    }
    for (int chan = 0; chan < m_peeks.count(); chan++) {
        m_peekage[chan] = char(m_peekage[chan] + 1);
        if (m_peeks.at(chan) < arr.at(chan) || m_peekage.at(chan) > 50) {
            m_peekage[chan] = 0;
            m_peeks[chan] = arr[chan];
        }
    }
    update();
}

double AudioSignal::valueToPixel(double in)
{
    // in=0 -> return 0 (null length from max), in=127/127 return 1 (max length )
    return 1.0 - log10(in) / log10(1.0 / 127.0);
}
