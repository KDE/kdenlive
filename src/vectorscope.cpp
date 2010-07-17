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
#include <QAction>

#include <qtconcurrentrun.h>
#include <QThread>
#include <QTime>

#include "vectorscope.h"

const float SCALING = 1/.7; // See class docs
const float P75 = .75;
const unsigned char DEFAULT_Y = 255;
const unsigned int REALTIME_FPS = 15; // in fps.

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
    m_skipPixels(1),
    circleEnabled(false),
    initialDimensionUpdateDone(false),
    semaphore(1)
{
    setupUi(this);

    m_colorTools = new ColorTools();
    m_colorPlaneExport = new ColorPlaneExport(this);

    paintMode->addItem(i18n("Green"), QVariant(PAINT_GREEN));
    paintMode->addItem(i18n("Green 2"), QVariant(PAINT_GREEN2));
    paintMode->addItem(i18n("Black"), QVariant(PAINT_BLACK));
    paintMode->addItem(i18n("Modified YUV (Chroma)"), QVariant(PAINT_CHROMA));
    paintMode->addItem(i18n("YUV"), QVariant(PAINT_YUV));
    paintMode->addItem(i18n("Original Color"), QVariant(PAINT_ORIG));

    backgroundMode->addItem(i18n("None"), QVariant(BG_NONE));
    backgroundMode->addItem(i18n("YUV"), QVariant(BG_YUV));
    backgroundMode->addItem(i18n("Modified YUV (Chroma)"), QVariant(BG_CHROMA));

    cbAutoRefresh->setChecked(true);

    connect(backgroundMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotBackgroundChanged()));
    connect(cbMagnify, SIGNAL(stateChanged(int)), this, SLOT(slotMagnifyChanged()));
    connect(this, SIGNAL(signalScopeCalculationFinished(uint,uint)), this, SLOT(slotScopeCalculationFinished(uint,uint)));
    connect(m_colorTools, SIGNAL(signalWheelCalculationFinished()), this, SLOT(slotWheelCalculationFinished()));
    connect(paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateScope()));
    connect(cbAutoRefresh, SIGNAL(stateChanged(int)), this, SLOT(slotUpdateScope()));

    newFrames.fetchAndStoreRelaxed(0);
    newChanges.fetchAndStoreRelaxed(0);
    newWheelChanges.fetchAndStoreRelaxed(0);


    ///// Build context menu /////
    setContextMenuPolicy(Qt::ActionsContextMenu);

    m_aExportBackground = new QAction(i18n("Export background"), this);
    addAction(m_aExportBackground);
    connect(m_aExportBackground, SIGNAL(triggered()), this, SLOT(slotExportBackground()));

    m_a75PBox = new QAction(i18n("75% box"), this);
    m_a75PBox->setCheckable(true);
    m_a75PBox->setChecked(false);
    addAction(m_a75PBox);
    connect(m_a75PBox, SIGNAL(changed()), this, SLOT(update()));

    m_aAxisEnabled = new QAction(i18n("Draw axis"), this);
    m_aAxisEnabled->setCheckable(true);
    m_aAxisEnabled->setChecked(false);
    addAction(m_aAxisEnabled);
    connect(m_aAxisEnabled, SIGNAL(changed()), this, SLOT(update()));

    m_aRealtime = new QAction(i18n("Realtime (with precision loss)"), this);
    m_aRealtime->setCheckable(true);
    m_aRealtime->setChecked(false);
    addAction(m_aRealtime);


    this->setMouseTracking(true);
    updateDimensions();
    prodWheelThread();
}

Vectorscope::~Vectorscope()
{
    delete m_colorPlaneExport;
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
    bool ok = false;
    if (m_scopeCalcThread.isRunning()) {
        qDebug() << "Calc thread still running.";
        ok = false;
    } else {

        // Acquire the semaphore. Don't release it anymore until the QFuture m_scopeCalcThread
        // tells us that the thread has finished.
        ok = semaphore.tryAcquire(1);
        if (ok) {
            qDebug() << "Calc thread not running anymore, finished: " << m_scopeCalcThread.isFinished() << ", Starting new thread";

            // See http://doc.qt.nokia.com/latest/qtconcurrentrun.html#run about
            // running member functions in a thread
            m_scopeCalcThread = QtConcurrent::run(this, &Vectorscope::calculateScope);

            newFrames.fetchAndStoreRelease(0); // Reset number of new frames, as we just got the newest
            newChanges.fetchAndStoreRelease(0); // Do the same with the external changes counter
        } else {
            qDebug() << "Could not acquire semaphore. Deadlock avoided? Not starting new thread.";
        }
    }
    return ok;
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
            m_wheelCalcThread = QtConcurrent::run(m_colorTools, &ColorTools::yuvColorWheel, m_scopeRect.size(), (unsigned char) 128, 1/SCALING, false, true);
            break;
        case BG_CHROMA:
            m_wheelCalcThread = QtConcurrent::run(m_colorTools, &ColorTools::yuvColorWheel, m_scopeRect.size(), (unsigned char) 255, 1/SCALING, true, true);
            break;
        }
        newWheelChanges.fetchAndStoreRelaxed(0);
        return true;
    }
}



void Vectorscope::calculateScope()
{
    qDebug() << "..Scope rendering starts now.";
    QTime start = QTime::currentTime();
    unsigned int skipPixels = 1;
    if (m_aRealtime->isChecked()) {
        skipPixels = m_skipPixels;
    }
    const int stepsize = 4*skipPixels;

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

    // Just an average for the number of image pixels per scope pixel.
    double avgPxPerPx = (double) 4*img.byteCount()/scope.size().width()/scope.size().height()/skipPixels;
    qDebug() << "Expecting " << avgPxPerPx << " pixels per pixel.";

    const QRect scopeRect(QPoint(0,0), scope.size());

    for (int i = 0; i < img.byteCount(); i+= stepsize) {
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
                scope.setPixel(pt, qRgba(qRed(px)+(255-qRed(px))/(3*avgPxPerPx), qGreen(px)+20*(255-qGreen(px))/(avgPxPerPx),
                                         qBlue(px)+(255-qBlue(px))/(avgPxPerPx), qAlpha(px)+(255-qAlpha(px))/(avgPxPerPx)));
                break;
            case PAINT_GREEN2:
                px = scope.pixel(pt);
                scope.setPixel(pt, qRgba(qRed(px)+ceil((255-(float)qRed(px))/(4*avgPxPerPx)), 255,
                                         qBlue(px)+ceil((255-(float)qBlue(px))/(avgPxPerPx)), qAlpha(px)+ceil((255-(float)qAlpha(px))/(avgPxPerPx))));
                break;
            case PAINT_BLACK:
                px = scope.pixel(pt);
                scope.setPixel(pt, qRgba(0,0,0, qAlpha(px)+(255-qAlpha(px))/20));
                break;
            }
        }

        bits += stepsize;
    }

    m_scope = scope;

    unsigned int mseconds = start.msecsTo(QTime::currentTime());
    qDebug() << "Scope rendered in " << mseconds << " ms. Sending finished signal.";
    emit signalScopeCalculationFinished(mseconds, skipPixels);
    qDebug() << "xxScope: Signal finished sent.";
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

    centerPoint = mapToCanvas(m_scopeRect, QPointF(0,0));
    pR75 = mapToCanvas(m_scopeRect, P75*SCALING*YUV_R);
    pG75 = mapToCanvas(m_scopeRect, P75*SCALING*YUV_G);
    pB75 = mapToCanvas(m_scopeRect, P75*SCALING*YUV_B);
    pCy75 = mapToCanvas(m_scopeRect, P75*SCALING*YUV_Cy);
    pMg75 = mapToCanvas(m_scopeRect, P75*SCALING*YUV_Mg);
    pYl75 = mapToCanvas(m_scopeRect, P75*SCALING*YUV_Yl);
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

    // Draw axis
    if (m_aAxisEnabled->isChecked()) {
        davinci.drawLine(mapToCanvas(m_scopeRect, QPointF(0,-.9)), mapToCanvas(m_scopeRect, QPointF(0,.9)));
        davinci.drawLine(mapToCanvas(m_scopeRect, QPointF(-.9,0)), mapToCanvas(m_scopeRect, QPointF(.9,0)));
    }

    // Draw center point
    switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
    case BG_CHROMA:
        davinci.setPen(penDark);
        break;
    default:
        davinci.setPen(penThin);
        break;
    }
    davinci.drawEllipse(centerPoint, 5,5);


    // Draw 75% box
    if (m_a75PBox->isChecked()) {
        davinci.drawLine(pR75, pYl75);
        davinci.drawLine(pYl75, pG75);
        davinci.drawLine(pG75, pCy75);
        davinci.drawLine(pCy75, pB75);
        davinci.drawLine(pB75, pMg75);
        davinci.drawLine(pMg75, pR75);
    }

    // Draw RGB/CMY points with 75% chroma (for NTSC)
    davinci.setPen(penThin);
    davinci.drawEllipse(pR75, 3,3);
    davinci.drawEllipse(pG75, 3,3);
    davinci.drawEllipse(pB75, 3,3);
    davinci.drawEllipse(pCy75, 3,3);
    davinci.drawEllipse(pMg75, 3,3);
    davinci.drawEllipse(pYl75, 3,3);



    // Draw the scope data (previously calculated in a separate thread)
    davinci.drawImage(m_scopeRect.topLeft(), m_scope);


    if (circleEnabled) {
        // Mouse moved: Draw a circle over the scope

        int dx = -centerPoint.x()+mousePos.x();
        int dy =  centerPoint.y()-mousePos.y();

        QPoint reference = mapToCanvas(m_scopeRect, QPointF(1,0));

        float r = sqrt(dx*dx + dy*dy);
        float percent = (float) 100*r/SCALING/m_scaling/(reference.x() - centerPoint.x());

        switch (backgroundMode->itemData(backgroundMode->currentIndex()).toInt()) {
        case BG_NONE:
            davinci.setPen(penLight);
            break;
        default:
            davinci.setPen(penDark);
            break;
        }
        davinci.drawEllipse(centerPoint, (int)r, (int)r);
        davinci.setPen(penThin);
        davinci.drawText(m_scopeRect.bottomRight()-QPoint(40,0), i18n("%1 \%", QString::number(percent, 'f', 0)));
        
        float angle = copysign(acos(dx/r)*180/M_PI, dy);
        davinci.drawText(m_scopeRect.bottomLeft()+QPoint(10,0), i18n("%1°", QString::number(angle, 'f', 1)));

        circleEnabled = false;
    }

    // Draw realtime factor (number of skipped pixels)
    if (m_aRealtime->isChecked()) {
        davinci.setPen(penThin);
        davinci.drawText(m_scopeRect.bottomRight()-QPoint(40,15), QVariant(m_skipPixels).toString().append("x"));
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

void Vectorscope::slotScopeCalculationFinished(unsigned int mseconds, unsigned int skipPixels)
{
    qDebug() << "Received finished signal.";
    if (!m_scopeCalcThread.isFinished()) {
        // Wait for the thread to finish. Otherwise the scope might not get updated
        // as prodCalcThread may see it still running.
        QTime start = QTime::currentTime();
        qDebug() << "Scope renderer has not finished yet although finished signal received, waiting ...";
        m_scopeCalcThread.waitForFinished();
        qDebug() << "Waiting for finish is over. Waited for " << start.msecsTo(QTime::currentTime()) << " ms";
    }
    semaphore.release();

    this->update();
    qDebug() << "Scope updated.";

    if (m_aRealtime->isChecked()) {
        m_skipPixels = ceil((float)REALTIME_FPS*mseconds*skipPixels/1000);
        Q_ASSERT(m_skipPixels >= 1);
        qDebug() << "Realtime checked. Switching from " << skipPixels << " to " << m_skipPixels;

    } else {
        qDebug() << "No realtime.";
    }

    // If auto-refresh is enabled and new frames are available,
    // just start the next calculation.
    if (newFrames > 0 && cbAutoRefresh->isChecked()) {
        qDebug() << "Found more frames in the queue (prodding now): " << newFrames;
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
    m_colorPlaneExport->show();

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
        if (paintMode->itemData(paintMode->currentIndex()).toInt() == PAINT_BLACK) {
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
    prodWheelThread();
    QWidget::resizeEvent(event);
}
