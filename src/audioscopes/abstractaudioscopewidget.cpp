/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "qtconcurrentrun.h"

#include "abstractaudioscopewidget.h"
#include "renderer.h"
#include "monitor.h"

#include <QDebug>
#include <QFuture>
#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

AbstractAudioScopeWidget::AbstractAudioScopeWidget(bool trackMouse, QWidget *parent) :
        AbstractScopeWidget(trackMouse, parent)
{
}

void AbstractAudioScopeWidget::slotReceiveAudio(const QVector<int16_t>& sampleData, int freq, int num_channels, int num_samples)
{
    m_audioFrame = sampleData;
    m_freq = freq;
    m_nChannels = num_channels;
    m_nSamples = num_samples;
    AbstractScopeWidget::slotRenderZoneUpdated();
}

AbstractAudioScopeWidget::~AbstractAudioScopeWidget() {}

QImage AbstractAudioScopeWidget::renderScope(uint accelerationFactor)
{
    return renderAudioScope(accelerationFactor, m_audioFrame, m_freq, m_nChannels, m_nSamples);
}
