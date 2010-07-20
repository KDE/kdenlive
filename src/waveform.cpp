/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QPainter>
#include <QPoint>
#include <QDebug>

#include "renderer.h"
#include "waveform.h"
#include "waveformgenerator.h"


Waveform::Waveform(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    AbstractScopeWidget(projMonitor, clipMonitor, parent),
    initialDimensionUpdateDone(false)
{
    ui = new Ui::Waveform_UI();
    ui->setupUi(this);

    m_waveformGenerator = new WaveformGenerator();
}

Waveform::~Waveform()
{
    delete m_waveformGenerator;
}



QRect Waveform::scopeRect()
{
    // Distance from top/left/right
    int offset = 6;

    QPoint topleft(offset, ui->line->y()+offset);

    return QRect(topleft, this->size() - QSize(offset+topleft.x(), offset+topleft.y()));
}


///// Implemented methods /////

QString Waveform::widgetName() const { return QString("Waveform"); }
bool Waveform::isHUDDependingOnInput() const { return false; }
bool Waveform::isScopeDependingOnInput() const { return true; }
bool Waveform::isBackgroundDependingOnInput() const { return false; }

QImage Waveform::renderHUD(uint)
{
    emit signalHUDRenderingFinished(0, 1);
    return QImage();
}

QImage Waveform::renderScope(uint accelFactor)
{
    QTime start = QTime::currentTime();
    start.start();

    QImage wave = m_waveformGenerator->calculateWaveform(scopeRect().size(),
                                                         m_activeRender->extractFrame(m_activeRender->seekFramePosition()), true, accelFactor);

    emit signalScopeRenderingFinished(start.elapsed(), 1);
    return wave;
}

QImage Waveform::renderBackground(uint)
{
    emit signalBackgroundRenderingFinished(0, 1);
    return QImage();
}
