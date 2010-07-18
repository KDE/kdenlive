/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <QWidget>

#include "ui_waveform_ui.h"
#include "renderer.h"
#include "monitor.h"
#include "waveformgenerator.h"

class Waveform_UI;
class WaveformGenerator;
class Render;
class Monitor;

class Waveform : public QWidget, public Ui::Waveform_UI {
    Q_OBJECT

public:
    Waveform(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    ~Waveform();

protected:
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);
    void mouseReleaseEvent(QMouseEvent *);

private:
    Monitor *m_projMonitor;
    Monitor *m_clipMonitor;
    Render *m_activeRender;

    WaveformGenerator *m_waveformGenerator;

    void updateDimensions();
    bool initialDimensionUpdateDone;

    QRect m_scopeRect;
    QImage m_waveform;

private slots:
    void slotActiveMonitorChanged(bool isClipMonitor);
    void slotRenderZoneUpdated();
    void slotWaveformCalculated(QImage waveform, const uint &msec);

};

#endif // WAVEFORM_H
