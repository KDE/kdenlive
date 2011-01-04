/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef BEZIERSPLINEWIDGET_H
#define BEZIERSPLINEWIDGET_H

#include "cubicbezierspline.h"
#include "beziersplineeditor.h"
#include "ui_bezierspline_ui.h"

#include <QtCore>
#include <QWidget>

class BezierSplineWidget : public QWidget
{
    Q_OBJECT
    
public:
    BezierSplineWidget(const QString &spline, QWidget* parent = 0);

    QString spline();

    enum CurveModes { ModeRed, ModeGreen, ModeBlue, ModeAlpha, ModeLuma, ModeRGB/*, ModeSaturation*/ };
    void setMode(CurveModes mode);

private slots:
    void slotUpdatePoint(const BPoint &p);

    void slotUpdatePointP();
    void slotUpdatePointH1();
    void slotUpdatePointH2();

    void slotGridChange();
    void slotShowPixmap(bool show = true);
    void slotResetSpline();
    void slotSetHandlesLinked(bool linked);

private:
    Ui::BezierSpline_UI m_ui;
    BezierSplineEditor m_edit;
    CurveModes m_mode;
    bool m_showPixmap;

signals:
    void modified();
};

#endif
