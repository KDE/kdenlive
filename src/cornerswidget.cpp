/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "cornerswidget.h"
#include "monitor.h"
#include "monitorscene.h"
#include "monitorscenecontrolwidget.h"
#include "onmonitoritems/onmonitorcornersitem.h"
#include "renderer.h"

#include <QGraphicsView>
#include <QHBoxLayout>

#include <KIcon>

CornersWidget::CornersWidget(Monitor* monitor, int clipPos, bool isEffect, int factor, QWidget* parent) :
        QWidget(parent),
        m_monitor(monitor),
        m_clipPos(clipPos),
        m_inPoint(0),
        m_outPoint(1),
        m_isEffect(isEffect),
        m_showScene(true),
        m_factor(factor)
{
    m_ui.setupUi(this);

    m_scene = monitor->getEffectScene();

    m_item = new OnMonitorCornersItem(m_scene);
    m_scene->addItem(m_item);

    m_config = new MonitorSceneControlWidget(m_scene, m_ui.frameConfig);
    QHBoxLayout *layout = new QHBoxLayout(m_ui.frameConfig);
    layout->addWidget(m_config->getShowHideButton());
    layout->addWidget(m_config);

    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();

    m_ui.spinX1->setRange(-width, width * 2);
    m_ui.spinX2->setRange(-width, width * 2);
    m_ui.spinX3->setRange(-width, width * 2);
    m_ui.spinX4->setRange(-width, width * 2);
    m_ui.spinY1->setRange(-height, height * 2);
    m_ui.spinY2->setRange(-height, height * 2);
    m_ui.spinY3->setRange(-height, height * 2);
    m_ui.spinY4->setRange(-height, height * 2);

    m_ui.toolReset1->setIcon(KIcon("edit-undo"));
    m_ui.toolReset1->setToolTip(i18n("Reset Corner 1"));
    m_ui.toolReset2->setIcon(KIcon("edit-undo"));
    m_ui.toolReset2->setToolTip(i18n("Reset Corner 2"));
    m_ui.toolReset3->setIcon(KIcon("edit-undo"));
    m_ui.toolReset3->setToolTip(i18n("Reset Corner 3"));
    m_ui.toolReset4->setIcon(KIcon("edit-undo"));
    m_ui.toolReset4->setToolTip(i18n("Reset Corner 4"));

    connect(m_ui.spinX1, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinX2, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinX3, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinX4, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinY1, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinY2, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinY3, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));
    connect(m_ui.spinY4, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateItem()));

    connect(m_ui.toolReset1, SIGNAL(clicked()), this, SLOT(slotResetCorner1()));
    connect(m_ui.toolReset2, SIGNAL(clicked()), this, SLOT(slotResetCorner2()));
    connect(m_ui.toolReset3, SIGNAL(clicked()), this, SLOT(slotResetCorner3()));
    connect(m_ui.toolReset4, SIGNAL(clicked()), this, SLOT(slotResetCorner4()));

    connect(m_config, SIGNAL(showScene(bool)), this, SLOT(slotShowScene(bool)));
    connect(m_monitor, SIGNAL(renderPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
    connect(m_scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateProperties()));
}

CornersWidget::~CornersWidget()
{
    delete m_config;
    m_scene->removeItem(m_item);
    delete m_item;
    if (m_monitor)
        m_monitor->slotEffectScene(false);
}

void CornersWidget::setRange(int minframe, int maxframe)
{
    m_inPoint = minframe;
    m_outPoint = maxframe;
    slotCheckMonitorPosition(m_monitor->render->seekFramePosition());
}

void CornersWidget::slotUpdateItem()
{
    QPointF c1(m_ui.spinX1->value(), m_ui.spinY1->value());
    QPointF c2(m_ui.spinX2->value(), m_ui.spinY2->value());
    QPointF c3(m_ui.spinX3->value(), m_ui.spinY3->value());
    QPointF c4(m_ui.spinX4->value(), m_ui.spinY4->value());

    m_item->setPolygon(QPolygonF() << c1 << c2 << c3 << c4);

    emit parameterChanged();
}

void CornersWidget::slotUpdateProperties(bool changed)
{
    QPolygon pol = m_item->polygon().toPolygon();
    blockSignals(true);
    m_ui.spinX1->setValue(pol.at(0).x());
    m_ui.spinX2->setValue(pol.at(1).x());
    m_ui.spinX3->setValue(pol.at(2).x());
    m_ui.spinX4->setValue(pol.at(3).x());
    m_ui.spinY1->setValue(pol.at(0).y());
    m_ui.spinY2->setValue(pol.at(1).y());
    m_ui.spinY3->setValue(pol.at(2).y());
    m_ui.spinY4->setValue(pol.at(3).y());
    blockSignals(false);

    if (changed)
        emit parameterChanged();
}


QPolygon CornersWidget::getValue()
{
    qreal width = m_monitor->render->frameRenderWidth();
    qreal height = m_monitor->render->renderHeight();
    QPolygon corners = m_item->polygon().toPolygon();
    QPolygon points;
    QPoint p;
    for (int i = 0; i < 4; ++i) {
        p = corners.at(i);
        p.setX((p.x() / width + 1) / 3.0 * m_factor);
        p.setY((p.y() / height + 1) / 3.0 * m_factor);
        points << p;
    }
    return points;
}

void CornersWidget::setValue(const QPolygon& points)
{
    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();
    QPolygonF corners;
    QPoint p;
    for (int i = 0; i < 4; ++i) {
        p = points.at(i);
        p.setX((p.x() / (qreal)m_factor * 3 - 1) * width);
        p.setY((p.y() / (qreal)m_factor * 3 - 1) * height);
        corners << p;
    }
    m_item->setPolygon(corners);

    slotUpdateProperties(false);
}

void CornersWidget::slotCheckMonitorPosition(int renderPos)
{
    if (m_showScene) {
        /*
           We do only get the position in timeline if this geometry belongs to a transition,
           therefore we need two ways here.
         */
        if (m_isEffect) {
            emit checkMonitorPosition(renderPos);
        } else {
            if (renderPos >= m_clipPos && renderPos <= m_clipPos + m_outPoint - m_inPoint) {
                if (!m_scene->views().at(0)->isVisible())
                    m_monitor->slotEffectScene(true);
            } else {
                m_monitor->slotEffectScene(false);
            }
        }
    }
}

void CornersWidget::slotShowScene(bool show)
{
    m_showScene = show;
    if (!m_showScene)
        m_monitor->slotEffectScene(false);
    else
        slotCheckMonitorPosition(m_monitor->render->seekFramePosition());
}

void CornersWidget::slotResetCorner1()
{
    blockSignals(true);
    m_ui.spinX1->setValue(0);
    m_ui.spinY1->setValue(0);
    blockSignals(false);
    slotUpdateItem();
}

void CornersWidget::slotResetCorner2()
{
    blockSignals(true);
    m_ui.spinX2->setValue(m_monitor->render->frameRenderWidth());
    m_ui.spinY2->setValue(0);
    blockSignals(false);
    slotUpdateItem();
}

void CornersWidget::slotResetCorner3()
{
    blockSignals(true);
    m_ui.spinX3->setValue(m_monitor->render->frameRenderWidth());
    m_ui.spinY3->setValue(m_monitor->render->renderHeight());
    blockSignals(false);
    slotUpdateItem();
}

void CornersWidget::slotResetCorner4()
{
    blockSignals(true);
    m_ui.spinX4->setValue(0);
    m_ui.spinY4->setValue(m_monitor->render->renderHeight());
    blockSignals(false);
    slotUpdateItem();
}

#include "cornerswidget.moc"
