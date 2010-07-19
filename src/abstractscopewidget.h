/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/**
     _____________________
    /                     \
   /      HUD Layer        \
  /                         \
  ---------------------------
     _____________________
    /                     \
   /     Scope Layer       \
  /                         \
  ---------------------------
     _____________________
    /                     \
   /   Background Layer    \
  /                         \
  ---------------------------
 */

#ifndef ABSTRACTSCOPEWIDGET_H
#define ABSTRACTSCOPEWIDGET_H

#include <QtCore>
#include <QWidget>

class QMenu;

class Monitor;
class Render;

class AbstractScopeWidget : public QWidget
{
    Q_OBJECT

public:
    AbstractScopeWidget(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    virtual ~AbstractScopeWidget(); // Must be virtual because of inheritance, to avoid memory leaks
    QPalette m_scopePalette;

    virtual QString widgetName() const = 0;

protected:
    ///// Variables /////

    Monitor *m_projMonitor;
    Monitor *m_clipMonitor;
    Render *m_activeRender;

    QMenu *m_menu;
    QAction *m_aAutoRefresh;
    QAction *m_aRealtime;

    /** Offset from the widget's borders */
    const uchar offset;

    QRect m_scopeRect;
    QImage m_imgHUD;
    QImage m_imgScope;
    QImage m_imgBackground;

    /** Counts the number of frames that have been rendered in the active monitor.
      The frame number will be reset when the calculation starts for the current frame. */
    QAtomicInt m_newHUDFrames;
    QAtomicInt m_newScopeFrames;
    QAtomicInt m_newBackgroundFrames;

    /** Counts the number of updates that, unlike new frames, force a recalculation
      of the scope, like for example a resize event. */
    QAtomicInt m_newHUDUpdates;
    QAtomicInt m_newScopeUpdates;
    QAtomicInt m_newBackgroundUpdates;

    QFuture<QImage> m_threadHUD;
    QFuture<QImage> m_threadScope;
    QFuture<QImage> m_threadBackground;

    QSemaphore m_semaphoreHUD;
    QSemaphore m_semaphoreScope;
    QSemaphore m_semaphoreBackground;


    ///// Unimplemented Methods /////

    /** Where on the widget we can paint in */
    virtual QRect scopeRect() = 0;

    /** HUD renderer. Must emit signalHUDRenderingFinished(). */
    virtual QImage renderHUD() = 0;
    /** Scope renderer. Must emit signalScopeRenderingFinished(). */
    virtual QImage renderScope() = 0;
    /** Background renderer. Must emit signalBackgroundRenderingFinished(). */
    virtual QImage renderBackground() = 0;

    ///// Methods /////

    void mouseReleaseEvent(QMouseEvent *);
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);


protected slots:
    /** Called when the active monitor has shown a new frame. */
    void slotRenderZoneUpdated();
    void slotHUDRenderingFinished();
    void slotScopeRenderingFinished();
    void slotBackgroundRenderingFinished();

signals:
    void signalHUDRenderingFinished();
    void signalScopeRenderingFinished();
    void signalBackgroundRenderingFinished();

private:
    void prodScopeThread();

private slots:
    void customContextMenuRequested(const QPoint &pos);
    void slotActiveMonitorChanged(bool isClipMonitor);

};

#endif // ABSTRACTSCOPEWIDGET_H
