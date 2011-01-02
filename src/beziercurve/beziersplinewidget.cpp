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

#include <QVBoxLayout>

#include <KIcon>

BezierSplineWidget::BezierSplineWidget(const QString& spline, QWidget* parent) :
        QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(&m_edit);
    QWidget *widget = new QWidget(this);
    m_ui.setupUi(widget);
    layout->addWidget(widget);

    m_ui.buttonLinkHandles->setIcon(KIcon("insert-link"));
    m_ui.buttonLinkHandles->setEnabled(false);
    m_ui.buttonZoomIn->setIcon(KIcon("zoom-in"));
    m_ui.buttonZoomOut->setIcon(KIcon("zoom-out"));
    m_ui.buttonGridChange->setIcon(KIcon("view-grid"));
    m_ui.widgetPoint->setEnabled(false);

    CubicBezierSpline s;
    s.fromString(spline);
    m_edit.setSpline(s);

    connect(&m_edit, SIGNAL(modified()), this, SIGNAL(modified()));
    connect(&m_edit, SIGNAL(currentPoint(const BPoint&)), this, SLOT(slotUpdatePoint(const BPoint&)));

    connect(m_ui.spinPX, SIGNAL(editingFinished()), this, SLOT(slotUpdateSpline()));
    connect(m_ui.spinPY, SIGNAL(editingFinished()), this, SLOT(slotUpdateSpline()));
    connect(m_ui.spinH1X, SIGNAL(editingFinished()), this, SLOT(slotUpdateSpline()));
    connect(m_ui.spinH1Y, SIGNAL(editingFinished()), this, SLOT(slotUpdateSpline()));
    connect(m_ui.spinH2X, SIGNAL(editingFinished()), this, SLOT(slotUpdateSpline()));
    connect(m_ui.spinH2Y, SIGNAL(editingFinished()), this, SLOT(slotUpdateSpline()));

    connect(m_ui.buttonZoomIn, SIGNAL(clicked()), &m_edit, SLOT(slotZoomIn()));
    connect(m_ui.buttonZoomOut, SIGNAL(clicked()), &m_edit, SLOT(slotZoomOut()));
    connect(m_ui.buttonGridChange, SIGNAL(clicked()), &m_edit, SLOT(slotGridChange()));
}

QString BezierSplineWidget::spline()
{
    return m_edit.spline().toString();
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
    }
    blockSignals(false);
}

void BezierSplineWidget::slotUpdateSpline()
{
    BPoint p = m_edit.getCurrentPoint();

    // check for every value, so we do not lose too much info through rounding
    if (m_ui.spinPX->value() != qRound(p.p.x() * 255))
        p.p.setX(m_ui.spinPX->value() / 255.);
    if (m_ui.spinPY->value() != qRound(p.p.y() * 255))
        p.p.setY(m_ui.spinPY->value() / 255.);

    if (m_ui.spinH1X->value() != qRound(p.h1.x() * 255))
        p.h1.setX(m_ui.spinH1X->value() / 255.);
    if (m_ui.spinH1Y->value() != qRound(p.h1.y() * 255))
        p.h1.setY(m_ui.spinH1Y->value() / 255.);

    if (m_ui.spinH2X->value() != qRound(p.h2.x() * 255))
        p.h2.setX(m_ui.spinH2X->value() / 255.);
    if (m_ui.spinH2Y->value() != qRound(p.h2.y() * 255))
        p.h2.setY(m_ui.spinH2Y->value() / 255.);

    m_edit.updateCurrentPoint(p);
    emit modified();
}

#include "beziersplinewidget.moc"
