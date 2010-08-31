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
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

const int REALTIME_FPS = 30;

const QColor light(250, 238, 226, 255);
const QColor dark ( 40,  40,  39, 255);
const QColor dark2( 25,  25,  23, 255);

const QPen AbstractScopeWidget::penThick(QBrush(QColor(250,250,250)),     2, Qt::SolidLine);
const QPen AbstractScopeWidget::penThin (QBrush(QColor(250,250,250)),     1, Qt::SolidLine);
const QPen AbstractScopeWidget::penLight(QBrush(QColor(200,200,250,150)), 1, Qt::SolidLine);
const QPen AbstractScopeWidget::penDark (QBrush(QColor(0,0,20,250)),      1, Qt::SolidLine);

AbstractScopeWidget::AbstractScopeWidget(Monitor *projMonitor, Monitor *clipMonitor, bool trackMouse, QWidget *parent) :
    QWidget(parent),
    m_projMonitor(projMonitor),
    m_clipMonitor(clipMonitor),
    m_mousePos(0,0),
    m_mouseWithinWidget(false),
    offset(5),
    m_accelFactorHUD(1),
    m_accelFactorScope(1),
    m_accelFactorBackground(1),
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

    m_activeRender = (m_clipMonitor->isActive()) ? m_clipMonitor->render : m_projMonitor->render;

    bool b = true;
    b &= connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenuRequested(QPoint)));

    b &= connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    b &= connect(m_activeRender, SIGNAL(frameUpdated(QImage)), this, SLOT(slotRenderZoneUpdated(QImage)));

    b &= connect(this, SIGNAL(signalHUDRenderingFinished(uint,uint)), this, SLOT(slotHUDRenderingFinished(uint,uint)));
    b &= connect(this, SIGNAL(signalScopeRenderingFinished(uint,uint)), this, SLOT(slotScopeRenderingFinished(uint,uint)));
    b &= connect(this, SIGNAL(signalBackgroundRenderingFinished(uint,uint)), this, SLOT(slotBackgroundRenderingFinished(uint,uint)));
    b &= connect(m_aRealtime, SIGNAL(toggled(bool)), this, SLOT(slotResetRealtimeFactor(bool)));
    b &= connect(m_aAutoRefresh, SIGNAL(toggled(bool)), this, SLOT(slotAutoRefreshToggled(bool)));
    Q_ASSERT(b);

    // Enable mouse tracking if desired.
    // Causes the mouseMoved signal to be emitted when the mouse moves inside the
    // widget, even when no mouse button is pressed.
    this->setMouseTracking(trackMouse);
}

AbstractScopeWidget::~AbstractScopeWidget()
{
    writeConfig();

    delete m_menu;
    delete m_aAutoRefresh;
    delete m_aRealtime;
}

void AbstractScopeWidget::init()
{
    m_widgetName = widgetName();
    readConfig();
}

void AbstractScopeWidget::readConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    m_aAutoRefresh->setChecked(scopeConfig.readEntry("autoRefresh", true));
    m_aRealtime->setChecked(scopeConfig.readEntry("realtime", false));
    scopeConfig.sync();
}

void AbstractScopeWidget::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("autoRefresh", m_aAutoRefresh->isChecked());
    scopeConfig.writeEntry("realtime", m_aRealtime->isChecked());
    scopeConfig.sync();
}

QString AbstractScopeWidget::configName() { return "Scope_" + m_widgetName; }

void AbstractScopeWidget::prodHUDThread()
{
    if (this->visibleRegion().isEmpty()) {
//        qDebug() << "Scope " << m_widgetName << " is not visible. Not calculating HUD.";
    } else {
        if (m_semaphoreHUD.tryAcquire(1)) {
            Q_ASSERT(!m_threadHUD.isRunning());

            m_newHUDFrames.fetchAndStoreRelaxed(0);
            m_newHUDUpdates.fetchAndStoreRelaxed(0);
            m_threadHUD = QtConcurrent::run(this, &AbstractScopeWidget::renderHUD, m_accelFactorHUD);
//            qDebug() << "HUD thread started in " << m_widgetName;

        } else {
//            qDebug() << "HUD semaphore locked, not prodding in " << m_widgetName << ". Thread running: " << m_threadHUD.isRunning();
        }
    }
}

void AbstractScopeWidget::prodScopeThread()
{
    // Only start a new thread if the scope is actually visible
    // and not hidden by another widget on the stack.
    if (this->visibleRegion().isEmpty()) {
//        qDebug() << "Scope " << m_widgetName << " is not visible. Not calculating scope.";
    } else {
        // Try to acquire the semaphore. This must only succeed if m_threadScope is not running
        // anymore. Therefore the semaphore must NOT be released before m_threadScope ends.
        // If acquiring the semaphore fails, the thread is still running.
        if (m_semaphoreScope.tryAcquire(1)) {
            Q_ASSERT(!m_threadScope.isRunning());

            m_newScopeFrames.fetchAndStoreRelaxed(0);
            m_newScopeUpdates.fetchAndStoreRelaxed(0);

            Q_ASSERT(m_accelFactorScope > 0);

            // See http://doc.qt.nokia.com/latest/qtconcurrentrun.html#run about
            // running member functions in a thread
            m_threadScope = QtConcurrent::run(this, &AbstractScopeWidget::renderScope, m_accelFactorScope, m_scopeImage);

//            qDebug() << "Scope thread started in " << m_widgetName;

        } else {
//            qDebug() << "Scope semaphore locked, not prodding in " << m_widgetName << ". Thread running: " << m_threadScope.isRunning();
        }
    }
}
void AbstractScopeWidget::prodBackgroundThread()
{
    if (this->visibleRegion().isEmpty()) {
//        qDebug() << "Scope " << m_widgetName << " is not visible. Not calculating background.";
    } else {
        if (m_semaphoreBackground.tryAcquire(1)) {
            Q_ASSERT(!m_threadBackground.isRunning());

            m_newBackgroundFrames.fetchAndStoreRelaxed(0);
            m_newBackgroundUpdates.fetchAndStoreRelaxed(0);
            m_threadBackground = QtConcurrent::run(this, &AbstractScopeWidget::renderBackground, m_accelFactorBackground);
//            qDebug() << "Background thread started in " << m_widgetName;

        } else {
//            qDebug() << "Background semaphore locked, not prodding in " << m_widgetName << ". Thread running: " << m_threadBackground.isRunning();
        }
    }
}

void AbstractScopeWidget::forceUpdate(bool doUpdate)
{
//    qDebug() << "Force update called in " << widgetName() << ". Arg: " << doUpdate;
    if (!doUpdate) { return; }
    m_newHUDUpdates.fetchAndAddRelaxed(1);
    m_newScopeUpdates.fetchAndAddRelaxed(1);
    m_newBackgroundUpdates.fetchAndAddRelaxed(1);
    prodHUDThread();
    prodScopeThread();
    prodBackgroundThread();
}
void AbstractScopeWidget::forceUpdateHUD()
{
    m_newHUDUpdates.fetchAndAddRelaxed(1);
    prodHUDThread();

}
void AbstractScopeWidget::forceUpdateScope()
{
    m_newScopeUpdates.fetchAndAddRelaxed(1);
    prodScopeThread();

}
void AbstractScopeWidget::forceUpdateBackground()
{
    m_newBackgroundUpdates.fetchAndAddRelaxed(1);
    prodBackgroundThread();

}


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

    forceUpdate();

    QWidget::resizeEvent(event);
}

void AbstractScopeWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_scopeRect = scopeRect();
}

void AbstractScopeWidget::paintEvent(QPaintEvent *)
{
    QPainter davinci(this);
    davinci.drawImage(m_scopeRect.topLeft(), m_imgBackground);
    davinci.drawImage(m_scopeRect.topLeft(), m_imgScope);
    davinci.drawImage(m_scopeRect.topLeft(), m_imgHUD);
}

void AbstractScopeWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
    m_mouseWithinWidget = true;
    emit signalMousePositionChanged();
}
void AbstractScopeWidget::leaveEvent(QEvent *)
{
    m_mouseWithinWidget = false;
    emit signalMousePositionChanged();
}

void AbstractScopeWidget::customContextMenuRequested(const QPoint &pos)
{
    m_menu->exec(this->mapToGlobal(pos));
}

uint AbstractScopeWidget::calculateAccelFactorHUD(uint oldMseconds, uint) { return ceil((float)oldMseconds*REALTIME_FPS/1000 ); }
uint AbstractScopeWidget::calculateAccelFactorScope(uint oldMseconds, uint) { return ceil((float)oldMseconds*REALTIME_FPS/1000 ); }
uint AbstractScopeWidget::calculateAccelFactorBackground(uint oldMseconds, uint) { return ceil((float)oldMseconds*REALTIME_FPS/1000 ); }


///// Slots /////

void AbstractScopeWidget::slotHUDRenderingFinished(uint mseconds, uint oldFactor)
{
//    qDebug() << "HUD rendering has finished, waiting for termination in " << m_widgetName;
    m_threadHUD.waitForFinished();
    m_imgHUD = m_threadHUD.result();

    m_semaphoreHUD.release(1);
    this->update();

    int accel;
    if (m_aRealtime->isChecked()) {
        accel = calculateAccelFactorHUD(mseconds, oldFactor);
        if (m_accelFactorHUD < 1) {
            accel = 1;
        }
        m_accelFactorHUD = accel;
    }

    if ( (m_newHUDFrames > 0 && m_aAutoRefresh->isChecked()) || m_newHUDUpdates > 0) {
//        qDebug() << "Trying to start a new HUD thread for " << m_widgetName
//                << ". New frames/updates: " << m_newHUDFrames << "/" << m_newHUDUpdates;
        prodHUDThread();;
    }
}

void AbstractScopeWidget::slotScopeRenderingFinished(uint mseconds, uint oldFactor)
{
    // The signal can be received before the thread has really finished. So we
    // need to wait until it has really finished before starting a new thread.
//    qDebug() << "Scope rendering has finished, waiting for termination in " << m_widgetName;
    m_threadScope.waitForFinished();
    m_imgScope = m_threadScope.result();

    // The scope thread has finished. Now we can release the semaphore, allowing a new thread.
    // See prodScopeThread where the semaphore is acquired again.
    m_semaphoreScope.release(1);
    this->update();

    // Calculate the acceleration factor hint to get «realtime» updates.
    int accel;
    if (m_aRealtime->isChecked()) {
        accel = calculateAccelFactorScope(mseconds, oldFactor);
        if (accel < 1) {
            // If mseconds happens to be 0.
            accel = 1;
        }
        // Don't directly calculate with m_accelFactorScope as we are dealing with concurrency.
        // If m_accelFactorScope is set to 0 at the wrong moment, who knows what might happen
        // then :) Therefore use a local variable.
        m_accelFactorScope = accel;
    }

    if ( (m_newScopeFrames > 0 && m_aAutoRefresh->isChecked()) || m_newScopeUpdates > 0) {
//        qDebug() << "Trying to start a new scope thread for " << m_widgetName
//                << ". New frames/updates: " << m_newScopeFrames << "/" << m_newScopeUpdates;
        prodScopeThread();
    }
}

void AbstractScopeWidget::slotBackgroundRenderingFinished(uint mseconds, uint oldFactor)
{
//    qDebug() << "Background rendering has finished, waiting for termination in " << m_widgetName;
    m_threadBackground.waitForFinished();
    m_imgBackground = m_threadBackground.result();

    m_semaphoreBackground.release(1);
    this->update();

    int accel;
    if (m_aRealtime->isChecked()) {
        accel = calculateAccelFactorBackground(mseconds, oldFactor);
        if (m_accelFactorBackground < 1) {
            accel = 1;
        }
        m_accelFactorBackground = accel;
    }

    if ( (m_newBackgroundFrames > 0 && m_aAutoRefresh->isChecked()) || m_newBackgroundUpdates > 0) {
//        qDebug() << "Trying to start a new background thread for " << m_widgetName
//                << ". New frames/updates: " << m_newBackgroundFrames << "/" << m_newBackgroundUpdates;
        prodBackgroundThread();;
    }
}

void AbstractScopeWidget::slotActiveMonitorChanged(bool isClipMonitor)
{
//    qDebug() << "Active monitor has changed in " << m_widgetName << ". Is the clip monitor active now? " << isClipMonitor;

    bool b = m_activeRender->disconnect(this);
    Q_ASSERT(b);

    m_activeRender = (isClipMonitor) ? m_clipMonitor->render : m_projMonitor->render;

    b &= connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    b &= connect(m_activeRender, SIGNAL(frameUpdated(QImage)), this, SLOT(slotRenderZoneUpdated(QImage)));
    Q_ASSERT(b);

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

//    qDebug() << "Monitor incoming. New frames total HUD/Scope/Background: " << m_newHUDFrames
//            << "/" << m_newScopeFrames << "/" << m_newBackgroundFrames;

    if (this->visibleRegion().isEmpty()) {
//        qDebug() << "Scope of widget " << m_widgetName << " is not at the top, not rendering.";
    } else {
        if (m_aAutoRefresh->isChecked()) {
            prodHUDThread();
            prodScopeThread();
            prodBackgroundThread();
        }
    }
}

void AbstractScopeWidget::slotRenderZoneUpdated(QImage frame)
{
    m_scopeImage = frame;
    slotRenderZoneUpdated();
}

void AbstractScopeWidget::slotResetRealtimeFactor(bool realtimeChecked)
{
    if (!realtimeChecked) {
        m_accelFactorHUD = 1;
        m_accelFactorScope = 1;
        m_accelFactorBackground = 1;
    }
}

void AbstractScopeWidget::slotAutoRefreshToggled(bool autoRefresh)
{
    // TODO only if depends on input
    if (autoRefresh) {
        forceUpdate();
    }
}
