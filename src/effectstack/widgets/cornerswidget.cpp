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
#include "utils/KoIconUtils.h"
#include "renderer.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "onmonitoritems/onmonitorcornersitem.h"

#include <QGraphicsView>
#include "klocalizedstring.h"

inline int lerp(const int a, const int b, double t)
{
    return a + (b - a) * t;
}

CornersWidget::CornersWidget(Monitor *monitor, const QDomElement &e, int minFrame, int maxFrame, int pos, const Timecode &tc, int activeKeyframe, QWidget *parent) :
    KeyframeEdit(e, minFrame, maxFrame, tc, activeKeyframe, parent),
    m_monitor(monitor),
    m_pos(pos)
{
    m_monitor->slotShowEffectScene(MonitorSceneCorners);
    connect(m_monitor, &Monitor::effectPointsChanged, this, &CornersWidget::slotUpdateGeometry);
    connect(this, &KeyframeEdit::valueChanged, this, &CornersWidget::slotUpdateItem, Qt::UniqueConnection);
    connect(m_monitor, &Monitor::addKeyframe, this, &CornersWidget::slotPrepareKeyframe);
}

CornersWidget::~CornersWidget()
{
}

void CornersWidget::setFrameSize(const QPoint &size, double stretch)
{
    QSize profileSize = m_monitor->profileSize();
    if (size.y() == 0 || (size.x() == profileSize.width() && size.y() == profileSize.height())) {
        // no need for dar bars
        m_monitor->setQmlProperty(QStringLiteral("sourcedar"), 0);
    } else {
        // show bars indicating source clip aspect ratio
        m_monitor->setQmlProperty(QStringLiteral("sourcedar"), (double) size.x() * stretch / size.y());
    }
}

void CornersWidget::addParameter(const QDomElement &e, int activeKeyframe)
{
    KeyframeEdit::addParameter(e, activeKeyframe);
    slotUpdateItem();
}

void CornersWidget::slotUpdateItem()
{
    if (keyframe_list->columnCount() < 8) {
        return;
    }
    QTableWidgetItem *keyframe, *keyframeOld;
    keyframe = keyframeOld = keyframe_list->item(0, 0);
    for (int row = 0; row < keyframe_list->rowCount(); ++row) {
        keyframeOld = keyframe;
        keyframe = keyframe_list->item(row, 0);
        int pos = getPos(row);
        if (pos >= m_pos) {
            if (pos == m_pos) {
                keyframe_list->setCurrentCell(row, 0);
            }
            break;
        }
    }

    QVariantList points, pointsPrev, pointsNext;
    pointsPrev = getPoints(keyframeOld);
    pointsNext = getPoints(keyframe);
    if (pointsPrev.count() != 4 || pointsNext.count() != 4) {
        return;
    }

    qreal position = (m_pos - getPos(keyframeOld->row())) / (qreal)(getPos(keyframe->row()) - getPos(keyframeOld->row()) + 1);

    if (keyframeOld  == keyframe) {
        points = pointsNext;
    } else {
        points.reserve(4);
        for (int i = 0; i < 4; ++i) {
            points.append(QVariant(QLineF(pointsPrev.at(i).toPointF(), pointsNext.at(i).toPointF()).pointAt(position).toPoint()));
        }
    }

    //m_item->setPolygon(QPolygonF() << points.at(0) << points.at(1) << points.at(2) << points.at(3));
    //m_monitor->setUpEffectGeometry(QPolygonF() << points.at(0) << points.at(1) << points.at(2) << points.at(3));
    m_monitor->setUpEffectGeometry(QRect(), points);
    bool enable = getPos(keyframe->row()) - m_min == m_pos || keyframe_list->rowCount() == 1;
    m_monitor->setEffectKeyframe(enable);
}

void CornersWidget::slotUpdateGeometry(const QVariantList &points)
{
    if (keyframe_list->columnCount() < 8) {
        return;
    }
    QTableWidgetItem *item = keyframe_list->currentItem();
    blockSignals(true);
    for (int col = 0; col < 4; ++col) {
        QPoint value = points.at(col).toPoint();
        int valX = 2000.0 * value.x() / m_monitor->render->frameRenderWidth() + 2000;
        int valY = 2000.0 * value.y() / m_monitor->render->renderHeight() + 2000;
        QTableWidgetItem *nitem = keyframe_list->item(item->row(), col * 2);
        if (nitem->text().toInt() != valX) {
            nitem->setText(QString::number(valX));
        }
        nitem = keyframe_list->item(item->row(), col * 2 + 1);
        if (nitem->text().toInt() != valY) {
            nitem->setText(QString::number(valY));
        }
    }
    slotAdjustKeyframeInfo(false);
    blockSignals(false);
    disconnect(this, &KeyframeEdit::valueChanged, this, &CornersWidget::slotUpdateItem);
    generateAllParams();
    connect(this, &KeyframeEdit::valueChanged, this, &CornersWidget::slotUpdateItem, Qt::UniqueConnection);
}

QVariantList CornersWidget::getPoints(QTableWidgetItem *keyframe)
{
    QVariantList points;

    if (!keyframe) {
        return points;
    }

    for (int col = 0; col < 4; ++col) {
        if (!keyframe_list->item(keyframe->row(), col)) {
            return QVariantList();
        }
        double xVal = (keyframe_list->item(keyframe->row(), col * 2)->text().toInt() - 2000) / 2000.;
        double yVal = (keyframe_list->item(keyframe->row(), col * 2 + 1)->text().toInt() - 2000) / 2000.;
        points << QPoint(xVal * m_monitor->render->frameRenderWidth(), yVal * m_monitor->render->renderHeight());
    }
    return points;
}

void CornersWidget::slotShowLines(bool show)
{
    KdenliveSettings::setOnmonitoreffects_cornersshowlines(show);
    //m_item->update();
}

void CornersWidget::slotShowControls(bool show)
{
    KdenliveSettings::setOnmonitoreffects_cornersshowcontrols(show);
    //m_item->update();
}

void CornersWidget::slotSyncPosition(int relTimelinePos)
{
    if (keyframe_list->rowCount()) {
        m_pos = qBound(0, relTimelinePos, m_max);
        slotUpdateItem();
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
        if (getPos(row) >= m_pos) {
            break;
        }
    }

    int pos2;
    if (row == keyframe_list->rowCount()) {
        pos2 = m_max;
    } else {
        pos2 = getPos(row);
        if (pos2 == m_pos) {
            return;
        }
    }

    int pos1 = 0;
    if (row > 0) {
        pos1 = getPos(row - 1);
    }

    int col = keyframe_list->currentColumn();
    double pos = (m_pos - pos1) / (double)(pos2 - pos1 + 1);

    keyframe_list->insertRow(row);
    keyframe_list->setVerticalHeaderItem(row, new QTableWidgetItem(getPosString(m_pos)));

    QVariantList points = m_monitor->effectPolygon();
    double val;
    for (int i = 0; i < keyframe_list->columnCount(); ++i) {
        if (i < 8) {
            if (i % 2 == 0) {
                val = points.at(i / 2).toPoint().x() / (double)m_monitor->render->frameRenderWidth();
            } else {
                val = points.at(i / 2).toPoint().y() / (double)m_monitor->render->renderHeight();
            }
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

void CornersWidget::slotPrepareKeyframe()
{
    slotAddKeyframe(m_pos);
}

