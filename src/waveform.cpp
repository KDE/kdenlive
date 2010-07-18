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

#include "waveform.h"


Waveform::Waveform(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    QWidget(parent),
    m_projMonitor(projMonitor),
    m_clipMonitor(clipMonitor),
    m_activeRender(clipMonitor->render),
    initialDimensionUpdateDone(false)
{
    setupUi(this);
    m_waveformGenerator = new WaveformGenerator();

    connect(m_waveformGenerator, SIGNAL(signalCalculationFinished(QImage,uint)), this, SLOT(slotWaveformCalculated(QImage,uint)));
}

Waveform::~Waveform()
{
    delete m_waveformGenerator;
}



void Waveform::updateDimensions()
{
    // Distance from top/left/right
    int offset = 6;

    QPoint topleft(offset, line->y()+offset);

    m_scopeRect = QRect(topleft, this->size() - QSize(offset+topleft.x(), offset+topleft.y()));
}


///// Events /////

void Waveform::paintEvent(QPaintEvent *)
{
    if (!initialDimensionUpdateDone) {
        // This is a workaround.
        // When updating the dimensions in the constructor, the size
        // of the control items at the top are simply ignored! So do
        // it here instead.
        updateDimensions();
        initialDimensionUpdateDone = true;
    }

    QPainter davinci(this);
    QPoint vinciPoint;

    davinci.setRenderHint(QPainter::Antialiasing, true);
    davinci.fillRect(0, 0, this->size().width(), this->size().height(), QColor(25,25,23));

    davinci.drawImage(m_scopeRect.topLeft(), m_waveform);

}

void Waveform::resizeEvent(QResizeEvent *event)
{
    updateDimensions();
    m_waveformGenerator->calculateWaveform(m_scopeRect.size(), m_activeRender->extractFrame(m_activeRender->seekFramePosition()), true);
    QWidget::resizeEvent(event);
}

void Waveform::mouseReleaseEvent(QMouseEvent *)
{
    qDebug() << "Calculating scope now. Size: " << m_scopeRect.size().width() << "x" << m_scopeRect.size().height();
    m_waveformGenerator->calculateWaveform(m_scopeRect.size(), m_activeRender->extractFrame(m_activeRender->seekFramePosition()), true);
}


///// Slots /////

void Waveform::slotWaveformCalculated(QImage waveform, const uint &msec)
{
    qDebug() << "Waveform received. Time taken for rendering: " << msec << " ms. Painting it.";
    m_waveform = waveform;
    this->update();
}

void Waveform::slotActiveMonitorChanged(bool isClipMonitor)
{
    if (isClipMonitor) {
        m_activeRender = m_clipMonitor->render;
        disconnect(this, SLOT(slotRenderZoneUpdated()));
        connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    } else {
        m_activeRender = m_projMonitor->render;
        disconnect(this, SLOT(slotRenderZoneUpdated()));
        connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    }
}

void Waveform::slotRenderZoneUpdated()
{
    qDebug() << "Monitor incoming.";//New frames total: " << newFrames;
    // Monitor has shown a new frame
//    newFrames.fetchAndAddRelaxed(1);
//    if (cbAutoRefresh->isChecked()) {
//        prodCalcThread();
//    }
}
