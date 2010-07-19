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
  This abstract widget is a proof that abstract things sometimes *are* useful.

  The widget expects three layers which
  * Will be painted on top of each other on each update
  * Are rendered in a separate thread so that the UI is not blocked
  * Are rendered only if necessary (e.g., if a layer does not depend
    on input images, it will not be re-rendered for incoming frames)

  The layer order is as follows:
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

  Colors of Scope Widgets are defined in here (and thus don't need to be
  re-defined in the implementation of the widget's .ui file).

  The custom context menu already contains entries, like for enabling auto-
  refresh. It can certainly be extended in the implementation of the widget.

  If you intend to write an own widget inheriting from this one, please read
  the comments on the unimplemented methods carefully. They are not only here
  for optical amusement, but also contain important information.
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

    /** The context menu. Feel free to add new entries in your implementation. */
    QMenu *m_menu;
    QAction *m_aAutoRefresh;
    QAction *m_aRealtime;

    /** Offset from the widget's borders */
    const uchar offset;

    QRect m_scopeRect;
    QImage m_imgHUD;
    QImage m_imgScope;
    QImage m_imgBackground;


    ///// Unimplemented Methods /////

    /** Where on the widget we can paint in */
    virtual QRect scopeRect() = 0;

    /** @brief HUD renderer.
      Must emit signalHUDRenderingFinished().
      Should */
    virtual QImage renderHUD() = 0;
    /** @brief Scope renderer. Must emit signalScopeRenderingFinished(). */
    virtual QImage renderScope() = 0;
    /** @brief Background renderer. Must emit signalBackgroundRenderingFinished(). */
    virtual QImage renderBackground() = 0;

    /** Must return true if the HUD layer depends on the input monitor.
      If it does not, then it does not need to be re-calculated when
      a new frame from the monitor is incoming. */
    virtual bool isHUDDependingOnInput() const = 0;
    /** @see isHUDDependingOnInput() */
    virtual bool isScopeDependingOnInput() const = 0;
    /** @see isHUDDependingOnInput() */
    virtual bool isBackgroundDependingOnInput() const = 0;

    ///// Methods /////

    void mouseReleaseEvent(QMouseEvent *);
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);

signals:
    void signalHUDRenderingFinished(uint mseconds);
    void signalScopeRenderingFinished(uint mseconds);
    void signalBackgroundRenderingFinished(uint mseconds);

private:

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

    QSemaphore m_semaphoreHUD;
    QSemaphore m_semaphoreScope;
    QSemaphore m_semaphoreBackground;

    QFuture<QImage> m_threadHUD;
    QFuture<QImage> m_threadScope;
    QFuture<QImage> m_threadBackground;

    bool initialDimensionUpdateDone;
    void prodHUDThread();
    void prodScopeThread();
    void prodBackgroundThread();

private slots:
    /** @brief Must be called when the active monitor has shown a new frame.
      This slot must be connected in the implementing class, it is *not*
      done in this abstract class. */
    void slotActiveMonitorChanged(bool isClipMonitor);
    void customContextMenuRequested(const QPoint &pos);
    void slotRenderZoneUpdated();
    void slotHUDRenderingFinished(uint mseconds);
    void slotScopeRenderingFinished(uint mseconds);
    void slotBackgroundRenderingFinished(uint mseconds);

};

#endif // ABSTRACTSCOPEWIDGET_H
