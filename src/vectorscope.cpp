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

  Vectorscope.

  The basis matrix for converting RGB to YUV is:

mRgb2Yuv =                       r =

   0.29900   0.58700   0.11400     1.00000
  -0.14741  -0.28939   0.43680  x  0.00000
   0.61478  -0.51480  -0.09998     0.00000

  The resulting YUV value is then drawn on the circle
  using U and V as coordinate values.

  The maximum length of such an UV vector is reached
  for the colors Red and Cyan: 0.632.
  To make optimal use of space in the circle, this value
  can be used for scaling.

  As we are dealing with RGB values in a range of {0,...,255}
  and the conversion values are made for [0,1], we already
  divide the conversion values by 255 previously, e.g. in
  GNU octave.
 */

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>

#include <qtconcurrentrun.h>
#include <QThread>

#include "vectorscope.h"

const float SCALING = 1/.7; // See class docs
const float P75 = .75;
const QPointF YUV_R(-.147,  .615);
const QPointF YUV_G(-.289, -.515);
const QPointF YUV_B(.437, -.100);
const QPointF YUV_Cy(.147, -.615);
const QPointF YUV_Mg(.289,  .515);
const QPointF YUV_Yl(-.437,  .100);

const QPen penThick(QBrush(QColor(250,250,250)), 2, Qt::SolidLine);
const QPen penThin(QBrush(QColor(250,250,250)), 1, Qt::SolidLine);
const QPen penLight(QBrush(QColor(144,255,100,50)), 1, Qt::SolidLine);


Vectorscope::Vectorscope(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    QWidget(parent),
    m_projMonitor(projMonitor),
    m_clipMonitor(clipMonitor),
    m_activeRender(clipMonitor->render),
    iPaintMode(GREEN),
    scaling(1),
    circleEnabled(false),
    initialDimensionUpdateDone(false)
{
    setupUi(this);

    paintMode->insertItem(GREEN, i18n("Green"));
    paintMode->insertItem(CHROMA, i18n("Chroma"));
    paintMode->insertItem(ORIG, i18n("Original Color"));

    connect(paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotPaintModeChanged(int)));
    connect(cbMagnify, SIGNAL(stateChanged(int)), this, SLOT(slotMagnifyChanged()));
    connect(this, SIGNAL(signalScopeCalculationFinished()), this, SLOT(slotScopeCalculationFinished()));
    connect(cbMagnify, SIGNAL(stateChanged(int)), this, SLOT(slotRenderZoneUpdated()));

    newFrames.fetchAndStoreRelaxed(0);

    this->setMouseTracking(true);
    updateDimensions();
}

Vectorscope::~Vectorscope()
{
}

/**

  Input point is on [-1,1]², 0 being at the center,
  and positive directions are top/right.

  Maps to a QRect «inside» which is using the
  0 point in the top left corner. The coordinates of
  the rect «inside» are relative to «parent». The
  coordinates returned can be used in the parent.


    parent v
  +-------------------+
  | inside v          |
  | +-----------+     |
  | |    +      |     |
  | |  --0++    |     | < point
  | |    -      |     |
  | +-----------+     |
  |                   |
  +-------------------+

 */
QPoint Vectorscope::mapToCanvas(QRect inside, QPointF point)
{
    return QPoint(point.x()/2*inside.width()  + inside.width()/2  + inside.x(),
                 -point.y()/2*inside.height() + inside.height()/2 + inside.y());
}

bool Vectorscope::prodCalcThread()
{
    if (m_scopeCalcThread.isRunning()) {
        qDebug() << "Calc thread still running.";
        return false;
    } else {
        // See http://doc.qt.nokia.com/latest/qtconcurrentrun.html#run about
        // running member functions in a thread
        qDebug() << "Calc thread not running anymore, finished: " << m_scopeCalcThread.isFinished() << ", Starting new thread";
        m_scopeCalcThread = QtConcurrent::run(this, &Vectorscope::calculateScope);
        return true;
    }
}

void Vectorscope::calculateScope()
{
    // Prepare the vectorscope data
    QImage scope(cw, cw, QImage::Format_ARGB32);
    scope.fill(qRgba(0,0,0,0));

    QImage img(m_activeRender->extractFrame(m_activeRender->seekFramePosition()));
    newFrames.fetchAndStoreRelease(0); // Reset number of new frames, as we just got the newest
    uchar *bits = img.bits();

    int r,g,b;
    double dy, dr, dg, db, dmax;
    double y,u,v;
    QPoint pt;
    QRgb px;

    for (int i = 0; i < img.byteCount(); i+= 4) {
        QRgb *col = (QRgb *) bits;

        r = qRed(*col);
        g = qGreen(*col);
        b = qBlue(*col);

        y = (double) 0.001173 * r + 0.002302 * g + 0.0004471 * b;
        u = (double) -0.0005781*r -0.001135*g +0.001713*b;
        v = (double) 0.002411*r -0.002019*g -0.0003921*b;

        pt = mapToCanvas(QRect(QPoint(0,0), scope.size()), QPointF(SCALING*scaling*u, SCALING*scaling*v));

        if (!(pt.x() <= scopeRect.width() && pt.x() >= 0
            && pt.y() <= scopeRect.height() && pt.y() >= 0)) {
            // Point lies outside (because of scaling), don't plot it
            continue;
        }

        // Draw the pixel using the chosen draw mode
        switch (iPaintMode) {
        case CHROMA:
            dy = 200;
            dr = dy + 290.8*v;
            dg = dy - 100.6*u - 148*v;
            db = dy + 517.2*u;

            dmax = dr;
            if (dg > dmax) dmax = dg;
            if (db > dmax) dmax = db;
            dmax = 255/dmax;

            dr *= dmax;
            dg *= dmax;
            db *= dmax;

            scope.setPixel(pt, qRgba(dr, dg, db, 255));
            break;
        case ORIG:
            scope.setPixel(pt, *col);
            break;
        case GREEN:
            px = scope.pixel(pt);
            scope.setPixel(pt, qRgba(qRed(px)+(255-qRed(px))/40, 255, qBlue(px)+(255-qBlue(px))/40, qAlpha(px)+(255-qAlpha(px))/20));
            break;
        }

        bits += 4;
    }

    m_scope = scope;

    qDebug() << "Scope rendered";
    emit signalScopeCalculationFinished();
}

void Vectorscope::updateDimensions()
{
    // Widget width/height
    int ww = this->size().width();
    int wh = this->size().height();

    // Distance from top/left/right
    int offset = 6;

    // controlsArea contains the controls at the top;
    // We want to paint below
    QPoint topleft(offset, controlsArea->geometry().height()+offset);

    // Circle Width: min of width and height
    cw = wh - topleft.y();
    if (ww < cw) { cw = ww; }
    cw -= 2*offset;
    scopeRect = QRect(topleft, QPoint(cw, cw) + topleft);
}

void Vectorscope::paintEvent(QPaintEvent *)
{

    if (!initialDimensionUpdateDone) {
        // This is a workaround.
        // When updating the dimensions in the constructor, the size
        // of the control items at the top are simply ignored! So do
        // it here instead.
        updateDimensions();
        initialDimensionUpdateDone = true;
    }

    // Draw the vectorscope circle
    QPainter davinci(this);
    QPoint vinciPoint;
    QPoint centerPoint = mapToCanvas(scopeRect, QPointF(0,0));
    davinci.setRenderHint(QPainter::Antialiasing, true);
    davinci.fillRect(0, 0, this->size().width(), this->size().height(), QColor(25,25,23));

    davinci.setPen(penThick);
    davinci.drawEllipse(scopeRect);

    // Draw RGB/CMY points with 100% chroma
    vinciPoint = mapToCanvas(scopeRect, SCALING*YUV_R);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint-QPoint(20, -10), "R");

    vinciPoint = mapToCanvas(scopeRect, SCALING*YUV_G);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint-QPoint(20, 0), "G");

    vinciPoint = mapToCanvas(scopeRect, SCALING*YUV_B);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint+QPoint(15, 10), "B");

    vinciPoint = mapToCanvas(scopeRect, SCALING*YUV_Cy);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint+QPoint(15, -5), "Cy");

    vinciPoint = mapToCanvas(scopeRect, SCALING*YUV_Mg);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint+QPoint(15, 10), "Mg");

    vinciPoint = mapToCanvas(scopeRect, SCALING*YUV_Yl);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint-QPoint(25, 0), "Yl");

    // Draw RGB/CMY points with 75% chroma (for NTSC)
    davinci.setPen(penThin);
    davinci.drawEllipse(centerPoint, 4,4);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_R), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_G), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_B), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_Cy), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_Mg), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_Yl), 3,3);




    davinci.setPen(penLight);


    // Draw the scope data (previously calculated in a separate thread)
    davinci.drawImage(scopeRect.topLeft(), m_scope);


    if (circleEnabled) {
        // Mouse moved: Draw a circle over the scope

        int dx = centerPoint.x()-mousePos.x();
        int dy = centerPoint.y()-mousePos.y();

        QPoint reference = mapToCanvas(scopeRect, QPointF(1,0));

        int r = sqrt(dx*dx + dy*dy);
        float percent = (float) 100*r/SCALING/scaling/(reference.x() - centerPoint.x());

        davinci.drawEllipse(centerPoint, r,r);
        davinci.setPen(penThin);
        davinci.drawText(scopeRect.bottomRight()-QPoint(40,0), QVariant((int)percent).toString().append(" %"));

        circleEnabled = false;
    }
}

void Vectorscope::slotPaintModeChanged(int index)
{
    iPaintMode = index;
    this->update();
}

void Vectorscope::slotMagnifyChanged()
{
    if (cbMagnify->isChecked()) {
        scaling = 1.4;
    } else {
        scaling = 1;
    }
    this->update();
}

void Vectorscope::slotActiveMonitorChanged(bool isClipMonitor)
{
    if (isClipMonitor) {
        m_activeRender = m_clipMonitor->render;
        disconnect(this, SLOT(slotRenderZoneUpdated()));
        connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    } else {
        m_activeRender = m_projMonitor->render;
        disconnect(this, SLOT(slotRenderZoneUpdated()));
        connect(m_activeRender, SIGNAL(rendererPosition(int)), this, SLOT(slotRenderZoneUpdated()));
    }
}

void Vectorscope::slotRenderZoneUpdated()
{
    qDebug() << "Monitor incoming. New frames total: " << newFrames;
    // Monitor has shown a new frame
    newFrames.fetchAndAddRelaxed(1);
    if (cbAutoRefresh->isChecked()) {
        prodCalcThread();
    }
}

void Vectorscope::mousePressEvent(QMouseEvent *)
{
    // Update the scope on mouse press
    prodCalcThread();
}

void Vectorscope::mouseMoveEvent(QMouseEvent *event)
{
    // Draw a circle around the center,
    // showing percentage number of the radius length

    circleEnabled = true;
    mousePos = event->pos();
    this->update();
}

void Vectorscope::resizeEvent(QResizeEvent *event)
{
    qDebug() << "Resized.";
    updateDimensions();
    QWidget::resizeEvent(event);
}

void Vectorscope::slotScopeCalculationFinished()
{
    this->update();
    qDebug() << "Scope updated.";

    // If auto-refresh is enabled and new frames are available,
    // just start the next calculation.
    if (newFrames > 0 && cbAutoRefresh->isChecked()) {
        qDebug() << "More frames in the queue: " << newFrames;
        prodCalcThread();
    }
}
