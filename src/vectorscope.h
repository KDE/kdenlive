/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef VECTORSCOPE_H
#define VECTORSCOPE_H

#include "renderer.h"
#include "ui_vectorscope_ui.h"

class Render;
class Vectorscope_UI;

enum PAINT_MODE { GREEN = 0, ORIG = 1, CHROMA = 2 };

class Vectorscope : public QWidget, public Ui::Vectorscope_UI {
    Q_OBJECT

public:
    Vectorscope(Render *render, QWidget *parent = 0);
    ~Vectorscope();

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);

private:
    Render *m_render;
    int iPaintMode;
    float scaling;
    QPoint mapToCanvas(QRect inside, QPointF point);

private slots:
    void slotPaintModeChanged(int index);
    void slotMagnifyChanged();

};

#endif // VECTORSCOPE_H
