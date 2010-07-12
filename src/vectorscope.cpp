/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
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
#include <QPainter>
#include <QDebug>

#include "vectorscope.h"

const float SCALING = 1/.7; // See class docs
const float P75 = .75;
const QPointF YUV_R(-.147,  .615);
const QPointF YUV_G(-.289, -.515);
const QPointF YUV_B(.437, -.100);
const QPointF YUV_Cy(.147, -.615);
const QPointF YUV_Mg(.289,  .515);
const QPointF YUV_Yl(-.437,  .100);



Vectorscope::Vectorscope(Render *render, QWidget *parent) :
    QWidget(parent),
    m_render(render),
    iPaintMode(GREEN),
    scaling(1)
{
    setupUi(this);
    paintMode->insertItem(GREEN, i18n("Green"));
    paintMode->insertItem(CHROMA, i18n("Chroma"));
    paintMode->insertItem(ORIG, i18n("Original Color"));
    connect(paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotPaintModeChanged(int)));
    connect(cbMagnify, SIGNAL(stateChanged(int)), this, SLOT(slotMagnifyChanged()));
}

Vectorscope::~Vectorscope()
{
}

void Vectorscope::mousePressEvent(QMouseEvent *)
{
    this->update();
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

void Vectorscope::paintEvent(QPaintEvent *)
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
    int cw = wh - topleft.y();
    if (ww < cw) { cw = ww; }
    cw -= 2*offset;
    QRect scopeRect(topleft, QPoint(cw, cw) + topleft);


    // Draw the vectorscope circle
    QPainter davinci(this);
    QPoint vinciPoint;
    davinci.setRenderHint(QPainter::Antialiasing, true);
    davinci.fillRect(0, 0, ww, wh, QColor(25,25,23));

    davinci.setPen(QPen(QBrush(QColor(250,250,250)), 2, Qt::SolidLine));
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
    davinci.setPen(QPen(QBrush(QColor(250,250,250)), 1, Qt::SolidLine));
    davinci.drawEllipse(mapToCanvas(scopeRect, QPointF(0,0)), 4,4);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_R), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_G), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_B), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_Cy), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_Mg), 3,3);
    davinci.drawEllipse(mapToCanvas(scopeRect, P75*SCALING*YUV_Yl), 3,3);


    // If no renderer available, there is nothing more to paint now.
    if (m_render == 0) return;


    // Prepare the vectorscope data
    QImage scope(cw, cw, QImage::Format_ARGB32);
    scope.fill(qRgba(0,0,0,0));

    QImage img(m_render->extractFrame(m_render->seekFramePosition()));
    uchar *bits = img.bits();

    davinci.setCompositionMode(QPainter::CompositionMode_Plus);
    davinci.setPen(QColor(144,255,100,50));
    
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

    davinci.drawImage(scopeRect.topLeft(), scope);

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
