/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster (simon.eu@gmail.com)
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include "abstractaudioscopewidget.h"

#include "monitor/monitor.h"

// Uncomment for debugging
//#define DEBUG_AASW

#ifdef DEBUG_AASW
#include "kdenlive_debug.h"
#endif

AbstractAudioScopeWidget::AbstractAudioScopeWidget(bool trackMouse, QWidget *parent)
    : AbstractScopeWidget(trackMouse, parent)
    , m_audioFrame()
    , m_newData(0)
{
}

void AbstractAudioScopeWidget::slotReceiveAudio(const audioShortVector &sampleData, int freq, int num_channels, int num_samples)
{
#ifdef DEBUG_AASW
    qCDebug(KDENLIVE_LOG) << "Received audio for " << widgetName() << '.';
#endif
    m_audioFrame = sampleData;
    m_freq = freq;
    m_nChannels = num_channels;
    m_nSamples = num_samples;

    m_newData.fetchAndAddAcquire(1);

    AbstractScopeWidget::slotRenderZoneUpdated();
}

AbstractAudioScopeWidget::~AbstractAudioScopeWidget() = default;

QImage AbstractAudioScopeWidget::renderScope(uint accelerationFactor)
{
    const int newData = m_newData.fetchAndStoreAcquire(0);

    return renderAudioScope(accelerationFactor, m_audioFrame, m_freq, m_nChannels, m_nSamples, newData);
}

#ifdef DEBUG_AASW
#undef DEBUG_AASW
#endif
