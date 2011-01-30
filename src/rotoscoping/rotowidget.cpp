/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
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

#include "rotowidget.h"
#include "monitor.h"
#include "renderer.h"
#include "monitorscene.h"
#include "monitoreditwidget.h"
#include "onmonitoritems/rotoscoping/bpointitem.h"
#include "onmonitoritems/rotoscoping/splineitem.h"

#include <qjson/parser.h>
#include <qjson/serializer.h>


RotoWidget::RotoWidget(QString data, Monitor *monitor, int in, int out, QWidget* parent) :
        QWidget(parent),
        m_monitor(monitor),
        m_showScene(true),
        m_in(in),
        m_out(out),
        m_pos(0)
{
    MonitorEditWidget *edit = monitor->getEffectEdit();
    edit->showVisibilityButton(true);
    m_scene = edit->getScene();

    QJson::Parser parser;
    bool ok;
    m_data = parser.parse(data.toUtf8(), &ok);
    if (!ok) {
        // :(
    }

    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();
    QList <BPoint> points;
    foreach (const QVariant &bpoint, m_data.toList()) {
        QList <QVariant> l = bpoint.toList();
        BPoint p;
        p.h1 = QPointF(l.at(0).toList().at(0).toDouble() * width, l.at(0).toList().at(1).toDouble() * height);
        p.p = QPointF(l.at(1).toList().at(0).toDouble() * width, l.at(2).toList().at(1).toDouble() * height);
        p.h2 = QPointF(l.at(2).toList().at(0).toDouble() * width, l.at(2).toList().at(1).toDouble() * height);
        points << p;
    }

    m_item = new SplineItem(points, NULL, m_scene);

    connect(m_item, SIGNAL(changed()), this, SLOT(slotUpdateData()));
    connect(edit, SIGNAL(showEdit(bool)), this, SLOT(slotShowScene(bool)));
    connect(m_monitor, SIGNAL(renderPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
}

RotoWidget::~RotoWidget()
{
    m_scene->removeItem(m_item);
    delete m_item;

    if (m_monitor) {
        MonitorEditWidget *edit = m_monitor->getEffectEdit();
        edit->showVisibilityButton(false);
        edit->removeCustomControls();
        m_monitor->slotEffectScene(false);
    }
}

void RotoWidget::slotCheckMonitorPosition(int renderPos)
{
    if (m_showScene)
        emit checkMonitorPosition(renderPos);
}

void RotoWidget::slotSyncPosition(int relTimelinePos)
{
    Q_UNUSED(relTimelinePos);
}

void RotoWidget::slotShowScene(bool show)
{
    m_showScene = show;
    if (!m_showScene)
        m_monitor->slotEffectScene(false);
    else
        slotCheckMonitorPosition(m_monitor->render->seekFramePosition());
}

void RotoWidget::slotUpdateData()
{
    int width = m_monitor->render->frameRenderWidth();
    int height = m_monitor->render->renderHeight();

    QList <BPoint> spline = m_item->getPoints();
    QList <QVariant> vlist;
    foreach (const BPoint &point, spline) {
        QList <QVariant> pl;
        for (int i = 0; i < 3; ++i)
            pl << QVariant(QList <QVariant>() << QVariant(point[i].x() / width) << QVariant(point[i].y() / height));
        vlist << QVariant(pl);
    }
    m_data = QVariant(vlist);

    emit valueChanged();
}

QString RotoWidget::getSpline()
{
    QJson::Serializer serializer;
    return QString(serializer.serialize(m_data));
}

#include "rotowidget.moc"
