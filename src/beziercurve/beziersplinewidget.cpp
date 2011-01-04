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

#include "beziersplinewidget.h"
#include "colortools.h"
#include "kdenlivesettings.h"

#include <QVBoxLayout>

#include <KIcon>

BezierSplineWidget::BezierSplineWidget(const QString& spline, QWidget* parent) :
        QWidget(parent),
        m_mode(ModeRGB)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_edit);
    QWidget *widget = new QWidget(this);
    m_ui.setupUi(widget);
    layout->addWidget(widget);

    m_ui.buttonLinkHandles->setIcon(KIcon("insert-link"));
    m_ui.buttonZoomIn->setIcon(KIcon("zoom-in"));
    m_ui.buttonZoomOut->setIcon(KIcon("zoom-out"));
    m_ui.buttonGridChange->setIcon(KIcon("view-grid"));
    m_ui.buttonShowPixmap->setIcon(QIcon(QPixmap::fromImage(ColorTools::rgbCurvePlane(QSize(16, 16), ColorTools::COL_Luma, 0.8))));
    m_ui.buttonResetSpline->setIcon(KIcon("view-refresh"));
    m_ui.widgetPoint->setEnabled(false);

    CubicBezierSpline s;
    s.fromString(spline);
    m_edit.setSpline(s);

    connect(&m_edit, SIGNAL(modified()), this, SIGNAL(modified()));
    connect(&m_edit, SIGNAL(currentPoint(const BPoint&)), this, SLOT(slotUpdatePoint(const BPoint&)));

    connect(m_ui.spinPX, SIGNAL(editingFinished()), this, SLOT(slotUpdatePointP()));
    connect(m_ui.spinPY, SIGNAL(editingFinished()), this, SLOT(slotUpdatePointP()));
    connect(m_ui.spinH1X, SIGNAL(editingFinished()), this, SLOT(slotUpdatePointH1()));
    connect(m_ui.spinH1Y, SIGNAL(editingFinished()), this, SLOT(slotUpdatePointH1()));
    connect(m_ui.spinH2X, SIGNAL(editingFinished()), this, SLOT(slotUpdatePointH2()));
    connect(m_ui.spinH2Y, SIGNAL(editingFinished()), this, SLOT(slotUpdatePointH2()));

    connect(m_ui.buttonLinkHandles, SIGNAL(toggled(bool)), this, SLOT(slotSetHandlesLinked(bool)));
    connect(m_ui.buttonZoomIn, SIGNAL(clicked()), &m_edit, SLOT(slotZoomIn()));
    connect(m_ui.buttonZoomOut, SIGNAL(clicked()), &m_edit, SLOT(slotZoomOut()));
    connect(m_ui.buttonGridChange, SIGNAL(clicked()), this, SLOT(slotGridChange()));
    connect(m_ui.buttonShowPixmap, SIGNAL(toggled(bool)), this, SLOT(slotShowPixmap(bool)));
    connect(m_ui.buttonResetSpline, SIGNAL(clicked()), this, SLOT(slotResetSpline()));

    m_edit.setGridLines(KdenliveSettings::bezier_gridlines());
    m_ui.buttonShowPixmap->setChecked(KdenliveSettings::bezier_showpixmap());
}

QString BezierSplineWidget::spline()
{
    return m_edit.spline().toString();
}

void BezierSplineWidget::setMode(BezierSplineWidget::CurveModes mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        if (m_showPixmap)
            slotShowPixmap();
    }
}

void BezierSplineWidget::slotGridChange()
{
    m_edit.setGridLines((m_edit.gridLines() + 1) % 9);
    KdenliveSettings::setBezier_gridlines(m_edit.gridLines());
}

void BezierSplineWidget::slotShowPixmap(bool show)
{
    m_showPixmap = show;
    KdenliveSettings::setBezier_showpixmap(show);
    if (show && m_mode != ModeAlpha && m_mode != ModeRGB)
        m_edit.setPixmap(QPixmap::fromImage(ColorTools::rgbCurvePlane(m_edit.size(), (ColorTools::ColorsRGB)((int)(m_mode)))));
    else
        m_edit.setPixmap(QPixmap());
}

void BezierSplineWidget::slotUpdatePoint(const BPoint &p)
{
    blockSignals(true);
    if (p == BPoint()) {
        m_ui.widgetPoint->setEnabled(false);
    } else {
        m_ui.widgetPoint->setEnabled(true);
        m_ui.spinPX->setValue(qRound(p.p.x() * 255));
        m_ui.spinPY->setValue(qRound(p.p.y() * 255));
        m_ui.spinH1X->setValue(qRound(p.h1.x() * 255));
        m_ui.spinH1Y->setValue(qRound(p.h1.y() * 255));
        m_ui.spinH2X->setValue(qRound(p.h2.x() * 255));
        m_ui.spinH2Y->setValue(qRound(p.h2.y() * 255));
        m_ui.buttonLinkHandles->setChecked(p.handlesLinked);
    }
    blockSignals(false);
}

void BezierSplineWidget::slotUpdatePointP()
{
    BPoint p = m_edit.getCurrentPoint();

    p.setP(QPointF(m_ui.spinPX->value() / 255., m_ui.spinPY->value() / 255.));

    m_edit.updateCurrentPoint(p);
}

void BezierSplineWidget::slotUpdatePointH1()
{
    BPoint p = m_edit.getCurrentPoint();

    p.setH1(QPointF(m_ui.spinH1X->value() / 255., m_ui.spinH1Y->value() / 255.));

    m_edit.updateCurrentPoint(p);
}

void BezierSplineWidget::slotUpdatePointH2()
{
    BPoint p = m_edit.getCurrentPoint();

    p.setH2(QPointF(m_ui.spinH2X->value() / 255., m_ui.spinH2Y->value() / 255.));

    m_edit.updateCurrentPoint(p);
}

void BezierSplineWidget::slotSetHandlesLinked(bool linked)
{
    BPoint p = m_edit.getCurrentPoint();
    p.handlesLinked = linked;
    m_edit.updateCurrentPoint(p);
}

void BezierSplineWidget::slotResetSpline()
{
    m_edit.setSpline(CubicBezierSpline());
}

#include "beziersplinewidget.moc"
