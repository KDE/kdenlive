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

#ifndef AUDIOSIGNAL_H
#define AUDIOSIGNAL_H

#include "abstractaudioscopewidget.h"

#include <QByteArray>
#include <QList>
#include <QTimer>

#include <QWidget>

#include <cstdint>

class AudioSignal : public AbstractAudioScopeWidget
{
    Q_OBJECT
public:
    explicit AudioSignal(QWidget *parent = nullptr);
    ~AudioSignal() override;
    /** @brief Used for checking whether audio data needs to be delivered */
    bool monitoringEnabled() const;

    QRect scopeRect() override;
    QImage renderHUD(uint accelerationFactor) override;
    QImage renderBackground(uint accelerationFactor) override;
    QImage renderAudioScope(uint accelerationFactor, const audioShortVector &audioFrame, const int, const int num_channels, const int samples,
                            const int) override;

    QString widgetName() const override { return QStringLiteral("audioSignal"); }
    bool isHUDDependingOnInput() const override { return false; }
    bool isScopeDependingOnInput() const override { return true; }
    bool isBackgroundDependingOnInput() const override { return false; }

private:
    double valueToPixel(double in);
    QTimer m_timer;
    QByteArray m_channels, m_peeks, m_peekage;
    QList<int> m_dbscale;

public slots:
    void showAudio(const QByteArray &);
    void slotReceiveAudio(audioShortVector audioSamples, int, int num_channels, int samples);
private slots:
    void slotNoAudioTimeout();

signals:
    void updateAudioMonitoring();
};

#endif
