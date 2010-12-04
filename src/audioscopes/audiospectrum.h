/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef AUDIOSPECTRUM_H
#define AUDIOSPECTRUM_H

#include <QtCore>

#include "abstractaudioscopewidget.h"
#include "ui_audiospectrum_ui.h"
#include "tools/kiss_fftr.h"

class AudioSpectrum_UI;

class AudioSpectrum : public AbstractAudioScopeWidget {
    Q_OBJECT

public:
    AudioSpectrum(QWidget *parent = 0);
    ~AudioSpectrum();

    // Implemented virtual methods
    QString widgetName() const;

    static const QString directions[]; // Mainly for debug output
    enum RescaleDirection { North, Northeast, East, Southeast };
    enum RescaleDimension { Min_dB, Max_dB, Max_Hz };


protected:
    ///// Implemented methods /////
    QRect scopeRect();
    QImage renderHUD(uint accelerationFactor);
    QImage renderAudioScope(uint accelerationFactor, const QVector<int16_t> audioFrame, const int freq, const int num_channels, const int num_samples);
    QImage renderBackground(uint accelerationFactor);
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;
    virtual void readConfig();
    void writeConfig();

    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    Ui::AudioSpectrum_UI *ui;
    kiss_fftr_cfg m_cfg;

    QAction *m_aLockHz;
    QAction *m_aLin;
    QAction *m_aLog;
    QActionGroup *m_agScale;

    QSize m_distance;

    /** Lower bound for the dB value to display */
    int m_dBmin;
    /** Upper bound (max: 0) */
    int m_dBmax;

    /** Maximum frequency (depends on the sampling rate)
        Stored for the HUD painter */
    uint m_freqMax;


    ///// Movement detection /////
    const int m_rescaleMinDist;
    const float m_rescaleVerticalThreshold;

    bool m_rescaleActive;
    bool m_rescalePropertiesLocked;
    bool m_rescaleFirstRescaleDone;
    short m_rescaleScale;
    Qt::KeyboardModifiers m_rescaleModifiers;
    AudioSpectrum::RescaleDirection m_rescaleClockDirection;
    QPoint m_rescaleStartPoint;



private slots:
    void slotUpdateCfg();

};

#endif // AUDIOSPECTRUM_H
