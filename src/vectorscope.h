/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef VECTORSCOPE_H
#define VECTORSCOPE_H

#include <QtCore>
#include "renderer.h"
#include "monitor.h"
#include "ui_vectorscope_ui.h"

class Render;
class Monitor;
class Vectorscope_UI;

enum PAINT_MODE { GREEN = 0, ORIG = 1, CHROMA = 2 };

class Vectorscope : public QWidget, public Ui::Vectorscope_UI {
    Q_OBJECT

public:
    Vectorscope(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    ~Vectorscope();

protected:
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *event);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *event);

private:
    Monitor *m_projMonitor;
    Monitor *m_clipMonitor;
    Render *m_activeRender;

    /** How to represent the pixels on the scope (green, original color, ...) */
    int iPaintMode;

    float scaling;


    QPoint mapToCanvas(QRect inside, QPointF point);

    bool circleEnabled;
    QPoint mousePos;

    /** Updates the dimension. Only necessary when the widget has been resized. */
    void updateDimensions();
    bool initialDimensionUpdateDone;
    QRect m_scopeRect;
    int cw;


    /** The vectorscope color distribution.
        Class variable as calculated by a thread. */
    QImage m_scope;

    void calculateScope();
    QFuture<void> m_scopeCalcThread;

    /** Prods the Scope calculation thread. If it is running, do nothing. If it is not,
      run a new thread.
      Returns true if a new thread has been started. */
    bool prodCalcThread();

    /** Counts the number of frames that have been rendered in one of the monitors.
      The frame number will be reset when the vectorscope starts calculating the
      current frame. */
    QAtomicInt newFrames;
    /** Counts the number of other changes that should cause the vectorscope to be
      recalculated. This is for example a resizeEvent. In this case, no new frames
      are generated, but the scope has to be updated in any case (also if auto-update
      is not enabled). */
    QAtomicInt newChanges;

signals:
    void signalScopeCalculationFinished();

private slots:
    void slotMagnifyChanged();
    void slotActiveMonitorChanged(bool isClipMonitor);
    void slotRenderZoneUpdated();
    void slotScopeCalculationFinished();
    void slotUpdateScope();

};

#endif // VECTORSCOPE_H
