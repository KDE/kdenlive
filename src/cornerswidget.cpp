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
#include "kdenlivesettings.h"

#include <QGraphicsView>
#include <QGridLayout>

#include <KIcon>

CornersWidget::CornersWidget(Monitor *monitor, QDomElement e, int minFrame, int maxFrame, Timecode tc, int activeKeyframe, QWidget* parent) :
        KeyframeEdit(e, minFrame, maxFrame, tc, activeKeyframe, parent),
        m_monitor(monitor),
        m_showScene(true)
{
    m_scene = monitor->getEffectScene();

    m_item = new OnMonitorCornersItem(m_scene);
    m_scene->addItem(m_item);

    m_config = new MonitorSceneControlWidget(m_scene, this);
    QGridLayout *l = static_cast<QGridLayout *>(layout());
    l->addWidget(m_config->getShowHideButton(), 1, 1);
    l->addWidget(m_config, 1, 2);

    QToolButton *buttonShowLines = new QToolButton(m_config);
    // TODO: Better Icons
    buttonShowLines->setIcon(KIcon("insert-horizontal-rule"));
    buttonShowLines->setToolTip(i18n("Show/Hide the lines connecting the corners"));
    buttonShowLines->setCheckable(true);
    buttonShowLines->setChecked(KdenliveSettings::onmonitoreffects_cornersshowlines());
    connect(buttonShowLines, SIGNAL(toggled(bool)), this, SLOT(slotShowLines(bool)));
    m_config->addWidget(buttonShowLines, 0, 2);
    QToolButton *buttonShowControls = new QToolButton(m_config);
    buttonShowControls->setIcon(KIcon("transform-move"));
    buttonShowControls->setToolTip(i18n("Show additional controls"));
    buttonShowControls->setCheckable(true);
    buttonShowControls->setChecked(KdenliveSettings::onmonitoreffects_cornersshowcontrols());
    connect(buttonShowControls, SIGNAL(toggled(bool)), this, SLOT(slotShowControls(bool)));
    m_config->addWidget(buttonShowControls, 0, 3);

    connect(m_config, SIGNAL(showScene(bool)), this, SLOT(slotShowScene(bool)));
    connect(m_monitor, SIGNAL(renderPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
    connect(m_scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateProperties()));

    connect(keyframe_list, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateItem()));
    connect(keyframe_list, SIGNAL(cellChanged(int, int)), this, SLOT(slotUpdateItem()));
}

CornersWidget::~CornersWidget()
{
    delete m_config;
    m_scene->removeItem(m_item);
    delete m_item;
    if (m_monitor)
        m_monitor->slotEffectScene(false);
}

void CornersWidget::addParameter(QDomElement e, int activeKeyframe)
{
    KeyframeEdit::addParameter(e, activeKeyframe);

    if (!m_item->polygon().count())
        slotUpdateItem();
}

void CornersWidget::slotUpdateItem()
{
    if (keyframe_list->columnCount() < 8)
        return;

    QTableWidgetItem *item = keyframe_list->currentItem();
    if (!item)
        return;

    QList<QPointF> points;
    double val;
    for (int col = 0; col < 8; col++) {
        if (!keyframe_list->item(item->row(), col))
            return;
        val = (keyframe_list->item(item->row(), col)->text().toInt() - 2000) / 2000.;
        if (col % 2 == 0)
            points << QPointF(val * m_monitor->render->frameRenderWidth(), 0);
        else
            points[col / 2].setY(val * m_monitor->render->renderHeight());
    }

    m_scene->blockSignals(true);
    m_item->setPolygon(QPolygonF() << points.at(0) << points.at(1) << points.at(2) << points.at(3));
    m_scene->blockSignals(false);
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

void CornersWidget::slotCheckMonitorPosition(int renderPos)
{
    if (m_showScene)
        emit checkMonitorPosition(renderPos);
}

void CornersWidget::slotShowScene(bool show)
{
    m_showScene = show;
    if (!m_showScene)
        m_monitor->slotEffectScene(false);
    else
        slotCheckMonitorPosition(m_monitor->render->seekFramePosition());
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

#include "cornerswidget.moc"
