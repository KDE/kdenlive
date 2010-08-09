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

  Note: Widgets deriving from this class should connect slotActiveMonitorChanged
  to the appropriate signal.

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

    ///// Unimplemented /////

    virtual QString widgetName() const = 0;

protected:
    ///// Variables /////

    Monitor *m_projMonitor;
    Monitor *m_clipMonitor;
    Render *m_activeRender;


    /** The context menu. Feel free to add new entries in your implementation. */
    QMenu *m_menu;

    /** Enables auto refreshing of the scope.
        This is when a new frame is shown on the active monitor.
        Resize events always force a recalculation. */
    QAction *m_aAutoRefresh;

    /** Realtime rendering. Should be disabled if it is not supported.
        Use the accelerationFactor variable passed to the render functions as a hint of
        how many times faster the scope should be calculated. */
    QAction *m_aRealtime;


    /** Offset from the widget's borders */
    const uchar offset;

    /** The rect on the widget we're painting in.
        Can be used by the implementing widget, e.g. in the render methods.
        Is updated when necessary (size changes). */
    QRect m_scopeRect;

    /** Images storing the calculated layers. Will be used on repaint events. */
    QImage m_imgHUD;
    QImage m_imgScope;
    QImage m_imgBackground;

    /** The acceleration factors can be accessed also by other renderer tasks,
        e.g. to display the scope's acceleration factor in the HUD renderer. */
    int m_accelFactorHUD;
    int m_accelFactorScope;
    int m_accelFactorBackground;


    ///// Unimplemented Methods /////

    /** Where on the widget we can paint in.
        May also update other variables that depend on the widget's size.  */
    virtual QRect scopeRect() = 0;

    /** @brief HUD renderer. Must emit signalHUDRenderingFinished(). @see renderScope */
    virtual QImage renderHUD(uint accelerationFactor) = 0;
    /** @brief Scope renderer. Must emit signalScopeRenderingFinished()
        when calculation has finished, to allow multi-threading.
        accelerationFactor hints how much faster than usual the calculation should be accomplished, if possible. */
    virtual QImage renderScope(uint accelerationFactor, QImage) = 0;
    /** @brief Background renderer. Must emit signalBackgroundRenderingFinished(). @see renderScope */
    virtual QImage renderBackground(uint accelerationFactor) = 0;

    /** Must return true if the HUD layer depends on the input monitor.
        If it does not, then it does not need to be re-calculated when
        a new frame from the monitor is incoming. */
    virtual bool isHUDDependingOnInput() const = 0;
    /** @see isHUDDependingOnInput() */
    virtual bool isScopeDependingOnInput() const = 0;
    /** @see isHUDDependingOnInput() */
    virtual bool isBackgroundDependingOnInput() const = 0;

    ///// Can be reimplemented /////
    /** Calculates the acceleration factor to be used by the render thread.
        This method can be refined in the subclass if required. */
    virtual uint calculateAccelFactorHUD(uint oldMseconds, uint oldFactor);
    virtual uint calculateAccelFactorScope(uint oldMseconds, uint oldFactor);
    virtual uint calculateAccelFactorBackground(uint oldMseconds, uint oldFactor);

    ///// Reimplemented /////

    void mouseReleaseEvent(QMouseEvent *);
    void paintEvent(QPaintEvent *);
    void raise();
    void resizeEvent(QResizeEvent *);
    void showEvent(QShowEvent *);

protected slots:
    /** Forces an update of all layers. */
    void forceUpdate();
    void forceUpdateHUD();
    void forceUpdateScope();
    void forceUpdateBackground();
    void slotAutoRefreshToggled(bool);

signals:
    /** mseconds represent the time taken for the calculation,
        accelerationFactor the acceleration factor that has been used. */
    void signalHUDRenderingFinished(uint mseconds, uint accelerationFactor);
    void signalScopeRenderingFinished(uint mseconds, uint accelerationFactor);
    void signalBackgroundRenderingFinished(uint mseconds, uint accelerationFactor);

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

    /** The semaphores ensure that the QFutures for the HUD/Scope/Background threads cannot
      be assigned a new thread while it is still running. (Could cause deadlocks and other
      nasty things known from parallelism. */
    QSemaphore m_semaphoreHUD;
    QSemaphore m_semaphoreScope;
    QSemaphore m_semaphoreBackground;

    QFuture<QImage> m_threadHUD;
    QFuture<QImage> m_threadScope;
    QFuture<QImage> m_threadBackground;

    QImage m_scopeImage;

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
    void slotRenderZoneUpdated(QImage);
    void slotHUDRenderingFinished(uint mseconds, uint accelerationFactor);
    void slotScopeRenderingFinished(uint mseconds, uint accelerationFactor);
    void slotBackgroundRenderingFinished(uint mseconds, uint accelerationFactor);

    /** Resets the acceleration factors to 1 when realtime rendering is disabled. */
    void slotResetRealtimeFactor(bool realtimeChecked);

};

#endif // ABSTRACTSCOPEWIDGET_H
