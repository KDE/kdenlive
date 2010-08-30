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
#include "keyframehelper.h"
#include "timecodedisplay.h"
#include "monitorscene.h"
#include "kdenlivesettings.h"

#include <QtCore>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QVBoxLayout>
#include <QGridLayout>

GeometryWidget::GeometryWidget(Monitor* monitor, Timecode timecode, int clipPos, bool isEffect, QWidget* parent ):
        QWidget(parent),
        m_monitor(monitor),
        m_timePos(new TimecodeDisplay(timecode)),
        m_clipPos(clipPos),
        m_inPoint(0),
        m_outPoint(1),
        m_isEffect(isEffect),
        m_rect(NULL),
        m_geometry(NULL),
        m_showScene(true)
{
    m_ui.setupUi(this);
    m_scene = monitor->getEffectScene();


    /*
        Setup of timeline and keyframe controls
    */

    ((QGridLayout *)(m_ui.widgetTimeWrapper->layout()))->addWidget(m_timePos, 1, 6);

    QVBoxLayout *layout = new QVBoxLayout(m_ui.frameTimeline);
    m_timeline = new KeyframeHelper(m_ui.frameTimeline);
    layout->addWidget(m_timeline);
    layout->setContentsMargins(0, 0, 0, 0);

    m_ui.buttonPrevious->setIcon(KIcon("media-skip-backward"));
    m_ui.buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    m_ui.buttonNext->setIcon(KIcon("media-skip-forward"));
    m_ui.buttonNext->setToolTip(i18n("Go to next keyframe"));
    m_ui.buttonAddDelete->setIcon(KIcon("document-new"));
    m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));

    m_ui.buttonSync->setIcon(KIcon("insert-link"));
    m_ui.buttonSync->setToolTip(i18n("Synchronize with timeline cursor"));
    m_ui.buttonSync->setChecked(KdenliveSettings::transitionfollowcursor());

    connect(m_timeline, SIGNAL(positionChanged(int)), this, SLOT(slotPositionChanged(int)));
    connect(m_timeline, SIGNAL(keyframeMoved(int)),   this, SLOT(slotKeyframeMoved(int)));
    connect(m_timeline, SIGNAL(addKeyframe(int)),     this, SLOT(slotAddKeyframe(int)));
    connect(m_timeline, SIGNAL(removeKeyframe(int)),  this, SLOT(slotDeleteKeyframe(int)));
    connect(m_timePos, SIGNAL(editingFinished()), this, SLOT(slotPositionChanged()));
    connect(m_ui.buttonPrevious,  SIGNAL(clicked()), this, SLOT(slotPreviousKeyframe()));
    connect(m_ui.buttonNext,      SIGNAL(clicked()), this, SLOT(slotNextKeyframe()));
    connect(m_ui.buttonAddDelete, SIGNAL(clicked()), this, SLOT(slotAddDeleteKeyframe()));
    connect(m_ui.buttonSync,      SIGNAL(toggled(bool)), this, SLOT(slotSetSynchronize(bool)));


    /*
        Setup of geometry controls
    */

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

    connect(m_ui.spinOpacity,      SIGNAL(valueChanged(int)), this, SLOT(slotSetOpacity(int)));
    connect(m_ui.sliderOpacity,    SIGNAL(valueChanged(int)), m_ui.spinOpacity, SLOT(setValue(int)));

    connect(m_ui.buttonMoveLeft,   SIGNAL(clicked()), this, SLOT(slotMoveLeft()));
    connect(m_ui.buttonCenterH,    SIGNAL(clicked()), this, SLOT(slotCenterH()));
    connect(m_ui.buttonMoveRight,  SIGNAL(clicked()), this, SLOT(slotMoveRight()));
    connect(m_ui.buttonMoveTop,    SIGNAL(clicked()), this, SLOT(slotMoveTop()));
    connect(m_ui.buttonCenterV,    SIGNAL(clicked()), this, SLOT(slotCenterV()));
    connect(m_ui.buttonMoveBottom, SIGNAL(clicked()), this, SLOT(slotMoveBottom()));


    /*
        Setup of configuration controls
    */

    m_ui.buttonConfig->setIcon(KIcon("system-run"));
    m_ui.buttonConfig->setToolTip(i18n("Show/Hide settings"));
    m_ui.groupSettings->setHidden(true);

    m_ui.buttonShowScene->setIcon(KIcon("video-display"));
    m_ui.buttonShowScene->setToolTip(i18n("Show monitor scene"));
    m_ui.buttonDirectUpdate->setIcon(KIcon("transform-scale"));
    m_ui.buttonDirectUpdate->setToolTip(i18n("Update parameters while monitor scene changes"));
    m_ui.buttonDirectUpdate->setChecked(KdenliveSettings::monitorscene_directupdate());

    m_ui.buttonZoomFit->setIcon(KIcon("zoom-fit-best"));
    m_ui.buttonZoomFit->setToolTip(i18n("Fit zoom to monitor size"));
    m_ui.buttonZoomOriginal->setIcon(KIcon("zoom-original"));
    m_ui.buttonZoomOriginal->setToolTip(i18n("Original size"));
    m_ui.buttonZoomIn->setIcon(KIcon("zoom-in"));
    m_ui.buttonZoomIn->setToolTip(i18n("Zoom in"));
    m_ui.buttonZoomOut->setIcon(KIcon("zoom-out"));
    m_ui.buttonZoomOut->setToolTip(i18n("Zoom out"));

    connect(m_ui.buttonConfig, SIGNAL(toggled(bool)), m_ui.groupSettings, SLOT(setVisible(bool)));

    connect(m_ui.sliderZoom, SIGNAL(valueChanged(int)), m_scene, SLOT(slotZoom(int)));
    connect(m_scene, SIGNAL(zoomChanged(int)), m_ui.sliderZoom, SLOT(setValue(int)));
    connect(m_ui.buttonZoomFit,      SIGNAL(clicked()), m_scene, SLOT(slotZoomFit()));
    connect(m_ui.buttonZoomOriginal, SIGNAL(clicked()), m_scene, SLOT(slotZoomOriginal()));
    connect(m_ui.buttonZoomIn,       SIGNAL(clicked()), m_scene, SLOT(slotZoomIn()));
    connect(m_ui.buttonZoomOut,      SIGNAL(clicked()), m_scene, SLOT(slotZoomOut()));
    m_scene->slotZoomFit();

    connect(m_ui.buttonShowScene, SIGNAL(toggled(bool)), this, SLOT(slotShowScene(bool)));
    connect(m_ui.buttonDirectUpdate, SIGNAL(toggled(bool)), m_scene, SLOT(slotSetDirectUpdate(bool)));


    connect(m_scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateGeometry()));
    connect(m_monitor, SIGNAL(renderPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
    connect(this, SIGNAL(parameterChanged()), this, SLOT(slotUpdateProperties()));
}

GeometryWidget::~GeometryWidget()
{
    delete m_timePos;
    delete m_timeline;
    m_scene->removeItem(m_rect);
    delete m_geometry;
    if (m_monitor)
        m_monitor->slotEffectScene(false);
}

void GeometryWidget::updateTimecodeFormat()
{
    m_timePos->slotUpdateTimeCodeFormat();
}

QString GeometryWidget::getValue() const
{
    return m_geometry->serialise();
}

void GeometryWidget::setupParam(const QDomElement elem, int minframe, int maxframe)
{
    m_inPoint = minframe;
    m_outPoint = maxframe;

    char *tmp = (char *) qstrdup(elem.attribute("value").toUtf8().data());
    if (m_geometry)
        m_geometry->parse(tmp, maxframe - minframe, m_monitor->render->renderWidth(), m_monitor->render->renderHeight());
    else
        m_geometry = new Mlt::Geometry(tmp, maxframe - minframe, m_monitor->render->renderWidth(), m_monitor->render->renderHeight());
    delete[] tmp;

    // remove keyframes out of range
    Mlt::GeometryItem invalidItem;
    bool foundInvalidItem = false;
    while (!m_geometry->next_key(&invalidItem, maxframe - minframe)) {
        foundInvalidItem = true;
        m_geometry->remove(invalidItem.frame());
    }

    if (elem.attribute("fixed") == "1") {
        // Keyframes are disabled
        m_ui.widgetTimeWrapper->setHidden(true);
    } else {
        m_ui.widgetTimeWrapper->setHidden(false);
        m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint - 1);
        m_timeline->update();
        m_timePos->setRange(0, m_outPoint - m_inPoint - 1);
    }

    // no opacity
    if (elem.attribute("opacity") == "false")
        m_ui.widgetOpacity->setHidden(true);

    Mlt::GeometryItem item;

    m_geometry->fetch(&item, 0);
    delete m_rect;
    m_rect = new QGraphicsRectItem(QRectF(0, 0, item.w(), item.h()));
    m_rect->setPos(item.x(), item.y());
    m_rect->setZValue(0);
    m_rect->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    m_rect->setPen(framepen);
    m_rect->setBrush(Qt::transparent);
    m_scene->addItem(m_rect);

    slotPositionChanged(0, false);
    slotCheckMonitorPosition(m_monitor->render->seekFramePosition());

    // update if we had to remove a keyframe which got out of range
    if (foundInvalidItem)
        QTimer::singleShot(300, this, SIGNAL(parameterChanged()));
}

void GeometryWidget::slotSyncPosition(int relTimelinePos)
{
    // do only sync if this effect is keyframable
    if (m_timePos->maximum() > 0 && KdenliveSettings::transitionfollowcursor()) {
        relTimelinePos = qMax(0, relTimelinePos);
        relTimelinePos = qMin(relTimelinePos, m_timePos->maximum());
        if (relTimelinePos != m_timePos->getValue())
            slotPositionChanged(relTimelinePos, false);
    }
}


void GeometryWidget::slotPositionChanged(int pos, bool seek)
{
    if (pos == -1) {
        pos = m_timePos->getValue();
        m_timeline->blockSignals(true);
        m_timeline->setValue(pos);
        m_timeline->blockSignals(false);
    } else {
        m_timePos->setValue(pos);
    }

    Mlt::GeometryItem item;
    if (m_geometry->fetch(&item, pos) || item.key() == false) {
        // no keyframe
        m_scene->setEnabled(false);
        m_ui.widgetGeometry->setEnabled(false);
        m_ui.buttonAddDelete->setIcon(KIcon("document-new"));
        m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    } else {
        // keyframe
        m_scene->setEnabled(true);
        m_ui.widgetGeometry->setEnabled(true);
        m_ui.buttonAddDelete->setIcon(KIcon("edit-delete"));
        m_ui.buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    }

    m_rect->setPos(item.x(), item.y());
    m_rect->setRect(0, 0, item.w(), item.h());

    m_ui.spinOpacity->blockSignals(true);
    m_ui.sliderOpacity->blockSignals(true);
    m_ui.spinOpacity->setValue(item.mix());
    m_ui.sliderOpacity->setValue(item.mix());
    m_ui.spinOpacity->blockSignals(false);
    m_ui.sliderOpacity->blockSignals(false);

    slotUpdateProperties();

    if (seek && KdenliveSettings::transitionfollowcursor())
        emit seekToPos(m_clipPos + pos);
}

void GeometryWidget::slotKeyframeMoved(int pos)
{
    slotPositionChanged(pos);
    slotUpdateGeometry();
}

void GeometryWidget::slotAddKeyframe(int pos)
{
    Mlt::GeometryItem item;
    if (pos == -1)
        pos = m_timePos->getValue();
    item.frame(pos);
    QRectF r = m_rect->rect().normalized();
    QPointF rectpos = m_rect->pos();
    item.x(rectpos.x());
    item.y(rectpos.y());
    item.w(r.width());
    item.h(r.height());
    item.mix(m_ui.spinOpacity->value());
    m_geometry->insert(item);

    m_timeline->update();
    slotPositionChanged(pos, false);
    emit parameterChanged();
}

void GeometryWidget::slotDeleteKeyframe(int pos)
{
    Mlt::GeometryItem item;
    if (pos == -1)
        pos = m_timePos->getValue();
    // check there is more than one keyframe, do not allow to delete last one
    if (m_geometry->next_key(&item, pos + 1)) {
        if (m_geometry->prev_key(&item, pos - 1) || item.frame() == pos)
            return;
    }
    m_geometry->remove(pos);

    m_timeline->update();
    slotPositionChanged(pos, false);
    emit parameterChanged();
}

void GeometryWidget::slotPreviousKeyframe()
{
    Mlt::GeometryItem item;
    // Go to start if no keyframe is found
    int currentPos = m_timePos->getValue();
    int pos = 0;
    if(!m_geometry->prev_key(&item, currentPos - 1) && item.frame() < currentPos)
        pos = item.frame();

    slotPositionChanged(pos);
}

void GeometryWidget::slotNextKeyframe()
{
    Mlt::GeometryItem item;
    // Go to end if no keyframe is found
    int pos = m_timeline->frameLength;
    if (!m_geometry->next_key(&item, m_timeline->value() + 1))
        pos = item.frame();

    slotPositionChanged(pos);
}

void GeometryWidget::slotAddDeleteKeyframe()
{
    Mlt::GeometryItem item;
    if (m_geometry->fetch(&item, m_timePos->getValue()) || item.key() == false)
        slotAddKeyframe();
    else
        slotDeleteKeyframe();
}


void GeometryWidget::slotCheckMonitorPosition(int renderPos)
{
    if (m_showScene) {
        /*
            We do only get the position in timeline if this geometry belongs to a transition,
            therefore we need to ways here.
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


void GeometryWidget::slotUpdateGeometry()
{
    Mlt::GeometryItem item;
    int pos = m_timePos->getValue();
    // get keyframe and make sure it is the correct one
    if (m_geometry->next_key(&item, pos) || item.frame() != pos)
        return;

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
        size = (int)((rectSize.width() * 100.0 / m_monitor->render->renderWidth()) + 0.5);
    else
        size = (int)((rectSize.height() * 100.0 / m_monitor->render->renderHeight()) + 0.5);

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
    m_rect->setRect(0, 0,
                    (int)((m_monitor->render->renderWidth() * value / 100.0) + 0.5),
                    (int)((m_monitor->render->renderHeight() * value / 100.0) + 0.5));
    slotUpdateGeometry();
}


void GeometryWidget::slotSetOpacity(int value)
{
    m_ui.sliderOpacity->blockSignals(true);
    m_ui.sliderOpacity->setValue(value);
    m_ui.sliderOpacity->blockSignals(false);

    int pos = m_timePos->getValue();
    Mlt::GeometryItem item;
    if (m_geometry->fetch(&item, pos) || item.key() == false)
        return;
    item.mix(value);
    m_geometry->insert(item);

    emit parameterChanged();
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


void GeometryWidget::slotSetSynchronize(bool sync)
{
    KdenliveSettings::setTransitionfollowcursor(sync);
}

void GeometryWidget::slotShowScene(bool show)
{
    m_showScene = show;
    if (!m_showScene)
        m_monitor->slotEffectScene(false);
    else
        slotCheckMonitorPosition(m_monitor->render->seekFramePosition());
}

#include "geometrywidget.moc"
