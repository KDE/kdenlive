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
#include "monitor/monitor.h"
#include "monitor/monitorscene.h"
#include "monitoreditwidget.h"
#include "onmonitoritems/onmonitorcornersitem.h"
#include "renderer.h"
#include "kdenlivesettings.h"

#include <QGraphicsView>
#include <QGridLayout>
#include <QToolButton>

#include <KIcon>

inline int lerp( const int a, const int b, double t )
{
    return a + (b - a) * t;
}

CornersWidget::CornersWidget(Monitor *monitor, const QDomElement& e, int minFrame, int maxFrame, const Timecode &tc, int activeKeyframe, QWidget* parent) :
        KeyframeEdit(e, minFrame, maxFrame, tc, activeKeyframe, parent),
        m_monitor(monitor),
        m_pos(0)
{
    MonitorEditWidget *edit = monitor->getEffectEdit();
    m_scene = edit->getScene();
    m_scene->cleanup();

    m_item = new OnMonitorCornersItem();
    m_scene->addItem(m_item);

    // TODO: Better Icons
    edit->removeCustomControls();
    edit->addCustomButton(KIcon("transform-move"), i18n("Show additional controls"), this, SLOT(slotShowControls(bool)),
                          true, KdenliveSettings::onmonitoreffects_cornersshowcontrols());
    edit->addCustomButton(KIcon("insert-horizontal-rule"), i18n("Show/Hide the lines connecting the corners"), this, SLOT(slotShowLines(bool)),
                          true, KdenliveSettings::onmonitoreffects_cornersshowlines());

    connect(m_item, SIGNAL(changed()), this, SLOT(slotUpdateProperties()));
    connect(m_scene, SIGNAL(addKeyframe()), this, SLOT(slotInsertKeyframe()));

    connect(keyframe_list, SIGNAL(cellChanged(int,int)), this, SLOT(slotUpdateItem()));
    m_scene->centerView();
}

CornersWidget::~CornersWidget()
{
    m_scene->removeItem(m_item);
    delete m_item;
    if (m_monitor) {
        MonitorEditWidget *edit = m_monitor->getEffectEdit();
        edit->removeCustomControls();
    }
}

void CornersWidget::addParameter(const QDomElement &e, int activeKeyframe)
{
    KeyframeEdit::addParameter(e, activeKeyframe);

    if (!m_item->polygon().count())
        slotUpdateItem();
}

void CornersWidget::slotUpdateItem()
{
    if (keyframe_list->columnCount() < 8)
        return;

    QTableWidgetItem *keyframe, *keyframeOld;
    keyframe = keyframeOld = keyframe_list->item(0, 0);
    for (int row = 0; row < keyframe_list->rowCount(); ++row) {
        keyframeOld = keyframe;
        keyframe = keyframe_list->item(row, 0);
        if (getPos(row) >= m_pos)
            break;
    }

    QList<QPointF> points, pointsPrev, pointsNext;
    pointsPrev = getPoints(keyframeOld);
    pointsNext = getPoints(keyframe);
    if (pointsPrev.count() != 4 || pointsNext.count() != 4)
        return;

    qreal position = (m_pos - getPos(keyframeOld->row())) / (qreal)( getPos(keyframe->row()) - getPos(keyframeOld->row()) + 1 );

    if (keyframeOld  == keyframe) {
        points = pointsNext;
    } else {
        for (int i = 0; i < 4; ++i)
            points.append(QLineF(pointsPrev.at(i), pointsNext.at(i)).pointAt(position));
    }

    m_scene->blockSignals(true);
    m_item->setPolygon(QPolygonF() << points.at(0) << points.at(1) << points.at(2) << points.at(3));
    m_scene->blockSignals(false);

    bool enable = getPos(keyframe->row()) == m_pos || keyframe_list->rowCount() == 1;
    m_item->setEnabled(enable);
    m_scene->setEnabled(enable);
}

void CornersWidget::slotUpdateProperties()
{
    if (keyframe_list->columnCount() < 8)
        return;

    QPolygonF pol = m_item->polygon();

    QTableWidgetItem *item = keyframe_list->currentItem();
    double val;
    for (int col = 0; col < 8; col++) {
        if (col % 2 == 0)
            val = pol.at(col / 2).x() / (double)m_monitor->render->frameRenderWidth();
        else
            val = pol.at(col / 2).y() / (double)m_monitor->render->renderHeight();
        val *= 2000;
        val += 2000;
        QTableWidgetItem *nitem = keyframe_list->item(item->row(), col);
        if (nitem->text().toInt() != (int)val)
            nitem->setText(QString::number((int)val));
    }

    slotAdjustKeyframeInfo(false);
}

QList<QPointF> CornersWidget::getPoints(QTableWidgetItem* keyframe)
{
    QList<QPointF> points;

    if (!keyframe)
        return points;

    for (int col = 0; col < 8; col++) {
        if (!keyframe_list->item(keyframe->row(), col))
            return QList<QPointF>();
        double val = (keyframe_list->item(keyframe->row(), col)->text().toInt() - 2000) / 2000.;
        if (col % 2 == 0)
            points << QPointF(val * m_monitor->render->frameRenderWidth(), 0);
        else
            points[col / 2].setY(val * m_monitor->render->renderHeight());
    }
    return points;
}


void CornersWidget::slotShowLines(bool show)
{
    KdenliveSettings::setOnmonitoreffects_cornersshowlines(show);
    m_item->update();
}

void CornersWidget::slotShowControls(bool show)
{
    KdenliveSettings::setOnmonitoreffects_cornersshowcontrols(show);
    m_item->update();
}

void CornersWidget::slotSyncPosition(int relTimelinePos)
{
    if (keyframe_list->rowCount()) {
        relTimelinePos = qBound(0, relTimelinePos, m_max);
        if (relTimelinePos != m_pos - m_min) {
            m_pos = relTimelinePos + m_min;
            slotUpdateItem();
        }
    }
}

void CornersWidget::slotInsertKeyframe()
{
    keyframe_list->blockSignals(true);

    int row;
    QTableWidgetItem *keyframe, *keyframeOld;
    keyframe = keyframeOld = keyframe_list->item(0, 0);
    for (row = 0; row < keyframe_list->rowCount(); ++row) {
        keyframeOld = keyframe;
        keyframe = keyframe_list->item(row, 0);
        if (getPos(row) >= m_pos)
            break;
    }


    int pos2;
    if (row == keyframe_list->rowCount()) {
        pos2 = m_max;
    } else {
        pos2 = getPos(row);
        if (pos2 == m_pos)
            return;
    }

    int pos1 = 0;
    if (row > 0)
        pos1 = getPos(row - 1);

    int col = keyframe_list->currentColumn();
    double pos = (m_pos - pos1) / (double)(pos2 - pos1 + 1);

    keyframe_list->insertRow(row);
    keyframe_list->setVerticalHeaderItem(row, new QTableWidgetItem(getPosString(m_pos)));

    QPolygonF pol = m_item->polygon();
    double val;
    for (int i = 0; i < keyframe_list->columnCount(); ++i) {
        if (i < 8) {
            if (i % 2 == 0)
                val = pol.at(i / 2).x() / (double)m_monitor->render->frameRenderWidth();
            else
                val = pol.at(i / 2).y() / (double)m_monitor->render->renderHeight();
            val *= 2000;
            val += 2000;
            keyframe_list->setItem(row, i, new QTableWidgetItem(QString::number((int)val)));
        } else {
            keyframe_list->setItem(row, i, new QTableWidgetItem(QString::number(lerp(keyframe_list->item(keyframeOld->row(), i)->text().toInt(), keyframe_list->item(keyframe->row(), i)->text().toInt(), pos))));
        }
    }

    keyframe_list->resizeRowsToContents();
    slotAdjustKeyframeInfo();
    keyframe_list->blockSignals(false);
    generateAllParams();
    button_delete->setEnabled(true);
    keyframe_list->setCurrentCell(row, col);
    keyframe_list->selectRow(row);
}


#include "cornerswidget.moc"
