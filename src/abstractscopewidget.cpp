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

#include "abstractscopewidget.h"
#include "renderer.h"
#include "monitor.h"

#include <QFuture>
#include <QColor>
#include <QDebug>
#include <QMenu>
#include <QPainter>

const QColor light(250, 238, 226, 255);
const QColor dark ( 40,  40,  39, 255);
const QColor dark2( 25,  25,  23, 255);

AbstractScopeWidget::AbstractScopeWidget(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    QWidget(parent),
    m_projMonitor(projMonitor),
    m_clipMonitor(clipMonitor),
    offset(5),
    m_semaphoreHUD(1),
    m_semaphoreScope(1),
    m_semaphoreBackground(1),
    initialDimensionUpdateDone(false)

{
    m_scopePalette = QPalette();
    m_scopePalette.setBrush(QPalette::Window, QBrush(dark2));
    m_scopePalette.setBrush(QPalette::Base, QBrush(dark));
    m_scopePalette.setBrush(QPalette::Button, QBrush(dark));
    m_scopePalette.setBrush(QPalette::Text, QBrush(light));
    m_scopePalette.setBrush(QPalette::WindowText, QBrush(light));
    m_scopePalette.setBrush(QPalette::ButtonText, QBrush(light));
    this->setPalette(m_scopePalette);
    this->setAutoFillBackground(true);

    m_aAutoRefresh = new QAction(i18n("Auto Refresh"), this);
    m_aAutoRefresh->setCheckable(true);
    m_aRealtime = new QAction(i18n("Realtime (with precision loss)"), this);
    m_aRealtime->setCheckable(true);

    m_menu = new QMenu(this);
    m_menu->setPalette(m_scopePalette);
    m_menu->addAction(m_aAutoRefresh);
    m_menu->addAction(m_aRealtime);

    this->setContextMenuPolicy(Qt::CustomContextMenu);

    if (m_projMonitor->isActive()) {
        m_activeRender = m_projMonitor->render;
    } else {
        m_activeRender = m_clipMonitor->render;
    }

    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenuRequested(QPoint)));

    connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    connect(this, SIGNAL(signalHUDRenderingFinished(uint)), this, SLOT(slotHUDRenderingFinished(uint)));
    connect(this, SIGNAL(signalScopeRenderingFinished(uint)), this, SLOT(slotScopeRenderingFinished(uint)));
    connect(this, SIGNAL(signalBackgroundRenderingFinished(uint)), this, SLOT(slotBackgroundRenderingFinished(uint)));


}

AbstractScopeWidget::~AbstractScopeWidget()
{
    delete m_menu;
    delete m_aAutoRefresh;
}

void AbstractScopeWidget::prodHUDThread() {}

void AbstractScopeWidget::prodScopeThread()
{
    if (m_semaphoreScope.tryAcquire(1)) {
        Q_ASSERT(!m_threadScope.isRunning());

        m_newScopeFrames.fetchAndStoreRelaxed(0);
        m_newScopeUpdates.fetchAndStoreRelaxed(0);
        m_threadScope = QtConcurrent::run(this, &AbstractScopeWidget::renderScope);
        qDebug() << "Scope thread started in " << widgetName();

    } else {
        qDebug() << "Scope semaphore locked, not prodding in " << widgetName() << ". Thread running: " << m_threadScope.isRunning();
    }
}

void AbstractScopeWidget::prodBackgroundThread() {}


///// Events /////

void AbstractScopeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    prodHUDThread();
    prodScopeThread();
    prodBackgroundThread();
    QWidget::mouseReleaseEvent(event);
}

void AbstractScopeWidget::resizeEvent(QResizeEvent *event)
{
    // Update the dimension of the available rect for painting
    m_scopeRect = scopeRect();

    m_newHUDUpdates.fetchAndAddRelaxed(1);
    m_newScopeUpdates.fetchAndAddRelaxed(1);
    m_newBackgroundUpdates.fetchAndAddRelaxed(1);

    prodHUDThread();
    prodScopeThread();
    prodBackgroundThread();

    QWidget::resizeEvent(event);
}

void AbstractScopeWidget::paintEvent(QPaintEvent *)
{
//    qDebug() << "Painting now on scope " << widgetName();
    if (!initialDimensionUpdateDone) {
        // This is a workaround.
        // When updating the dimensions in the constructor, the size
        // of the control items at the top are simply ignored! So do
        // it here instead.
        m_scopeRect = scopeRect();
        initialDimensionUpdateDone = true;
    }

    QPainter davinci(this);
    davinci.drawImage(scopeRect().topLeft(), m_imgBackground);
    davinci.drawImage(scopeRect().topLeft(), m_imgScope);
    davinci.drawImage(scopeRect().topLeft(), m_imgHUD);
    davinci.fillRect(scopeRect(), QBrush(QColor(200, 100, 0, 16)));
}

void AbstractScopeWidget::customContextMenuRequested(const QPoint &pos)
{
    m_menu->exec(this->mapToGlobal(pos));
}


///// Slots /////

void AbstractScopeWidget::slotHUDRenderingFinished(uint)
{

}

void AbstractScopeWidget::slotScopeRenderingFinished(uint)
{
    qDebug() << "Scope rendering has finished, waiting for termination in " << widgetName();
    m_threadScope.waitForFinished();
    m_imgScope = m_threadScope.result();
    m_semaphoreScope.release(1);
    this->update();

    if ( (m_newScopeFrames > 0 && m_aAutoRefresh->isChecked()) || m_newScopeUpdates > 0) {
        qDebug() << "Trying to start a new thread for " << widgetName()
                << ". New frames/updates: " << m_newScopeFrames << "/" << m_newScopeUpdates;
    }
}

void AbstractScopeWidget::slotBackgroundRenderingFinished(uint)
{

}

void AbstractScopeWidget::slotActiveMonitorChanged(bool isClipMonitor)
{
    qDebug() << "Active monitor has changed in " << widgetName() << ". Is the clip monitor active now? " << isClipMonitor;
    if (isClipMonitor) {
        m_activeRender = m_clipMonitor->render;
        disconnect(this, SLOT(slotRenderZoneUpdated()));
        connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
        qDebug() << "Connected to clip monitor.";
    } else {
        m_activeRender = m_projMonitor->render;
        disconnect(this, SLOT(slotRenderZoneUpdated()));
        connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
        qDebug() << "Connected to project monitor.";
    }
    // Update the scope for the new monitor.
    prodHUDThread();
    prodScopeThread();
    prodBackgroundThread();
}

void AbstractScopeWidget::slotRenderZoneUpdated()
{
    m_newHUDFrames.fetchAndAddRelaxed(1);
    m_newScopeFrames.fetchAndAddRelaxed(1);
    m_newBackgroundFrames.fetchAndAddRelaxed(1);

    qDebug() << "Monitor incoming. New frames total HUD/Scope/Background: " << m_newHUDFrames
            << "/" << m_newScopeFrames << "/" << m_newBackgroundFrames;

    if (this->visibleRegion().isEmpty()) {
        qDebug() << "Scope of widget " << widgetName() << " is not at the top, not rendering.";
    } else {
        if (m_aAutoRefresh->isChecked()) {
            prodHUDThread();
            prodScopeThread();
            prodBackgroundThread();
        }
    }
}
