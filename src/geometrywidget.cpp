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


#include "geometrywidget.h"
#include "monitor.h"
#include "renderer.h"
#include "monitorscene.h"

#include <QtCore>
#include <QGraphicsView>
#include <QGraphicsRectItem>

GeometryWidget::GeometryWidget(Monitor* monitor, int clipPos, QWidget* parent ):
        QWidget(parent),
        m_monitor(monitor),
        m_clipPos(clipPos),
        m_inPoint(0),
        m_outPoint(1),
        m_rect(NULL),
        m_geometry(NULL)
{
    m_ui.setupUi(this);
    m_scene = monitor->getEffectScene();

    m_ui.buttonMoveLeft->setIcon(KIcon("kdenlive-align-left"));
    m_ui.buttonMoveLeft->setToolTip(i18n("Move to left"));
    m_ui.buttonCenterH->setIcon(KIcon("kdenlive-align-hor"));
    m_ui.buttonCenterH->setToolTip(i18n("Center horizontally"));
    m_ui.buttonMoveRight->setIcon(KIcon("kdenlive-align-right"));
    m_ui.buttonMoveRight->setToolTip(i18n("Move to right"));
    m_ui.buttonMoveTop->setIcon(KIcon("kdenlive-align-top"));
    m_ui.buttonMoveTop->setToolTip(i18n("Move to top"));
    m_ui.buttonCenterV->setIcon(KIcon("kdenlive-align-vert"));
    m_ui.buttonCenterV->setToolTip(i18n("Center vertically"));
    m_ui.buttonMoveBottom->setIcon(KIcon("kdenlive-align-bottom"));
    m_ui.buttonMoveBottom->setToolTip(i18n("Move to bottom"));


    connect(m_ui.spinX,            SIGNAL(valueChanged(int)), this, SLOT(slotSetX(int)));
    connect(m_ui.spinY,            SIGNAL(valueChanged(int)), this, SLOT(slotSetY(int)));
    connect(m_ui.spinWidth,        SIGNAL(valueChanged(int)), this, SLOT(slotSetWidth(int)));
    connect(m_ui.spinHeight,       SIGNAL(valueChanged(int)), this, SLOT(slotSetHeight(int)));

    connect(m_ui.spinSize,         SIGNAL(valueChanged(int)), this, SLOT(slotResize(int)));

    connect(m_ui.buttonMoveLeft,   SIGNAL(clicked()), this, SLOT(slotMoveLeft()));
    connect(m_ui.buttonCenterH,    SIGNAL(clicked()), this, SLOT(slotCenterH()));
    connect(m_ui.buttonMoveRight,  SIGNAL(clicked()), this, SLOT(slotMoveRight()));
    connect(m_ui.buttonMoveTop,    SIGNAL(clicked()), this, SLOT(slotMoveTop()));
    connect(m_ui.buttonCenterV,    SIGNAL(clicked()), this, SLOT(slotCenterV()));
    connect(m_ui.buttonMoveBottom, SIGNAL(clicked()), this, SLOT(slotMoveBottom()));

    connect(m_scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateGeometry()));
    connect(m_monitor->render, SIGNAL(rendererPosition(int)), this, SLOT(slotCheckPosition(int)));
    connect(this, SIGNAL(parameterChanged()), this, SLOT(slotUpdateProperties()));
}

GeometryWidget::~GeometryWidget()
{
    if (m_monitor)
        m_monitor->slotEffectScene(false);
    delete m_rect;
    delete m_geometry;
}

QString GeometryWidget::getValue() const
{
    return m_geometry->serialise();
}

void GeometryWidget::setupParam(const QDomElement elem, int minframe, int maxframe)
{
    m_inPoint = minframe;
    m_outPoint = maxframe;
    QString value = elem.attribute("value");

    char *tmp = (char *) qstrdup(value.toUtf8().data());
    if (m_geometry)
        m_geometry->parse(tmp, maxframe - minframe, m_monitor->render->renderWidth(), m_monitor->render->renderHeight());
    else
        m_geometry = new Mlt::Geometry(tmp, maxframe - minframe, m_monitor->render->renderWidth(), m_monitor->render->renderHeight());
    delete[] tmp;

    Mlt::GeometryItem item;

    m_geometry->fetch(&item, 0);
    delete m_rect;
    m_rect = new QGraphicsRectItem(QRectF(0, 0, item.w(), item.h()));
    m_rect->setPos(item.x(), item.y());
    m_rect->setZValue(0);
    m_rect->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    QPen framepen(Qt::DotLine);
    framepen.setColor(Qt::yellow);
    m_rect->setPen(framepen);
    m_rect->setBrush(Qt::transparent);
    m_scene->addItem(m_rect);

    slotUpdateProperties();
    slotCheckPosition(m_monitor->render->seekFramePosition());
}

void GeometryWidget::slotCheckPosition(int renderPos)
{
    if (renderPos >= m_clipPos && renderPos <= m_clipPos + m_outPoint - m_inPoint) {
        if (!m_scene->views().at(0)->isVisible())
            m_monitor->slotEffectScene(true);
    } else {
        m_monitor->slotEffectScene(false);
    }
}

void GeometryWidget::slotUpdateGeometry()
{
    Mlt::GeometryItem item;
    m_geometry->next_key(&item, 0);

    QRectF rectSize = m_rect->rect().normalized();
    QPointF rectPos = m_rect->pos();
    item.x(rectPos.x());
    item.y(rectPos.y());
    item.w(rectSize.width());
    item.h(rectSize.height());
    m_geometry->insert(item);
    emit parameterChanged();
}

void GeometryWidget::slotUpdateProperties()
{
    QRectF rectSize = m_rect->rect().normalized();
    QPointF rectPos = m_rect->pos();
    int size;
    if (rectSize.width() / m_monitor->render->dar() < rectSize.height())
        size = (int)(rectSize.width() * 100 / m_monitor->render->renderWidth());
    else
        size = (int)(rectSize.height() * 100 / m_monitor->render->renderHeight());

    m_ui.spinX->blockSignals(true);
    m_ui.spinY->blockSignals(true);
    m_ui.spinWidth->blockSignals(true);
    m_ui.spinHeight->blockSignals(true);
    m_ui.spinSize->blockSignals(true);

    m_ui.spinX->setValue(rectPos.x());
    m_ui.spinY->setValue(rectPos.y());
    m_ui.spinWidth->setValue(rectSize.width());
    m_ui.spinHeight->setValue(rectSize.height());
    m_ui.spinSize->setValue(size);

    m_ui.spinX->blockSignals(false);
    m_ui.spinY->blockSignals(false);
    m_ui.spinWidth->blockSignals(false);
    m_ui.spinHeight->blockSignals(false);
    m_ui.spinSize->blockSignals(false);
}


void GeometryWidget::slotSetX(int value)
{
    m_rect->setPos(value, m_ui.spinY->value());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetY(int value)
{
    m_rect->setPos(m_ui.spinX->value(), value);
    slotUpdateGeometry();
}

void GeometryWidget::slotSetWidth(int value)
{
    m_rect->setRect(0, 0, value, m_ui.spinHeight->value());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetHeight(int value)
{
    m_rect->setRect(0, 0, m_ui.spinWidth->value(), value);
    slotUpdateGeometry();
}


void GeometryWidget::slotResize(int value)
{
    m_rect->setRect(0, 0, m_monitor->render->renderWidth() * value / 100, m_monitor->render->renderHeight() * value / 100);
    slotUpdateGeometry();
}


void GeometryWidget::slotMoveLeft()
{
    m_rect->setPos(0, m_rect->pos().y());
    slotUpdateGeometry();
}

void GeometryWidget::slotCenterH()
{
    m_rect->setPos((m_monitor->render->renderWidth() - m_rect->rect().width()) / 2, m_rect->pos().y());
    slotUpdateGeometry();
}

void GeometryWidget::slotMoveRight()
{
    m_rect->setPos(m_monitor->render->renderWidth() - m_rect->rect().width(), m_rect->pos().y());
    slotUpdateGeometry();
}

void GeometryWidget::slotMoveTop()
{
    m_rect->setPos(m_rect->pos().x(), 0);
    slotUpdateGeometry();
}

void GeometryWidget::slotCenterV()
{
    m_rect->setPos(m_rect->pos().x(), (m_monitor->render->renderHeight() - m_rect->rect().height()) / 2);
    slotUpdateGeometry();
}

void GeometryWidget::slotMoveBottom()
{
    m_rect->setPos(m_rect->pos().x(), m_monitor->render->renderHeight() - m_rect->rect().height());
    slotUpdateGeometry();
}

#include "geometrywidget.moc"
