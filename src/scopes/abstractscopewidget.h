/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ABSTRACTSCOPEWIDGET_H
#define ABSTRACTSCOPEWIDGET_H

#include <QFuture>
#include <QMenu>
#include <QSemaphore>
#include <QWidget>
/**
  \brief Abstract class for audio/colour scopes (receive data and paint it).

  This abstract widget is a proof that abstract things sometimes \b *are* useful.

  The widget expects three layers which
  \li Will be painted on top of each other on each update
  \li Are rendered in a separate thread so that the UI is not blocked
  \li Are rendered only if necessary (e.g., if a layer does not depend
    on input images, it will not be re-rendered for incoming frames)

  The layer order is as follows:
  \verbatim
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
  \endverbatim

  Colors of Scope Widgets are defined in here (and thus don't need to be
  re-defined in the implementation of the widget's .ui file).

  The custom context menu already contains entries, like for enabling auto-
  refresh. It can certainly be extended in the implementation of the widget.

  If you intend to write an own widget inheriting from this one, please read
  the comments on the unimplemented methods carefully. They are not only here
  for optical amusement, but also contain important information.
 */
class AbstractScopeWidget : public QWidget
{
    Q_OBJECT

public:
    /**
      \param trackMouse enables mouse tracking; The variables m_mousePos and m_mouseWithinWidget will be set
            if mouse tracking is enabled.
      \see signalMousePositionChanged(): Emitted when mouse tracking is enabled
      */
    explicit AbstractScopeWidget(bool trackMouse = false, QWidget *parent = nullptr);
    ~AbstractScopeWidget() override; // Must be virtual because of inheritance, to avoid memory leaks

    enum RescaleDirection { North, Northeast, East, Southeast };

    QPalette m_scopePalette;

    /** Initializes widget settings (reads configuration).
      Has to be called in the implementing object. */
    virtual void init();

    /** Tell whether this scope has auto-refresh enabled. Required for determining whether
        new data (e.g. an image frame) has to be delivered to this widget. */
    bool autoRefreshEnabled() const;

    bool needsSingleFrame();

    ///// Unimplemented /////

    virtual QString widgetName() const = 0;

    ///// Variables /////
    static const QColor colHighlightLight;
    static const QColor colHighlightDark;
    static const QColor colDarkWhite;

    static const QPen penThick;
    static const QPen penThin;
    static const QPen penLight;
    static const QPen penLightDots;
    static const QPen penLighter;
    static const QPen penDark;
    static const QPen penDarkDots;
    static const QPen penBackground;

    static const QString directions[]; // Mainly for debug output

protected:
    ///// Variables /////

    /** The context menu. Feel free to add new entries in your implementation. */
    QMenu *m_menu;

    /** Enables auto refreshing of the scope.
        This is when fresh data is incoming.
        Resize events always force a recalculation. */
    QAction *m_aAutoRefresh;

    /** Realtime rendering. Should be disabled if it is not supported.
        Use the accelerationFactor variable passed to the render functions as a hint of
        how many times faster the scope should be calculated. */
    QAction *m_aRealtime;

    /** The mouse position; Updated when the mouse enters the widget
        AND mouse tracking has been enabled. */
    QPoint m_mousePos;
    /** Knows whether the mouse currently lies within the widget or not.
        Can e.g. be used for drawing a HUD only when the mouse is in the widget. */
    bool m_mouseWithinWidget{false};

    /** Offset from the widget's borders */
    const uchar offset{5};

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
    int m_accelFactorHUD{1};
    int m_accelFactorScope{1};
    int m_accelFactorBackground{1};

    /** Reads the widget's configuration.
        Can be extended in the implementing subclass (make sure to run readConfig as well). */
    virtual void readConfig();
    /** Writes the widget configuration.
        Implementing widgets have to implement an own method and run it in their destructor. */
    void writeConfig();
    /** Identifier for the widget's configuration. */
    QString configName();

    ///// Unimplemented Methods /////

    /** Where on the widget we can paint in.
        May also update other variables, like m_scopeRect or custom ones,
        that have to change together with the widget's size.  */
    virtual QRect scopeRect() = 0;

    /** @brief HUD renderer. Must emit signalHUDRenderingFinished().
        @see renderScope(uint). */
    virtual QImage renderHUD(uint accelerationFactor) = 0;
    /** @brief Rendering function for the scope layer.
        This function \b must emit signalScopeRenderingFinished(), otherwise the scope
        will not attempt to ever call this function again. This signal is required for multi-threading;
        not emitting it on unused rendering function may increase performance.
        @param accelerationFactor hints how much faster than usual the calculation should be accomplished, if possible.
        @see renderHUD(uint) for the upper layer
        @see renderBackground(uint) for the layer below
         */
    virtual QImage renderScope(uint accelerationFactor) = 0;
    /** @brief Background renderer. Must emit signalBackgroundRenderingFinished().
        @see renderScope(uint) */
    virtual QImage renderBackground(uint accelerationFactor) = 0;

    /** Must return true if the HUD layer depends on the input data.
        If it does not, then it does not need to be re-calculated when
        fresh data is incoming. */
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

    /** The Abstract Scope will try to detect the movement direction when dragging on the widget with the mouse.
        As soon as the direction is determined it will execute this method. Can be used e.g. for re-scaling content.
        This is just a dummy function, re-implement to add functionality. */
    virtual void handleMouseDrag(const QPoint &movement, const RescaleDirection rescaleDirection, const Qt::KeyboardModifiers rescaleModifiers);

    ///// Reimplemented /////

    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override; // Called when the widget is activated via the Menu entry
    //    void raise(); // Called only when  manually calling the event -> useless

public slots:
    /** Forces an update of all layers. */
    void forceUpdate(bool doUpdate = true);
    void forceUpdateHUD();
    void forceUpdateScope();
    void forceUpdateBackground();

protected slots:
    void slotAutoRefreshToggled(bool);

signals:
    /**
      \param mseconds represents the time taken for the calculation.
      \param accelerationFactor is the acceleration factor that has been used for this calculation.
      */
    void signalHUDRenderingFinished(uint mseconds, uint accelerationFactor);
    void signalScopeRenderingFinished(uint mseconds, uint accelerationFactor);
    void signalBackgroundRenderingFinished(uint mseconds, uint accelerationFactor);

    /** For the mouse position itself see m_mousePos.
        To check whether the mouse has leaved the widget, see m_mouseWithinWidget.
        This signal is typically connected to forceUpdateHUD(). */
    void signalMousePositionChanged();

    /** Do we need the renderer to send its frames to us?
        Emitted when auto-refresh is toggled. */
    void requestAutoRefresh(bool);

private:
    /** Counts the number of data frames that have been rendered in the active monitor.
        The frame number will be reset when the calculation starts for the current data set. */
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
        nasty things known from parallelism.) */
    QSemaphore m_semaphoreHUD;
    QSemaphore m_semaphoreScope;
    QSemaphore m_semaphoreBackground;

    QFuture<QImage> m_threadHUD;
    QFuture<QImage> m_threadScope;
    QFuture<QImage> m_threadBackground;

    bool m_initialDimensionUpdateDone{false};
    bool m_requestForcedUpdate{false};

    QImage m_scopeImage;

    QString m_widgetName;

    void prodHUDThread();
    void prodScopeThread();
    void prodBackgroundThread();

    ///// Movement detection /////
    const int m_rescaleMinDist{4};
    const float m_rescaleVerticalThreshold{2.0f};

    bool m_rescaleActive{false};
    bool m_rescalePropertiesLocked{false};
    bool m_rescaleFirstRescaleDone{true};
    Qt::KeyboardModifiers m_rescaleModifiers;
    RescaleDirection m_rescaleDirection{North};
    QPoint m_rescaleStartPoint;

protected slots:
    void slotContextMenuRequested(const QPoint &pos);
    /** To be called when a new frame has been received.
        The scope then decides whether and when it wants to recalculate the scope, depending
        on whether it is currently visible and whether a calculation thread is already running. */
    void slotRenderZoneUpdated();
    /** The following slots are called when rendering of a component has finished. They e.g. update
        the widget and decide whether to immediately restart the calculation thread. */
    void slotHUDRenderingFinished(uint mseconds, uint accelerationFactor);
    void slotScopeRenderingFinished(uint mseconds, uint accelerationFactor);
    void slotBackgroundRenderingFinished(uint mseconds, uint accelerationFactor);

    /** Resets the acceleration factors to 1 when realtime rendering is disabled. */
    void slotResetRealtimeFactor(bool realtimeChecked);
};

#endif // ABSTRACTSCOPEWIDGET_H
