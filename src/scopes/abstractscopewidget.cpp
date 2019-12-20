/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "abstractscopewidget.h"

#include "monitor/monitor.h"

#include <QColor>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QtConcurrent>

#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <cmath>
// Uncomment for Scope debugging.
//#define DEBUG_ASW

#ifdef DEBUG_ASW
#include "kdenlive_debug.h"
#endif

const int REALTIME_FPS = 30;

const QColor light(250, 238, 226, 255);
const QColor dark(40, 40, 39, 255);
const QColor dark2(25, 25, 23, 255);
const QColor AbstractScopeWidget::colHighlightLight(18, 130, 255, 255);
const QColor AbstractScopeWidget::colHighlightDark(255, 64, 19, 255);
const QColor AbstractScopeWidget::colDarkWhite(250, 250, 250);

const QPen AbstractScopeWidget::penThick(QBrush(AbstractScopeWidget::colDarkWhite.rgb()), 2, Qt::SolidLine);
const QPen AbstractScopeWidget::penThin(QBrush(AbstractScopeWidget::colDarkWhite.rgb()), 1, Qt::SolidLine);
const QPen AbstractScopeWidget::penLight(QBrush(QColor(200, 200, 250, 150)), 1, Qt::SolidLine);
const QPen AbstractScopeWidget::penLightDots(QBrush(QColor(200, 200, 250, 150)), 1, Qt::DotLine);
const QPen AbstractScopeWidget::penLighter(QBrush(QColor(225, 225, 250, 225)), 1, Qt::SolidLine);
const QPen AbstractScopeWidget::penDark(QBrush(QColor(0, 0, 20, 250)), 1, Qt::SolidLine);
const QPen AbstractScopeWidget::penDarkDots(QBrush(QColor(0, 0, 20, 250)), 1, Qt::DotLine);
const QPen AbstractScopeWidget::penBackground(QBrush(dark2), 1, Qt::SolidLine);

const QString AbstractScopeWidget::directions[] = {QStringLiteral("North"), QStringLiteral("Northeast"), QStringLiteral("East"), QStringLiteral("Southeast")};

AbstractScopeWidget::AbstractScopeWidget(bool trackMouse, QWidget *parent)
    : QWidget(parent)
    , m_mousePos(0, 0)
    , m_semaphoreHUD(1)
    , m_semaphoreScope(1)
    , m_semaphoreBackground(1)

{
    m_scopePalette = QPalette();
    m_scopePalette.setBrush(QPalette::Window, QBrush(dark2));
    m_scopePalette.setBrush(QPalette::Base, QBrush(dark));
    m_scopePalette.setBrush(QPalette::Button, QBrush(dark));
    m_scopePalette.setBrush(QPalette::Text, QBrush(light));
    m_scopePalette.setBrush(QPalette::WindowText, QBrush(light));
    m_scopePalette.setBrush(QPalette::ButtonText, QBrush(light));
    setPalette(m_scopePalette);
    setAutoFillBackground(true);

    m_aAutoRefresh = new QAction(i18n("Auto Refresh"), this);
    m_aAutoRefresh->setCheckable(true);
    m_aRealtime = new QAction(i18n("Realtime (with precision loss)"), this);
    m_aRealtime->setCheckable(true);

    m_menu = new QMenu();
    // Disabled dark palette on menus since it breaks up with some themes: kdenlive issue #2950
    // m_menu->setPalette(m_scopePalette);
    m_menu->addAction(m_aAutoRefresh);
    m_menu->addAction(m_aRealtime);

    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &AbstractScopeWidget::customContextMenuRequested, this, &AbstractScopeWidget::slotContextMenuRequested);

    connect(this, &AbstractScopeWidget::signalHUDRenderingFinished, this, &AbstractScopeWidget::slotHUDRenderingFinished);
    connect(this, &AbstractScopeWidget::signalScopeRenderingFinished, this, &AbstractScopeWidget::slotScopeRenderingFinished);
    connect(this, &AbstractScopeWidget::signalBackgroundRenderingFinished, this, &AbstractScopeWidget::slotBackgroundRenderingFinished);
    connect(m_aRealtime, &QAction::toggled, this, &AbstractScopeWidget::slotResetRealtimeFactor);
    connect(m_aAutoRefresh, &QAction::toggled, this, &AbstractScopeWidget::slotAutoRefreshToggled);

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
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    m_aAutoRefresh->setChecked(scopeConfig.readEntry("autoRefresh", true));
    m_aRealtime->setChecked(scopeConfig.readEntry("realtime", false));
    scopeConfig.sync();
}

void AbstractScopeWidget::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("autoRefresh", m_aAutoRefresh->isChecked());
    scopeConfig.writeEntry("realtime", m_aRealtime->isChecked());
    scopeConfig.sync();
}

QString AbstractScopeWidget::configName()
{
    return "Scope_" + m_widgetName;
}

void AbstractScopeWidget::prodHUDThread()
{
    if (this->visibleRegion().isEmpty()) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Scope " << m_widgetName << " is not visible. Not calculating HUD.";
#endif
    } else {
        if (m_semaphoreHUD.tryAcquire(1)) {
            Q_ASSERT(!m_threadHUD.isRunning());

            m_newHUDFrames.fetchAndStoreRelaxed(0);
            m_newHUDUpdates.fetchAndStoreRelaxed(0);
            m_threadHUD = QtConcurrent::run(this, &AbstractScopeWidget::renderHUD, m_accelFactorHUD);
#ifdef DEBUG_ASW
            qCDebug(KDENLIVE_LOG) << "HUD thread started in " << m_widgetName;
#endif

        }
#ifdef DEBUG_ASW
        else {
            qCDebug(KDENLIVE_LOG) << "HUD semaphore locked, not prodding in " << m_widgetName << ". Thread running: " << m_threadHUD.isRunning();
        }
#endif
    }
}

void AbstractScopeWidget::prodScopeThread()
{
    // Only start a new thread if the scope is actually visible
    // and not hidden by another widget on the stack and if user want the scope to update.
    if (this->visibleRegion().isEmpty() || (!m_aAutoRefresh->isChecked() && !m_requestForcedUpdate)) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Scope " << m_widgetName << " is not visible. Not calculating scope.";
#endif
    } else {
        // Try to acquire the semaphore. This must only succeed if m_threadScope is not running
        // anymore. Therefore the semaphore must NOT be released before m_threadScope ends.
        // If acquiring the semaphore fails, the thread is still running.
        if (m_semaphoreScope.tryAcquire(1)) {
            Q_ASSERT(!m_threadScope.isRunning());

            m_newScopeFrames.fetchAndStoreRelaxed(0);
            m_newScopeUpdates.fetchAndStoreRelaxed(0);

            Q_ASSERT(m_accelFactorScope > 0);

            // See https://doc.qt.io/qt-5/qtconcurrentrun.html about
            // running member functions in a thread
            m_threadScope = QtConcurrent::run(this, &AbstractScopeWidget::renderScope, m_accelFactorScope);
            m_requestForcedUpdate = false;

#ifdef DEBUG_ASW
            qCDebug(KDENLIVE_LOG) << "Scope thread started in " << m_widgetName;
#endif

        } else {
#ifdef DEBUG_ASW
            qCDebug(KDENLIVE_LOG) << "Scope semaphore locked, not prodding in " << m_widgetName << ". Thread running: " << m_threadScope.isRunning();
#endif
        }
    }
}
void AbstractScopeWidget::prodBackgroundThread()
{
    if (this->visibleRegion().isEmpty()) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Scope " << m_widgetName << " is not visible. Not calculating background.";
#endif
    } else {
        if (m_semaphoreBackground.tryAcquire(1)) {
            Q_ASSERT(!m_threadBackground.isRunning());

            m_newBackgroundFrames.fetchAndStoreRelaxed(0);
            m_newBackgroundUpdates.fetchAndStoreRelaxed(0);
            m_threadBackground = QtConcurrent::run(this, &AbstractScopeWidget::renderBackground, m_accelFactorBackground);

#ifdef DEBUG_ASW
            qCDebug(KDENLIVE_LOG) << "Background thread started in " << m_widgetName;
#endif

        } else {
#ifdef DEBUG_ASW
            qCDebug(KDENLIVE_LOG) << "Background semaphore locked, not prodding in " << m_widgetName << ". Thread running: " << m_threadBackground.isRunning();
#endif
        }
    }
}

void AbstractScopeWidget::forceUpdate(bool doUpdate)
{
#ifdef DEBUG_ASW
    qCDebug(KDENLIVE_LOG) << "Forced update called in " << widgetName() << ". Arg: " << doUpdate;
#endif

    if (!doUpdate) {
        return;
    }
    m_requestForcedUpdate = true;
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
    m_requestForcedUpdate = true;
    prodScopeThread();
}
void AbstractScopeWidget::forceUpdateBackground()
{
    m_newBackgroundUpdates.fetchAndAddRelaxed(1);
    prodBackgroundThread();
}

///// Events /////

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

void AbstractScopeWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Rescaling mode starts
        m_rescaleActive = true;
        m_rescalePropertiesLocked = false;
        m_rescaleFirstRescaleDone = false;
        m_rescaleStartPoint = event->pos();
        m_rescaleModifiers = event->modifiers();
    }
}
void AbstractScopeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_rescaleActive = false;
    m_rescalePropertiesLocked = false;

    if (!m_aAutoRefresh->isChecked()) {
        m_requestForcedUpdate = true;
    }
    prodHUDThread();
    prodScopeThread();
    prodBackgroundThread();
    QWidget::mouseReleaseEvent(event);
}
void AbstractScopeWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->pos();
    m_mouseWithinWidget = true;
    emit signalMousePositionChanged();

    QPoint movement = event->pos() - m_rescaleStartPoint;

    if (m_rescaleActive) {
        if (m_rescalePropertiesLocked) {
            // Direction is known, now adjust parameters

            // Reset the starting point to make the next moveEvent relative to the current one
            m_rescaleStartPoint = event->pos();

            if (!m_rescaleFirstRescaleDone) {
                // We have just learned the desired direction; Normalize the movement to one pixel
                // to avoid a jump by m_rescaleMinDist

                if (movement.x() != 0) {
                    movement.setX(movement.x() / abs(movement.x()));
                }
                if (movement.y() != 0) {
                    movement.setY(movement.y() / abs(movement.y()));
                }

                m_rescaleFirstRescaleDone = true;
            }

            handleMouseDrag(movement, m_rescaleDirection, m_rescaleModifiers);

        } else {
            // Detect the movement direction here.
            // This algorithm relies on the aspect ratio of dy/dx (size and signum).
            if (movement.manhattanLength() > m_rescaleMinDist) {
                float diff = ((float)movement.y()) / (float)movement.x();

                if (std::fabs(diff) > m_rescaleVerticalThreshold || movement.x() == 0) {
                    m_rescaleDirection = North;
                } else if (std::fabs(diff) < 1 / m_rescaleVerticalThreshold) {
                    m_rescaleDirection = East;
                } else if (diff < 0) {
                    m_rescaleDirection = Northeast;
                } else {
                    m_rescaleDirection = Southeast;
                }
#ifdef DEBUG_ASW
                qCDebug(KDENLIVE_LOG) << "Diff is " << diff << "; chose " << directions[m_rescaleDirection] << " as direction";
#endif
                m_rescalePropertiesLocked = true;
            }
        }
    }
}

void AbstractScopeWidget::leaveEvent(QEvent *)
{
    m_mouseWithinWidget = false;
    emit signalMousePositionChanged();
}

void AbstractScopeWidget::slotContextMenuRequested(const QPoint &pos)
{
    m_menu->exec(this->mapToGlobal(pos));
}

uint AbstractScopeWidget::calculateAccelFactorHUD(uint oldMseconds, uint)
{
    return std::ceil((float)oldMseconds * REALTIME_FPS / 1000);
}
uint AbstractScopeWidget::calculateAccelFactorScope(uint oldMseconds, uint)
{
    return std::ceil((float)oldMseconds * REALTIME_FPS / 1000);
}
uint AbstractScopeWidget::calculateAccelFactorBackground(uint oldMseconds, uint)
{
    return std::ceil((float)oldMseconds * REALTIME_FPS / 1000);
}

///// Slots /////

void AbstractScopeWidget::slotHUDRenderingFinished(uint mseconds, uint oldFactor)
{
#ifdef DEBUG_ASW
    qCDebug(KDENLIVE_LOG) << "HUD rendering has finished in " << mseconds << " ms, waiting for termination in " << m_widgetName;
#endif
    m_threadHUD.waitForFinished();
    m_imgHUD = m_threadHUD.result();

    m_semaphoreHUD.release(1);
    this->update();

    if (m_aRealtime->isChecked()) {
        int accel;
        accel = (int)calculateAccelFactorHUD(mseconds, oldFactor);
        if (m_accelFactorHUD < 1) {
            accel = 1;
        }
        m_accelFactorHUD = accel;
    }

    if ((m_newHUDFrames > 0 && m_aAutoRefresh->isChecked()) || m_newHUDUpdates > 0) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Trying to start a new HUD thread for " << m_widgetName << ". New frames/updates: " << m_newHUDFrames << '/'
                              << m_newHUDUpdates;
#endif
        prodHUDThread();
    }
}

void AbstractScopeWidget::slotScopeRenderingFinished(uint mseconds, uint oldFactor)
{
// The signal can be received before the thread has really finished. So we
// need to wait until it has really finished before starting a new thread.
#ifdef DEBUG_ASW
    qCDebug(KDENLIVE_LOG) << "Scope rendering has finished in " << mseconds << " ms, waiting for termination in " << m_widgetName;
#endif
    m_threadScope.waitForFinished();
    m_imgScope = m_threadScope.result();

    // The scope thread has finished. Now we can release the semaphore, allowing a new thread.
    // See prodScopeThread where the semaphore is acquired again.
    m_semaphoreScope.release(1);
    this->update();

    // Calculate the acceleration factor hint to get «realtime» updates.
    if (m_aRealtime->isChecked()) {
        int accel;
        accel = (int)calculateAccelFactorScope(mseconds, oldFactor);
        if (accel < 1) {
            // If mseconds happens to be 0.
            accel = 1;
        }
        // Don't directly calculate with m_accelFactorScope as we are dealing with concurrency.
        // If m_accelFactorScope is set to 0 at the wrong moment, who knows what might happen
        // then :) Therefore use a local variable.
        m_accelFactorScope = accel;
    }

    if ((m_newScopeFrames > 0 && m_aAutoRefresh->isChecked()) || m_newScopeUpdates > 0) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Trying to start a new scope thread for " << m_widgetName << ". New frames/updates: " << m_newScopeFrames << '/'
                              << m_newScopeUpdates;
#endif
        prodScopeThread();
    }
}

void AbstractScopeWidget::slotBackgroundRenderingFinished(uint mseconds, uint oldFactor)
{
#ifdef DEBUG_ASW
    qCDebug(KDENLIVE_LOG) << "Background rendering has finished in " << mseconds << " ms, waiting for termination in " << m_widgetName;
#endif
    m_threadBackground.waitForFinished();
    m_imgBackground = m_threadBackground.result();

    m_semaphoreBackground.release(1);
    this->update();

    if (m_aRealtime->isChecked()) {
        int accel;
        accel = (int)calculateAccelFactorBackground(mseconds, oldFactor);
        if (m_accelFactorBackground < 1) {
            accel = 1;
        }
        m_accelFactorBackground = accel;
    }

    if ((m_newBackgroundFrames > 0 && m_aAutoRefresh->isChecked()) || m_newBackgroundUpdates > 0) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Trying to start a new background thread for " << m_widgetName << ". New frames/updates: " << m_newBackgroundFrames << '/'
                              << m_newBackgroundUpdates;
#endif
        prodBackgroundThread();
    }
}

void AbstractScopeWidget::slotRenderZoneUpdated()
{
    m_newHUDFrames.fetchAndAddRelaxed(1);
    m_newScopeFrames.fetchAndAddRelaxed(1);
    m_newBackgroundFrames.fetchAndAddRelaxed(1);

#ifdef DEBUG_ASW
    qCDebug(KDENLIVE_LOG) << "Data incoming at " << widgetName() << ". New frames total HUD/Scope/Background: " << m_newHUDFrames << '/' << m_newScopeFrames
                          << '/' << m_newBackgroundFrames;
#endif

    if (this->visibleRegion().isEmpty()) {
#ifdef DEBUG_ASW
        qCDebug(KDENLIVE_LOG) << "Scope of widget " << m_widgetName << " is not at the top, not rendering.";
#endif
    } else {
        if (m_aAutoRefresh->isChecked()) {
            prodHUDThread();
            prodScopeThread();
            prodBackgroundThread();
        }
    }
}

void AbstractScopeWidget::slotResetRealtimeFactor(bool realtimeChecked)
{
    if (!realtimeChecked) {
        m_accelFactorHUD = 1;
        m_accelFactorScope = 1;
        m_accelFactorBackground = 1;
    }
}

bool AbstractScopeWidget::autoRefreshEnabled() const
{
    return m_aAutoRefresh->isChecked();
}

void AbstractScopeWidget::slotAutoRefreshToggled(bool autoRefresh)
{
#ifdef DEBUG_ASW
    qCDebug(KDENLIVE_LOG) << "Auto-refresh switched to " << autoRefresh << " in " << widgetName() << " (Visible: " << isVisible() << '/'
                          << this->visibleRegion().isEmpty() << ')';
#endif
    if (isVisible()) {
        // Notify listeners whether we accept new frames now
        emit requestAutoRefresh(autoRefresh);
    }
    // TODO only if depends on input
    if (autoRefresh) {
        // forceUpdate();
        m_requestForcedUpdate = true;
    }
}

void AbstractScopeWidget::handleMouseDrag(const QPoint &, const RescaleDirection, const Qt::KeyboardModifiers) {}

#ifdef DEBUG_ASW
#undef DEBUG_ASW
#endif
