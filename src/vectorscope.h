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
    Vectorscope(Render *projRender, Render *clipRender, QWidget *parent = 0);
    ~Vectorscope();

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *event);

private:
    Render *m_projRender;
    Render *m_clipRender;
    Render *m_activeRender;
    QImage m_scope;
    int iPaintMode;
    float scaling;
    QPoint mapToCanvas(QRect inside, QPointF point);

    bool circleOnly;
    QPoint mousePos;

private slots:
    void slotPaintModeChanged(int index);
    void slotMagnifyChanged();
    void slotActiveMonitorChanged(bool isClipMonitor);

};

#endif // VECTORSCOPE_H
