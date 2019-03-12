/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "abstractgfxscopewidget.h"
#include "monitor/monitormanager.h"

#include <QMouseEvent>

// Uncomment for debugging.
//#define DEBUG_AGSW

#ifdef DEBUG_AGSW
#endif

AbstractGfxScopeWidget::AbstractGfxScopeWidget(bool trackMouse, QWidget *parent)
    : AbstractScopeWidget(trackMouse, parent)
{
}

AbstractGfxScopeWidget::~AbstractGfxScopeWidget() = default;

QImage AbstractGfxScopeWidget::renderScope(uint accelerationFactor)
{
    QMutexLocker lock(&m_mutex);
    return renderGfxScope(accelerationFactor, m_scopeImage);
}

void AbstractGfxScopeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    AbstractScopeWidget::mouseReleaseEvent(event);
    emit signalFrameRequest(widgetName());
}

///// Slots /////

void AbstractGfxScopeWidget::slotRenderZoneUpdated(const QImage &frame)
{
    QMutexLocker lock(&m_mutex);
    m_scopeImage = frame;
    AbstractScopeWidget::slotRenderZoneUpdated();
}

void AbstractGfxScopeWidget::slotAutoRefreshToggled(bool autoRefresh)
{
    if (autoRefresh) {
        emit signalFrameRequest(widgetName());
    }
}

#ifdef DEBUG_AGSW
#undef DEBUG_AGSW
#endif
