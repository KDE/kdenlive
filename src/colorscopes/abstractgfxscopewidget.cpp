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

#include "abstractgfxscopewidget.h"
#include "renderer.h"
#include "monitor.h"

#include <QFuture>
#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

const int REALTIME_FPS = 30;

AbstractGfxScopeWidget::AbstractGfxScopeWidget(Monitor *projMonitor, Monitor *clipMonitor, bool trackMouse, QWidget *parent) :
        AbstractScopeWidget(trackMouse, parent),
        m_clipMonitor(clipMonitor),
        m_projMonitor(projMonitor)

{
    m_activeRender = (m_clipMonitor->isActive()) ? m_clipMonitor->render : m_projMonitor->render;

    bool b = true;
    b &= connect(m_activeRender, SIGNAL(frameUpdated(QImage)), this, SLOT(slotRenderZoneUpdated(QImage)));
    Q_ASSERT(b);
}
AbstractGfxScopeWidget::~AbstractGfxScopeWidget() { }

QImage AbstractGfxScopeWidget::renderScope(uint accelerationFactor)
{
    return renderGfxScope(accelerationFactor, m_scopeImage);
}

void AbstractGfxScopeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_aAutoRefresh->isChecked()) {
        m_activeRender->sendFrameUpdate();
    }
    AbstractScopeWidget::mouseReleaseEvent(event);
}


///// Slots /////

void AbstractGfxScopeWidget::slotActiveMonitorChanged(bool isClipMonitor)
{
//    qDebug() << "Active monitor has changed in " << m_widgetName << ". Is the clip monitor active now? " << isClipMonitor;

    bool b = m_activeRender->disconnect(this);
    Q_ASSERT(b);

    m_activeRender = (isClipMonitor) ? m_clipMonitor->render : m_projMonitor->render;

    //b &= connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    b &= connect(m_activeRender, SIGNAL(frameUpdated(QImage)), this, SLOT(slotRenderZoneUpdated(QImage)));
    Q_ASSERT(b);

    // Update the scope for the new monitor.
    forceUpdate(true);
}

void AbstractGfxScopeWidget::slotRenderZoneUpdated(QImage frame)
{
    m_scopeImage = frame;
    AbstractScopeWidget::slotRenderZoneUpdated();
}

void AbstractGfxScopeWidget::slotAutoRefreshToggled(bool autoRefresh)
{
    if (autoRefresh) {
        m_activeRender->sendFrameUpdate();
    }
}
