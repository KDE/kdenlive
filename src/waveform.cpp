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

    connect(m_waveformGenerator, SIGNAL(signalCalculationFinished(QImage,uint)), this, SLOT(slotWaveformCalculated(QImage,uint)));
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

QString Waveform::widgetName() const
{
    return QString("Waveform");
}


///// Implemented methods /////
QImage Waveform::renderHUD()
{
    emit signalHUDRenderingFinished();
    return QImage();
}

QImage Waveform::renderScope()
{
    QImage wave = m_waveformGenerator->calculateWaveform(scopeRect().size(),
                                                         m_activeRender->extractFrame(m_activeRender->seekFramePosition()), true);
    emit signalScopeRenderingFinished();
    return wave;
}

QImage Waveform::renderBackground()
{
    emit signalBackgroundRenderingFinished();
    return QImage();
}


///// Events /////

void Waveform::paintEvent(QPaintEvent *)
{
    if (!initialDimensionUpdateDone) {
        // This is a workaround.
        // When updating the dimensions in the constructor, the size
        // of the control items at the top are simply ignored! So do
        // it here instead.
        scopeRect();
        initialDimensionUpdateDone = true;
    }

    QPainter davinci(this);
    QPoint vinciPoint;

    davinci.setRenderHint(QPainter::Antialiasing, true);
    davinci.fillRect(0, 0, this->size().width(), this->size().height(), QColor(25,25,23));

    davinci.drawImage(m_scopeRect.topLeft(), m_waveform);

}

//void Waveform::resizeEvent(QResizeEvent *event)
//{
//    updateDimensions();
//    m_waveformGenerator->calculateWaveform(m_scopeRect.size(), m_activeRender->extractFrame(m_activeRender->seekFramePosition()), true);
//    QWidget::resizeEvent(event);
//}

//void Waveform::mouseReleaseEvent(QMouseEvent *)
//{
//    qDebug() << "Calculating scope now. Size: " << m_scopeRect.size().width() << "x" << m_scopeRect.size().height();
//    m_waveformGenerator->calculateWaveform(m_scopeRect.size(), m_activeRender->extractFrame(m_activeRender->seekFramePosition()), true);
//}


///// Slots /////

void Waveform::slotWaveformCalculated(QImage waveform, const uint &msec)
{
    qDebug() << "Waveform received. Time taken for rendering: " << msec << " ms. Painting it.";
    m_waveform = waveform;
    this->update();
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
