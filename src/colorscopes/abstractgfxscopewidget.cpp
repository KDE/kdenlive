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
#include "renderer.h"
#include "monitormanager.h"

#include <QtConcurrentRun>
#include <QFuture>
#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

// Uncomment for debugging.
//#define DEBUG_AGSW

#ifdef DEBUG_AGSW
#include <QDebug>
#endif

const int REALTIME_FPS = 30;

AbstractGfxScopeWidget::AbstractGfxScopeWidget(MonitorManager *manager, bool trackMouse, QWidget *parent) :
        AbstractScopeWidget(trackMouse, parent),
        m_manager(manager)
{
    m_activeRender = m_manager->activeRenderer();

    bool b = true;
    if (m_activeRender != NULL)
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
    if (!m_aAutoRefresh->isChecked() && m_activeRender) {
        m_activeRender->sendFrameUpdate();
    }
    AbstractScopeWidget::mouseReleaseEvent(event);
}


///// Slots /////

void AbstractGfxScopeWidget::slotActiveMonitorChanged()
{
    if (m_activeRender) {
        if (m_activeRender == m_manager->activeRenderer()) return;
        bool b = true;
        b &= m_activeRender->disconnect(this);
        Q_ASSERT(b);
    }
    m_activeRender = m_manager->activeRenderer();

    if (m_activeRender) {
#ifdef DEBUG_AGSW
    qDebug() << "Active monitor has changed in " << widgetName() << ". Is the clip monitor active now? " << m_activeRender->name();
#endif
        bool b = connect(m_activeRender, SIGNAL(frameUpdated(QImage)), this, SLOT(slotRenderZoneUpdated(QImage)));
        Q_ASSERT(b);
    }

    // Update the scope for the new monitor.
    forceUpdate(true);
}

void AbstractGfxScopeWidget::slotClearMonitor()
{
    m_activeRender = NULL;
}

void AbstractGfxScopeWidget::slotRenderZoneUpdated(QImage frame)
{
    m_scopeImage = frame;
    AbstractScopeWidget::slotRenderZoneUpdated();
}

void AbstractGfxScopeWidget::slotAutoRefreshToggled(bool autoRefresh)
{
    if (autoRefresh && m_activeRender) {
        m_activeRender->sendFrameUpdate();
    }
}


#ifdef DEBUG_AGSW
#undef DEBUG_AGSW
#endif
