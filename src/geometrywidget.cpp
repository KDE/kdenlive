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
#include "monitoreditwidget.h"
#include "onmonitoritems/onmonitorrectitem.h"
#include "kdenlivesettings.h"
#include "dragvalue.h"

#include <QtCore>
#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMenu>



GeometryWidget::GeometryWidget(Monitor* monitor, Timecode timecode, int clipPos, bool isEffect, bool showRotation, QWidget* parent):
    QWidget(parent),
    m_monitor(monitor),
    m_timePos(new TimecodeDisplay(timecode)),
    m_clipPos(clipPos),
    m_inPoint(0),
    m_outPoint(1),
    m_isEffect(isEffect),
    m_rect(NULL),
    m_previous(NULL),
    m_geometry(NULL),
    m_showScene(true),
    m_showRotation(showRotation)
{
    m_ui.setupUi(this);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    MonitorEditWidget *edit = monitor->getEffectEdit();
    edit->removeCustomControls();
    edit->addCustomButton(KIcon("transform-crop"), i18n("Show previous keyframe"), this, SLOT(slotShowPreviousKeyFrame(bool)), true, KdenliveSettings::onmonitoreffects_geometryshowprevious());
    m_scene = edit->getScene();
    m_scene->cleanup();

    /*
        Setup of timeline and keyframe controls
    */

    ((QGridLayout *)(m_ui.widgetTimeWrapper->layout()))->addWidget(m_timePos, 1, 6);

    QVBoxLayout *layout = new QVBoxLayout(m_ui.frameTimeline);
    m_timeline = new KeyframeHelper(m_ui.frameTimeline);
    layout->addWidget(m_timeline);
    layout->setContentsMargins(0, 0, 0, 0);
    
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    m_ui.buttonPrevious->setIcon(KIcon("media-skip-backward"));
    m_ui.buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    m_ui.buttonPrevious->setIconSize(iconSize);
    m_ui.buttonNext->setIcon(KIcon("media-skip-forward"));
    m_ui.buttonNext->setToolTip(i18n("Go to next keyframe"));
    m_ui.buttonNext->setIconSize(iconSize);
    m_ui.buttonAddDelete->setIcon(KIcon("document-new"));
    m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    m_ui.buttonAddDelete->setIconSize(iconSize);

    m_ui.buttonSync->setIcon(KIcon("insert-link"));
    m_ui.buttonSync->setToolTip(i18n("Synchronize with timeline cursor"));
    m_ui.buttonSync->setChecked(KdenliveSettings::transitionfollowcursor());
    m_ui.buttonSync->setIconSize(iconSize);

    connect(m_timeline, SIGNAL(requestSeek(int)), this, SLOT(slotRequestSeek(int)));
    connect(m_timeline, SIGNAL(keyframeMoved(int)),   this, SLOT(slotKeyframeMoved(int)));
    connect(m_timeline, SIGNAL(addKeyframe(int)),     this, SLOT(slotAddKeyframe(int)));
    connect(m_timeline, SIGNAL(removeKeyframe(int)),  this, SLOT(slotDeleteKeyframe(int)));
    connect(m_timePos, SIGNAL(editingFinished()), this, SLOT(slotPositionChanged()));
    connect(m_ui.buttonPrevious,  SIGNAL(clicked()), this, SLOT(slotPreviousKeyframe()));
    connect(m_ui.buttonNext,      SIGNAL(clicked()), this, SLOT(slotNextKeyframe()));
    connect(m_ui.buttonAddDelete, SIGNAL(clicked()), this, SLOT(slotAddDeleteKeyframe()));
    connect(m_ui.buttonSync,      SIGNAL(toggled(bool)), this, SLOT(slotSetSynchronize(bool)));

    m_spinX = new DragValue(i18nc("x axis position", "X"), 0, 0, -99000, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinX, 0, 0);
    
    m_spinY = new DragValue(i18nc("y axis position", "Y"), 0, 0, -99000, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinY, 0, 1);
    
    m_spinWidth = new DragValue(i18nc("Frame width", "W"), m_monitor->render->frameRenderWidth(), 0, 1, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinWidth, 0, 2);
    
    m_spinHeight = new DragValue(i18nc("Frame height", "H"), m_monitor->render->renderHeight(), 0, 1, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinHeight, 0, 3);

    m_ui.horizontalLayout->setColumnStretch(4, 10);

    QMenu *menu = new QMenu(this);
    QAction *adjustSize = new QAction(i18n("Adjust to original size"), this);
    connect(adjustSize, SIGNAL(triggered()), this, SLOT(slotAdjustToFrameSize()));
    menu->addAction(adjustSize);
    QAction *fitToWidth = new QAction(i18n("Fit to width"), this);
    connect(fitToWidth, SIGNAL(triggered()), this, SLOT(slotFitToWidth()));
    menu->addAction(fitToWidth);
    QAction *fitToHeight = new QAction(i18n("Fit to height"), this);
    connect(fitToHeight, SIGNAL(triggered()), this, SLOT(slotFitToHeight()));
    menu->addAction(fitToHeight);
    menu->addSeparator();
    QAction *importKeyframes = new QAction(i18n("Import keyframes from clip"), this);
    connect(importKeyframes, SIGNAL(triggered()), this, SIGNAL(importClipKeyframes()));
    menu->addAction(importKeyframes);
    menu->addSeparator();
    QAction *alignleft = new QAction(KIcon("kdenlive-align-left"), i18n("Align left"), this);
    connect(alignleft, SIGNAL(triggered()), this, SLOT(slotMoveLeft()));
    menu->addAction(alignleft);
    QAction *alignhcenter = new QAction(KIcon("kdenlive-align-hor"), i18n("Center horizontally"), this);
    connect(alignhcenter, SIGNAL(triggered()), this, SLOT(slotCenterH()));
    menu->addAction(alignhcenter);
    QAction *alignright = new QAction(KIcon("kdenlive-align-right"), i18n("Align right"), this);
    connect(alignright, SIGNAL(triggered()), this, SLOT(slotMoveRight()));
    menu->addAction(alignright);
    QAction *aligntop = new QAction(KIcon("kdenlive-align-top"), i18n("Align top"), this);
    connect(aligntop, SIGNAL(triggered()), this, SLOT(slotMoveTop()));
    menu->addAction(aligntop);
    QAction *alignvcenter = new QAction(KIcon("kdenlive-align-vert"), i18n("Center vertically"), this);
    connect(alignvcenter, SIGNAL(triggered()), this, SLOT(slotCenterV()));
    menu->addAction(alignvcenter);
    QAction *alignbottom = new QAction(KIcon("kdenlive-align-bottom"), i18n("Align bottom"), this);
    connect(alignbottom, SIGNAL(triggered()), this, SLOT(slotMoveBottom()));
    menu->addAction(alignbottom);
    m_ui.buttonOptions->setMenu(menu);

    QHBoxLayout *alignLayout = new QHBoxLayout;
    alignLayout->setSpacing(0);
    QToolButton *alignButton = new QToolButton;
    alignButton->setDefaultAction(alignleft);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignhcenter);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignright);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(aligntop);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignvcenter);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignbottom);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);
    alignLayout->addStretch(10);

    m_ui.horizontalLayout->addLayout(alignLayout, 1, 0, 1, 4);
    //m_ui.horizontalLayout->addStretch(10);
    
    m_spinSize = new DragValue(i18n("Size"), 100, 2, 1, 99000, -1, i18n("%"), false, this);
    m_ui.horizontalLayout2->addWidget(m_spinSize);
    
    m_opacity = new DragValue(i18n("Opacity"), 100, 0, 0, 100, -1, i18n("%"), true, this);
    m_ui.horizontalLayout2->addWidget(m_opacity);


    if (showRotation) {
        m_rotateX = new DragValue(i18n("Rotate X"), 0, 0, -1800, 1800, -1, QString(), true, this);
        m_rotateX->setObjectName("rotate_x");
        m_ui.horizontalLayout3->addWidget(m_rotateX);
        m_rotateY = new DragValue(i18n("Rotate Y"), 0, 0, -1800, 1800,  -1, QString(), true, this);
        m_rotateY->setObjectName("rotate_y");
        m_ui.horizontalLayout3->addWidget(m_rotateY);
        m_rotateZ = new DragValue(i18n("Rotate Z"), 0, 0, -1800, 1800,  -1, QString(), true, this);
        m_rotateZ->setObjectName("rotate_z");
        m_ui.horizontalLayout3->addWidget(m_rotateZ);
        connect(m_rotateX,            SIGNAL(valueChanged(double)), this, SLOT(slotUpdateGeometry()));
        connect(m_rotateY,            SIGNAL(valueChanged(double)), this, SLOT(slotUpdateGeometry()));
        connect(m_rotateZ,            SIGNAL(valueChanged(double)), this, SLOT(slotUpdateGeometry()));
    }
    
    /*
        Setup of geometry controls
    */

    connect(m_spinX,            SIGNAL(valueChanged(double)), this, SLOT(slotSetX(double)));
    connect(m_spinY,            SIGNAL(valueChanged(double)), this, SLOT(slotSetY(double)));
    connect(m_spinWidth,        SIGNAL(valueChanged(double)), this, SLOT(slotSetWidth(double)));
    connect(m_spinHeight,       SIGNAL(valueChanged(double)), this, SLOT(slotSetHeight(double)));

    connect(m_spinSize, SIGNAL(valueChanged(double)), this, SLOT(slotResize(double)));

    connect(m_opacity, SIGNAL(valueChanged(double)), this, SLOT(slotSetOpacity(double)));
    
    /*connect(m_ui.buttonMoveLeft,   SIGNAL(clicked()), this, SLOT(slotMoveLeft()));
    connect(m_ui.buttonCenterH,    SIGNAL(clicked()), this, SLOT(slotCenterH()));
    connect(m_ui.buttonMoveRight,  SIGNAL(clicked()), this, SLOT(slotMoveRight()));
    connect(m_ui.buttonMoveTop,    SIGNAL(clicked()), this, SLOT(slotMoveTop()));
    connect(m_ui.buttonCenterV,    SIGNAL(clicked()), this, SLOT(slotCenterV()));
    connect(m_ui.buttonMoveBottom, SIGNAL(clicked()), this, SLOT(slotMoveBottom()));*/


    /*
        Setup of configuration controls
    */

    connect(m_scene, SIGNAL(addKeyframe()),    this, SLOT(slotAddKeyframe()));
    connect(this, SIGNAL(parameterChanged()), this, SLOT(slotUpdateProperties()));
}

GeometryWidget::~GeometryWidget()
{
    m_scene->setEnabled(true);
    delete m_timePos;
    delete m_timeline;
    delete m_spinX;
    delete m_spinY;
    delete m_spinWidth;
    delete m_spinHeight;
    delete m_opacity;
    m_scene->removeItem(m_rect);
    if (m_rect) delete m_rect;
    if (m_previous) delete m_previous;
    delete m_geometry;
    m_extraGeometryNames.clear();
    m_extraFactors.clear();
    while (!m_extraGeometries.isEmpty()) {
        Mlt::Geometry *g = m_extraGeometries.takeFirst();
        delete g;
    }
}

void GeometryWidget::slotShowPreviousKeyFrame(bool show)
{
    KdenliveSettings::setOnmonitoreffects_geometryshowprevious(show);
    slotPositionChanged(-1, false);
}

void GeometryWidget::updateTimecodeFormat()
{
    m_timePos->slotUpdateTimeCodeFormat();
}

QString GeometryWidget::getValue() const
{
    return m_geometry->serialise();
}

QString GeometryWidget::getExtraValue(const QString &name) const
{
    int ix = m_extraGeometryNames.indexOf(name);
    QString val = m_extraGeometries.at(ix)->serialise();
    if (!val.contains("=")) val = val.section('/', 0, 0);
    else {
        QStringList list = val.split(';', QString::SkipEmptyParts);
        val.clear();
        val.append(list.takeFirst().section('/', 0, 0));
        foreach (const QString &value, list) {
            val.append(';' + value.section('/', 0, 0));
        }
    }
    return val;
}

void GeometryWidget::setupParam(const QDomElement elem, int minframe, int maxframe)
{
    m_inPoint = minframe;
    m_outPoint = maxframe;

    if (m_geometry)
        m_geometry->parse(elem.attribute("value").toUtf8().data(), maxframe - minframe, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    else
        m_geometry = new Mlt::Geometry(elem.attribute("value").toUtf8().data(), maxframe - minframe, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());

    if (elem.attribute("fixed") == "1" || maxframe < minframe) {
        // Keyframes are disabled
        m_ui.widgetTimeWrapper->setHidden(true);
    } else {
        m_ui.widgetTimeWrapper->setHidden(false);
        m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
        m_timeline->update();
        m_timePos->setRange(0, m_outPoint - m_inPoint);
    }

    // no opacity
    if (elem.attribute("opacity") == "false") {
        m_opacity->setHidden(true);
        m_ui.horizontalLayout2->addStretch(2);
    }

    Mlt::GeometryItem item;

    m_geometry->fetch(&item, 0);
    delete m_rect;
    m_rect = new OnMonitorRectItem(QRectF(0, 0, item.w(), item.h()), m_monitor->render->dar());
    m_rect->setPos(item.x(), item.y());
    m_rect->setZValue(0);
    m_scene->addItem(m_rect);
    connect(m_rect, SIGNAL(changed()), this, SLOT(slotUpdateGeometry()));
    m_scene->centerView();
    slotPositionChanged(0, false);
}

void GeometryWidget::addParameter(const QDomElement elem)
{
    Mlt::Geometry *geometry = new Mlt::Geometry(elem.attribute("value").toUtf8().data(), m_outPoint - m_inPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    m_extraGeometries.append(geometry);
    m_timeline->addGeometry(geometry);
    m_extraFactors.append(elem.attribute("factor", "1"));
    m_extraGeometryNames.append(elem.attribute("name"));
    //kDebug()<<"ADDED PARAM: "<<elem.attribute("value");
}

void GeometryWidget::slotSyncPosition(int relTimelinePos)
{
    // do only sync if this effect is keyframable
    if (m_timePos->maximum() > 0 && KdenliveSettings::transitionfollowcursor()) {
        relTimelinePos = qBound(0, relTimelinePos, m_timePos->maximum());
        if (relTimelinePos != m_timePos->getValue())
            slotPositionChanged(relTimelinePos, false);
    }
}

void GeometryWidget::slotRequestSeek(int pos)
{
    if (KdenliveSettings::transitionfollowcursor())
        emit seekToPos(m_clipPos + pos);
}


void GeometryWidget::slotPositionChanged(int pos, bool seek)
{
    if (pos == -1)
        pos = m_timePos->getValue();
    else
        m_timePos->setValue(pos);

    //m_timeline->blockSignals(true);
    m_timeline->setValue(pos);
    //m_timeline->blockSignals(false);

    Mlt::GeometryItem item;
    Mlt::GeometryItem previousItem;
    if (m_geometry->fetch(&item, pos) || item.key() == false) {
        // no keyframe
        m_rect->setEnabled(false);
        m_scene->setEnabled(false);
        m_ui.widgetGeometry->setEnabled(false);
        m_ui.buttonAddDelete->setIcon(KIcon("document-new"));
        m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    } else {
        // keyframe
        m_rect->setEnabled(true);
        m_scene->setEnabled(true);
        m_ui.widgetGeometry->setEnabled(true);
        m_ui.buttonAddDelete->setIcon(KIcon("edit-delete"));
        m_ui.buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    }
    
    if (KdenliveSettings::onmonitoreffects_geometryshowprevious() == false || m_geometry->prev_key(&previousItem, pos - 1) || previousItem.frame() == item.frame()) {
        if (m_previous) {
            m_scene->removeItem(m_previous);
        }
    }
    else if (m_previous && m_previous->scene() && m_previous->data(Qt::UserRole).toInt() == previousItem.frame()) {
        // previous frame already here, do nothing
    }
    else {
        if (m_previous == NULL) {
            m_previous = new QGraphicsRectItem(0, 0, previousItem.w(), previousItem.h());
            m_previous->setBrush(QColor(200, 200, 0, 20));
            m_previous->setPen(QPen(Qt::white, 0, Qt::DotLine));
            m_previous->setPos(previousItem.x(), previousItem.y());
            m_previous->setZValue(-1);
            m_previous->setEnabled(false);
        }
        else {
            m_previous->setPos(previousItem.x(), previousItem.y());
            m_previous->setRect(0, 0, previousItem.w(), previousItem.h());
        }
        m_previous->setData(Qt::UserRole, previousItem.frame());
        if (m_previous->scene() == 0) m_scene->addItem(m_previous);
    }

    m_rect->setPos(item.x(), item.y());
    m_rect->setRect(0, 0, item.w(), item.h());

    m_opacity->blockSignals(true);
    m_opacity->setValue(item.mix());
    m_opacity->blockSignals(false);

    for (int i = 0; i < m_extraGeometries.count(); i++) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        QString name = m_extraGeometryNames.at(i);
        if (!geom->fetch(&item, pos)) {
            DragValue *widget = findChild<DragValue *>(name);
            if (widget) {
                widget->blockSignals(true);
                widget->setValue(item.x() * m_extraFactors.at(i).toInt());
                widget->blockSignals(false);
            }
        }
    }

    slotUpdateProperties();

    if (seek && KdenliveSettings::transitionfollowcursor())
        emit seekToPos(m_clipPos + pos);
}

void GeometryWidget::slotKeyframeMoved(int pos)
{
    slotPositionChanged(pos);
    slotUpdateGeometry();
    QTimer::singleShot(100, this, SIGNAL(parameterChanged()));
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
    item.mix(m_opacity->value());
    m_geometry->insert(item);

    for (int i = 0; i < m_extraGeometries.count(); i++) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        QString name = m_extraGeometryNames.at(i);
        DragValue *widget = findChild<DragValue *>(name);
        if (widget) {
            Mlt::GeometryItem item2;
            item2.frame(pos);
            item2.x((double) widget->value() / m_extraFactors.at(i).toInt());
            geom->insert(item2);
        }
    }
    
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

    for (int i = 0; i < m_extraGeometries.count(); i++) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        geom->remove(pos);
    }

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
    if (!m_geometry->prev_key(&item, currentPos - 1) && item.frame() < currentPos)
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

    for (int i = 0; i < m_extraGeometries.count(); i++) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        QString name = m_extraGeometryNames.at(i);
        Mlt::GeometryItem item2;
        DragValue *widget = findChild<DragValue *>(name);
        if (widget && !geom->next_key(&item2, pos) && item2.frame() == pos) {
            item2.x((double) widget->value() / m_extraFactors.at(i).toInt());
            geom->insert(item2);
        }
    }
    emit parameterChanged();
}

void GeometryWidget::slotUpdateProperties()
{
    QRectF rectSize = m_rect->rect().normalized();
    QPointF rectPos = m_rect->pos();
    double size;
    if (rectSize.width() / m_monitor->render->dar() > rectSize.height())
        size = rectSize.width() * 100.0 / m_monitor->render->frameRenderWidth();
    else
        size = rectSize.height() * 100.0 / m_monitor->render->renderHeight();

    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinSize->blockSignals(true);

    m_spinX->setValue(rectPos.x());
    m_spinY->setValue(rectPos.y());
    m_spinWidth->setValue(rectSize.width());
    m_spinHeight->setValue(rectSize.height());
    m_spinSize->setValue(size);

    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    m_spinSize->blockSignals(false);
}


void GeometryWidget::slotSetX(double value)
{
    m_rect->setPos(value, m_spinY->value());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetY(double value)
{
    m_rect->setPos(m_spinX->value(), value);
    slotUpdateGeometry();
}

void GeometryWidget::slotSetWidth(double value)
{
    m_rect->setRect(0, 0, value, m_spinHeight->value());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetHeight(double value)
{
    m_rect->setRect(0, 0, m_spinWidth->value(), value);
    slotUpdateGeometry();
}

void GeometryWidget::updateMonitorGeometry()
{
    m_rect->setRect(0, 0, m_spinWidth->value(), m_spinHeight->value());
    slotUpdateGeometry();
}


void GeometryWidget::slotResize(double value)
{
    m_rect->setRect(0, 0,
                    (int)((m_monitor->render->frameRenderWidth() * value / 100.0) + 0.5),
                    (int)((m_monitor->render->renderHeight() * value / 100.0) + 0.5));
    slotUpdateGeometry();
}


void GeometryWidget::slotSetOpacity(double value)
{
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
    m_rect->setPos((m_monitor->render->frameRenderWidth() - m_rect->rect().width()) / 2, m_rect->pos().y());
    slotUpdateGeometry();
}

void GeometryWidget::slotMoveRight()
{
    m_rect->setPos(m_monitor->render->frameRenderWidth() - m_rect->rect().width(), m_rect->pos().y());
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
    if (sync)
        emit seekToPos(m_clipPos + m_timePos->getValue());
}

void GeometryWidget::setFrameSize(QPoint size)
{
    m_frameSize = size;
}

void GeometryWidget::slotAdjustToFrameSize()
{
    if (m_frameSize == QPoint() || m_frameSize.x() == 0 || m_frameSize.y() == 0) {
        m_frameSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinWidth->setValue((int) (m_frameSize.x() / m_monitor->render->sar() + 0.5));
    m_spinHeight->setValue(m_frameSize.y());
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    updateMonitorGeometry();
}

void GeometryWidget::slotFitToWidth()
{
    if (m_frameSize == QPoint() || m_frameSize.x() == 0 || m_frameSize.y() == 0) {
        m_frameSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }
    double factor = (double) m_monitor->render->frameRenderWidth() / m_frameSize.x() * m_monitor->render->sar();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue((int) (m_frameSize.y() * factor + 0.5));
    m_spinWidth->setValue(m_monitor->render->frameRenderWidth());
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    updateMonitorGeometry();
}

void GeometryWidget::slotFitToHeight()
{
    if (m_frameSize == QPoint() || m_frameSize.x() == 0 || m_frameSize.y() == 0) {
        m_frameSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }
    double factor = (double) m_monitor->render->renderHeight() / m_frameSize.y();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue(m_monitor->render->renderHeight());
    m_spinWidth->setValue((int) (m_frameSize.x() / m_monitor->render->sar() * factor + 0.5));
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    updateMonitorGeometry();
}

void GeometryWidget::importKeyframes(const QString &data)
{
    QStringList list = data.split(';', QString::SkipEmptyParts);
    QPoint screenSize = m_frameSize;
    if (screenSize == QPoint() || screenSize.x() == 0 || screenSize.y() == 0) {
        screenSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }
    for (int i = 0; i < list.count(); i++) {
	Mlt::GeometryItem item;
	QString geom = list.at(i);
	if (geom.contains('=')) {
	    item.frame(geom.section('=', 0, 0).toInt());
	}
	geom = geom.section('=', 1);
	if (geom.contains('/')) {
	    item.x(geom.section('/', 0, 0).toDouble());
	    item.y(geom.section('/', 1, 1).section(':', 0, 0).toDouble());
	}
	else {
	    item.x(0);
	    item.y(0);
	}
	if (geom.contains('x')) {
	    item.w(geom.section('x', 0, 0).section(':', 1, 1).toDouble());
	    item.h(geom.section('x', 1, 1).section(':', 0, 0).toDouble());
	}
	else {
	    item.w(screenSize.x());
	    item.h(screenSize.y());
	}
	//TODO: opacity
	item.mix(100);
	m_geometry->insert(item);
    }
    emit parameterChanged();
}

#include "geometrywidget.moc"
