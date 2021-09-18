/*
    SPDX-FileCopyrightText: 2010 Marco Gittler (g.marco@freenet.de)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

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
