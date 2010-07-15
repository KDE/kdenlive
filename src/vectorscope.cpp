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

  See also:
    http://de.wikipedia.org/wiki/Vektorskop
    http://www.elektroniktutor.de/techno/vektskop.html

 */

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>
//#include <QMenu>
#include <QAction>

#include <qtconcurrentrun.h>
#include <QThread>
#include <QTime>

#include "vectorscope.h"

const float SCALING = 1/.7; // See class docs
const float P75 = .75;
const unsigned char DEFAULT_Y = 255;

const QPointF YUV_R(-.147,  .615);
const QPointF YUV_G(-.289, -.515);
const QPointF YUV_B(.437, -.100);
const QPointF YUV_Cy(.147, -.615);
const QPointF YUV_Mg(.289,  .515);
const QPointF YUV_Yl(-.437,  .100);

const QPen penThick(QBrush(QColor(250,250,250)), 2, Qt::SolidLine);
const QPen penThin(QBrush(QColor(250,250,250)), 1, Qt::SolidLine);
const QPen penLight(QBrush(QColor(200,200,250,150)), 1, Qt::SolidLine);
const QPen penDark(QBrush(QColor(0,0,20,250)), 1, Qt::SolidLine);


Vectorscope::Vectorscope(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    QWidget(parent),
    m_projMonitor(projMonitor),
    m_clipMonitor(clipMonitor),
    m_activeRender(clipMonitor->render),
    m_scaling(1),
    circleEnabled(false),
    initialDimensionUpdateDone(false)
{
    setupUi(this);

    // Use item index 20 to always append it to the list. (Update if more than 20 items here.)
    paintMode->insertItem(20, i18n("Green"), QVariant(PAINT_GREEN));
    paintMode->insertItem(20, i18n("Black"), QVariant(PAINT_BLACK));
    paintMode->insertItem(20, i18n("Chroma"), QVariant(PAINT_CHROMA));
    paintMode->insertItem(20, i18n("YUV"), QVariant(PAINT_YUV));
    paintMode->insertItem(20, i18n("Original Color"), QVariant(PAINT_ORIG));

    backgroundMode->insertItem(20, i18n("None"), QVariant(BG_NONE));
    backgroundMode->insertItem(20, i18n("YUV"), QVariant(BG_YUV));
    backgroundMode->insertItem(20, i18n("Chroma"), QVariant(BG_CHROMA));

    cbAutoRefresh->setChecked(true);

    connect(paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotPaintModeChanged(int)));
    connect(backgroundMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotBackgroundChanged()));
    connect(cbMagnify, SIGNAL(stateChanged(int)), this, SLOT(slotMagnifyChanged()));
    connect(this, SIGNAL(signalScopeCalculationFinished()), this, SLOT(slotScopeCalculationFinished()));
    connect(this, SIGNAL(signalWheelCalculationFinished()), this, SLOT(slotWheelCalculationFinished()));
    connect(paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateScope()));
    connect(cbAutoRefresh, SIGNAL(stateChanged(int)), this, SLOT(slotUpdateScope()));

    newFrames.fetchAndStoreRelaxed(0);
    newChanges.fetchAndStoreRelaxed(0);
    newWheelChanges.fetchAndStoreRelaxed(0);

    setContextMenuPolicy(Qt::ActionsContextMenu);
    m_aExportBackground = new QAction(i18n("Export background"), this);
    m_aExportBackground->setEnabled(false);
    addAction(m_aExportBackground);
    connect(m_aExportBackground, SIGNAL(triggered()), this, SLOT(slotExportBackground()));

//    m_contextMenu = QMenu(this, "Vectorscope menu");


    this->setMouseTracking(true);
    updateDimensions();
    prodWheelThread();
}

Vectorscope::~Vectorscope()
{
}

QImage Vectorscope::yuvColorWheel(const QSize &size, unsigned char Y, float scaling, bool modifiedVersion, bool circleOnly)
{
    QImage wheel(size, QImage::Format_ARGB32);
    if (size.width() == 0 || size.height() == 0) {
        qCritical("ERROR: Size of the color wheel must not be 0!");
        return wheel;
    }
    wheel.fill(qRgba(0,0,0,0));

    double dr, dg, db, du, dv, dmax;
    double ru, rv, rr;
    const int w = size.width();
    const int h = size.height();
    const float w2 = (float)w/2;
    const float h2 = (float)h/2;

    for (int u = 0; u < w; u++) {
        // Transform u from {0,...,w} to [-1,1]
        du = (double) 2*u/(w-1) - 1;
        du = scaling*du;

        for (int v = 0; v < h; v++) {
            dv = (double) 2*v/(h-1) - 1;
            dv = scaling*dv;

            if (circleOnly) {
                // Ellipsis equation: x²/a² + y²/b² = 1
                // Here: x=ru, y=rv, a=w/2, b=h/2, 1=rr
                // For rr > 1, the point lies outside. Don't draw it.
                ru = u - w2;
                rv = v - h2;
                rr = ru*ru/(w2*w2) + rv*rv/(h2*h2);
                if (rr > 1) {
                    continue;
                }
            }

            // Calculate the RGB values from YUV
            dr = Y + 290.8*dv;
            dg = Y - 100.6*du - 148*dv;
            db = Y + 517.2*du;

            if (modifiedVersion) {
                // Scale the RGB values down, or up, to max 255
                dmax = fabs(dr);
                if (fabs(dg) > dmax) dmax = fabs(dg);
                if (fabs(db) > dmax) dmax = fabs(db);
                dmax = 255/dmax;

                dr *= dmax;
                dg *= dmax;
                db *= dmax;
            }

            // Avoid overflows (which would generate intersting patterns).
            // Note that not all possible (y,u,v) values with u,v \in [-1,1]
            // have a correct RGB representation, therefore some RGB values
            // may exceed {0,...,255}.
            if (dr < 0) dr = 0;
            if (dg < 0) dg = 0;
            if (db < 0) db = 0;
            if (dr > 255) dr = 255;
            if (dg > 255) dg = 255;
            if (db > 255) db = 255;

            wheel.setPixel(u, (h-v-1), qRgba(dr, dg, db, 255));
        }
    }

    emit signalWheelCalculationFinished();
    return wheel;
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
        newFrames.fetchAndStoreRelease(0); // Reset number of new frames, as we just got the newest
        newChanges.fetchAndStoreRelease(0); // Do the same with the external changes counter
        return true;
    }
}

bool Vectorscope::prodWheelThread()
{
    if (m_wheelCalcThread.isRunning()) {
        qDebug() << "Wheel thread still running.";
        return false;
    } else {
        switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
        case BG_NONE:
            qDebug() << "No background.";
            m_wheel = QImage();
            this->update();
            break;
        case BG_YUV:
            qDebug() << "YUV background.";
            m_wheelCalcThread = QtConcurrent::run(this, &Vectorscope::yuvColorWheel, m_scopeRect.size(), (unsigned char) 128, 1/SCALING, false, true);
            break;
        case BG_CHROMA:
            m_wheelCalcThread = QtConcurrent::run(this, &Vectorscope::yuvColorWheel, m_scopeRect.size(), (unsigned char) 255, 1/SCALING, true, true);
            break;
        }
        newWheelChanges.fetchAndStoreRelaxed(0);
        return true;
    }
}



void Vectorscope::calculateScope()
{
    // Prepare the vectorscope data
    QImage scope(cw, cw, QImage::Format_ARGB32);
    scope.fill(qRgba(0,0,0,0));

    QImage img(m_activeRender->extractFrame(m_activeRender->seekFramePosition()));
    const uchar *bits = img.bits();

    int r,g,b;
    double dy, dr, dg, db, dmax;
    double y,u,v;
    QPoint pt;
    QRgb px;

    const QRect scopeRect(QPoint(0,0), scope.size());

    for (int i = 0; i < img.byteCount(); i+= 4) {
        QRgb *col = (QRgb *) bits;

        r = qRed(*col);
        g = qGreen(*col);
        b = qBlue(*col);

        y = (double)  0.001173 * r +0.002302 * g +0.0004471* b;
        u = (double) -0.0005781* r -0.001135 * g +0.001713 * b;
        v = (double)  0.002411 * r -0.002019 * g -0.0003921* b;

        pt = mapToCanvas(scopeRect, QPointF(SCALING*m_scaling*u, SCALING*m_scaling*v));

        if (!(pt.x() <= scopeRect.width() && pt.x() >= 0
            && pt.y() <= scopeRect.height() && pt.y() >= 0)) {
            // Point lies outside (because of scaling), don't plot it

        } else {

            // Draw the pixel using the chosen draw mode.
            switch (paintMode->itemData(paintMode->currentIndex()).toInt()) {
            case PAINT_YUV:
                // see yuvColorWheel
                dy = 128; // Default Y value. Lower = darker.

                // Calculate the RGB values from YUV
                dr = dy + 290.8*v;
                dg = dy - 100.6*u - 148*v;
                db = dy + 517.2*u;

                if (dr < 0) dr = 0;
                if (dg < 0) dg = 0;
                if (db < 0) db = 0;
                if (dr > 255) dr = 255;
                if (dg > 255) dg = 255;
                if (db > 255) db = 255;

                scope.setPixel(pt, qRgba(dr, dg, db, 255));
                break;

            case PAINT_CHROMA:
                dy = 200; // Default Y value. Lower = darker.

                // Calculate the RGB values from YUV
                dr = dy + 290.8*v;
                dg = dy - 100.6*u - 148*v;
                db = dy + 517.2*u;

                // Scale the RGB values back to max 255
                dmax = dr;
                if (dg > dmax) dmax = dg;
                if (db > dmax) dmax = db;
                dmax = 255/dmax;

                dr *= dmax;
                dg *= dmax;
                db *= dmax;

                scope.setPixel(pt, qRgba(dr, dg, db, 255));
                break;
            case PAINT_ORIG:
                scope.setPixel(pt, *col);
                break;
            case PAINT_GREEN:
                px = scope.pixel(pt);
                scope.setPixel(pt, qRgba(qRed(px)+(255-qRed(px))/40, 255, qBlue(px)+(255-qBlue(px))/40, qAlpha(px)+(255-qAlpha(px))/20));
                break;
            case PAINT_BLACK:
                px = scope.pixel(pt);
                scope.setPixel(pt, qRgba(0,0,0, qAlpha(px)+(255-qAlpha(px))/20));
                break;
            }
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
    m_scopeRect = QRect(topleft, QPoint(cw, cw) + topleft);
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
    QPoint centerPoint = mapToCanvas(m_scopeRect, QPointF(0,0));
    davinci.setRenderHint(QPainter::Antialiasing, true);
    davinci.fillRect(0, 0, this->size().width(), this->size().height(), QColor(25,25,23));

    davinci.drawImage(m_scopeRect.topLeft(), m_wheel);

    davinci.setPen(penThick);
    davinci.drawEllipse(m_scopeRect);

    // Draw RGB/CMY points with 100% chroma
    vinciPoint = mapToCanvas(m_scopeRect, SCALING*YUV_R);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint-QPoint(20, -10), "R");

    vinciPoint = mapToCanvas(m_scopeRect, SCALING*YUV_G);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint-QPoint(20, 0), "G");

    vinciPoint = mapToCanvas(m_scopeRect, SCALING*YUV_B);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint+QPoint(15, 10), "B");

    vinciPoint = mapToCanvas(m_scopeRect, SCALING*YUV_Cy);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint+QPoint(15, -5), "Cy");

    vinciPoint = mapToCanvas(m_scopeRect, SCALING*YUV_Mg);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint+QPoint(15, 10), "Mg");

    vinciPoint = mapToCanvas(m_scopeRect, SCALING*YUV_Yl);
    davinci.drawEllipse(vinciPoint, 4,4);
    davinci.drawText(vinciPoint-QPoint(25, 0), "Yl");

    switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
    case BG_NONE:
        davinci.setPen(penLight);
        break;
    default:
        davinci.setPen(penDark);
        break;
    }

    davinci.drawLine(mapToCanvas(m_scopeRect, QPointF(0,-.9)), mapToCanvas(m_scopeRect, QPointF(0,.9)));
    davinci.drawLine(mapToCanvas(m_scopeRect, QPointF(-.9,0)), mapToCanvas(m_scopeRect, QPointF(.9,0)));

    // Draw RGB/CMY points with 75% chroma (for NTSC)
    switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
    case BG_CHROMA:
        davinci.setPen(penDark);
        break;
    default:
        davinci.setPen(penThin);
        break;
    }
    davinci.drawEllipse(centerPoint, 5,5);
    davinci.setPen(penThin);
    davinci.drawEllipse(mapToCanvas(m_scopeRect, P75*SCALING*YUV_R), 3,3);
    davinci.drawEllipse(mapToCanvas(m_scopeRect, P75*SCALING*YUV_G), 3,3);
    davinci.drawEllipse(mapToCanvas(m_scopeRect, P75*SCALING*YUV_B), 3,3);
    davinci.drawEllipse(mapToCanvas(m_scopeRect, P75*SCALING*YUV_Cy), 3,3);
    davinci.drawEllipse(mapToCanvas(m_scopeRect, P75*SCALING*YUV_Mg), 3,3);
    davinci.drawEllipse(mapToCanvas(m_scopeRect, P75*SCALING*YUV_Yl), 3,3);



    // Draw the scope data (previously calculated in a separate thread)
    davinci.drawImage(m_scopeRect.topLeft(), m_scope);


    if (circleEnabled) {
        // Mouse moved: Draw a circle over the scope

        int dx = centerPoint.x()-mousePos.x();
        int dy = centerPoint.y()-mousePos.y();

        QPoint reference = mapToCanvas(m_scopeRect, QPointF(1,0));

        int r = sqrt(dx*dx + dy*dy);
        float percent = (float) 100*r/SCALING/m_scaling/(reference.x() - centerPoint.x());

        switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
        case BG_NONE:
            davinci.setPen(penLight);
            break;
        default:
            davinci.setPen(penDark);
            break;
        }
        davinci.drawEllipse(centerPoint, r,r);
        davinci.setPen(penThin);
        davinci.drawText(m_scopeRect.bottomRight()-QPoint(40,0), QVariant((int)percent).toString().append(" %"));

        circleEnabled = false;
    }
}




///// Slots /////

void Vectorscope::slotMagnifyChanged()
{
    if (cbMagnify->isChecked()) {
        m_scaling = 1.4;
    } else {
        m_scaling = 1;
    }
    prodCalcThread();
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

void Vectorscope::slotScopeCalculationFinished()
{
    if (!m_scopeCalcThread.isFinished()) {
        // Wait for the thread to finish. Otherwise the scope might not get updated
        // as prodCalcThread may see it still running.
        QTime start = QTime::currentTime();
        qDebug() << "Scope renderer has not finished yet, waiting ...";
        m_scopeCalcThread.waitForFinished();
        qDebug() << "Done. Waited for " << start.msecsTo(QTime::currentTime()) << " ms";
    }

    this->update();
    qDebug() << "Scope updated.";

    // If auto-refresh is enabled and new frames are available,
    // just start the next calculation.
    if (newFrames > 0 && cbAutoRefresh->isChecked()) {
        qDebug() << "More frames in the queue: " << newFrames;
        prodCalcThread();
    } else if (newChanges > 0) {
        qDebug() << newChanges << " changes (e.g. resize) in the meantime.";
        prodCalcThread();
    } else {
        qDebug() << newFrames << " new frames, " << newChanges << " new changes. Not updating.";
    }
}

void Vectorscope::slotWheelCalculationFinished()
{
    if (!m_wheelCalcThread.isFinished()) {
        QTime start = QTime::currentTime();
        qDebug() << "Wheel calc has not finished yet, waiting ...";
        m_wheelCalcThread.waitForFinished();
        qDebug() << "Done. Waited for " << start.msecsTo(QTime::currentTime()) << " ms";
    }

    qDebug() << "Wheel calculated. Updating.";
    qDebug() << m_wheelCalcThread.resultCount() << " results from the Wheel thread.";
    if (m_wheelCalcThread.resultCount() > 0) {
        m_wheel = m_wheelCalcThread.resultAt(0);
    }
    this->update();
    if (newWheelChanges > 0) {
        prodWheelThread();
    }
}

void Vectorscope::slotUpdateScope()
{
    prodCalcThread();
}

void Vectorscope::slotUpdateWheel()
{
    prodWheelThread();
}

void Vectorscope::slotExportBackground()
{
    qDebug() << "Exporting background";
}

void Vectorscope::slotBackgroundChanged()
{
    // Background changed, switch to a suitable color mode now
    int index;
    switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
    case BG_YUV:
        index = paintMode->findData(QVariant(PAINT_BLACK));
        if (index >= 0) {
            paintMode->setCurrentIndex(index);
        }
        break;

    case BG_NONE:
        if (paintMode->itemData(paintMode->currentIndex()) == PAINT_BLACK) {
            index = paintMode->findData(QVariant(PAINT_GREEN));
            paintMode->setCurrentIndex(index);
        }
        break;
    }
    newWheelChanges.fetchAndAddAcquire(1);
    prodWheelThread();
}



///// Events /////

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
    newChanges.fetchAndAddAcquire(1);
    prodCalcThread();
    QWidget::resizeEvent(event);
}
