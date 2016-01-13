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

#include "effectstack/dragvalue.h"
#include "effectstack/keyframehelper.h"
#include "kdenlivesettings.h"
#include "renderer.h"
#include "timecodedisplay.h"
#include "monitor/monitor.h"
#include "monitor/monitoreditwidget.h"
#include "onmonitoritems/onmonitorrectitem.h"
#include "onmonitoritems/onmonitorpathitem.h"
#include "utils/KoIconUtils.h"

#include "klocalizedstring.h"
#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMenu>


GeometryWidget::GeometryWidget(Monitor* monitor, const Timecode &timecode, int clipPos, bool showRotation, QWidget* parent):
    QWidget(parent),
    m_monitor(monitor),
    m_timePos(new TimecodeDisplay(timecode)),
    m_clipPos(clipPos),
    m_inPoint(0),
    m_outPoint(1),
    m_previous(NULL),
    m_geometry(NULL),
    m_fixedGeom(false),
    m_singleKeyframe(false)
{
    m_ui.setupUi(this);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    connect(m_monitor, SIGNAL(effectChanged(QRect)), this, SLOT(slotUpdateGeometry(QRect)));
    /*MonitorEditWidget *edit = monitor->getEffectEdit();
    edit->removeCustomControls();
    edit->addCustomButton(KoIconUtils::themedIcon("draw-path"), i18n("Show path"), this, SLOT(slotShowPath(bool)), true, KdenliveSettings::onmonitoreffects_geometryshowpath());
    edit->addCustomButton(KoIconUtils::themedIcon("transform-crop"), i18n("Show previous keyframe"), this, SLOT(slotShowPreviousKeyFrame(bool)), true, KdenliveSettings::onmonitoreffects_geometryshowprevious());
    m_scene = edit->getScene();
    m_scene->cleanup();*/

    /*
        Setup of timeline and keyframe controls
    */

    ((QGridLayout *)(m_ui.widgetTimeWrapper->layout()))->addWidget(m_timePos, 1, 5);

    QVBoxLayout *layout = new QVBoxLayout(m_ui.frameTimeline);
    m_timeline = new KeyframeHelper(m_ui.frameTimeline);
    layout->addWidget(m_timeline);
    layout->setContentsMargins(0, 0, 0, 0);
    
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    m_ui.buttonPrevious->setIcon(KoIconUtils::themedIcon("media-skip-backward"));
    m_ui.buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    m_ui.buttonPrevious->setIconSize(iconSize);
    m_ui.buttonNext->setIcon(KoIconUtils::themedIcon("media-skip-forward"));
    m_ui.buttonNext->setToolTip(i18n("Go to next keyframe"));
    m_ui.buttonNext->setIconSize(iconSize);
    m_ui.buttonAddDelete->setIcon(KoIconUtils::themedIcon("list-add"));
    m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    m_ui.buttonAddDelete->setIconSize(iconSize);

    connect(m_timeline, SIGNAL(requestSeek(int)), this, SLOT(slotRequestSeek(int)));
    connect(m_timeline, SIGNAL(keyframeMoved(int)),   this, SLOT(slotKeyframeMoved(int)));
    connect(m_timeline, SIGNAL(addKeyframe(int)),     this, SLOT(slotAddKeyframe(int)));
    connect(m_timeline, SIGNAL(removeKeyframe(int)),  this, SLOT(slotDeleteKeyframe(int)));
    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotPositionChanged()));
    connect(m_ui.buttonPrevious,  SIGNAL(clicked()), this, SLOT(slotPreviousKeyframe()));
    connect(m_ui.buttonNext,      SIGNAL(clicked()), this, SLOT(slotNextKeyframe()));
    connect(m_ui.buttonAddDelete, SIGNAL(clicked()), this, SLOT(slotAddDeleteKeyframe()));

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
    QAction *adjustSize = new QAction(KoIconUtils::themedIcon("zoom-fit-best"), i18n("Adjust to original size"), this);
    connect(adjustSize, SIGNAL(triggered()), this, SLOT(slotAdjustToFrameSize()));
    QAction *fitToWidth = new QAction(KoIconUtils::themedIcon("zoom-fit-width"), i18n("Fit to width"), this);
    connect(fitToWidth, SIGNAL(triggered()), this, SLOT(slotFitToWidth()));
    QAction *fitToHeight = new QAction(KoIconUtils::themedIcon("zoom-fit-height"), i18n("Fit to height"), this);
    connect(fitToHeight, SIGNAL(triggered()), this, SLOT(slotFitToHeight()));

    QAction *importKeyframes = new QAction(i18n("Import keyframes from clip"), this);
    connect(importKeyframes, SIGNAL(triggered()), this, SIGNAL(importClipKeyframes()));
    menu->addAction(importKeyframes);
    QAction *resetKeyframes = new QAction(i18n("Reset all keyframes"), this);
    connect(resetKeyframes, SIGNAL(triggered()), this, SLOT(slotResetKeyframes()));
    menu->addAction(resetKeyframes);

    QAction *resetNextKeyframes = new QAction(i18n("Reset keyframes after cursor"), this);
    connect(resetNextKeyframes, SIGNAL(triggered()), this, SLOT(slotResetNextKeyframes()));
    menu->addAction(resetNextKeyframes);
    QAction *resetPreviousKeyframes = new QAction(i18n("Reset keyframes before cursor"), this);
    connect(resetPreviousKeyframes, SIGNAL(triggered()), this, SLOT(slotResetPreviousKeyframes()));
    menu->addAction(resetPreviousKeyframes);
    menu->addSeparator();

    QAction *syncTimeline = new QAction(KoIconUtils::themedIcon("edit-link"), i18n("Synchronize with timeline cursor"), this);
    syncTimeline->setCheckable(true);
    syncTimeline->setChecked(KdenliveSettings::transitionfollowcursor());
    connect(syncTimeline, SIGNAL(toggled(bool)), this, SLOT(slotSetSynchronize(bool)));
    menu->addAction(syncTimeline);

    QAction *alignleft = new QAction(KoIconUtils::themedIcon("kdenlive-align-left"), i18n("Align left"), this);
    connect(alignleft, SIGNAL(triggered()), this, SLOT(slotMoveLeft()));
    QAction *alignhcenter = new QAction(KoIconUtils::themedIcon("kdenlive-align-hor"), i18n("Center horizontally"), this);
    connect(alignhcenter, SIGNAL(triggered()), this, SLOT(slotCenterH()));
    QAction *alignright = new QAction(KoIconUtils::themedIcon("kdenlive-align-right"), i18n("Align right"), this);
    connect(alignright, SIGNAL(triggered()), this, SLOT(slotMoveRight()));
    QAction *aligntop = new QAction(KoIconUtils::themedIcon("kdenlive-align-top"), i18n("Align top"), this);
    connect(aligntop, SIGNAL(triggered()), this, SLOT(slotMoveTop()));
    QAction *alignvcenter = new QAction(KoIconUtils::themedIcon("kdenlive-align-vert"), i18n("Center vertically"), this);
    connect(alignvcenter, SIGNAL(triggered()), this, SLOT(slotCenterV()));
    QAction *alignbottom = new QAction(KoIconUtils::themedIcon("kdenlive-align-bottom"), i18n("Align bottom"), this);
    connect(alignbottom, SIGNAL(triggered()), this, SLOT(slotMoveBottom()));
    
    m_ui.buttonOptions->setMenu(menu);
    m_ui.buttonOptions->setIcon(KoIconUtils::themedIcon("configure"));
    m_ui.buttonOptions->setToolTip(i18n("Options"));
    m_ui.buttonOptions->setIconSize(iconSize);

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

    alignButton = new QToolButton;
    alignButton->setDefaultAction(adjustSize);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(fitToWidth);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);
    
    alignButton = new QToolButton;
    alignButton->setDefaultAction(fitToHeight);
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

    connect(m_monitor, SIGNAL(addKeyframe()), this, SLOT(slotAddKeyframe()));
    connect(m_monitor, SIGNAL(seekToKeyframe(int)), this, SLOT(slotSeekToKeyframe(int)));
    connect(this, SIGNAL(parameterChanged()), this, SLOT(slotUpdateProperties()));
}

GeometryWidget::~GeometryWidget()
{
    delete m_timePos;
    delete m_timeline;
    delete m_spinX;
    delete m_spinY;
    delete m_spinWidth;
    delete m_spinHeight;
    delete m_opacity;
    delete m_previous;
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

void GeometryWidget::slotShowPath(bool show)
{
    KdenliveSettings::setOnmonitoreffects_geometryshowpath(show);
    slotPositionChanged(-1, false);
}

void GeometryWidget::updateTimecodeFormat()
{
    m_timePos->slotUpdateTimeCodeFormat();
}

QString GeometryWidget::getValue() const
{
    QString result = m_geometry->serialise();
    if (!m_fixedGeom && result.contains(";") && !result.section(";",0,0).contains("=")) {
        result.prepend("0=");
    }
    return result;
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

void GeometryWidget::setupParam(const QDomElement &elem, int minframe, int maxframe)
{
    m_inPoint = minframe;
    m_outPoint = maxframe;
    if (m_geometry)
        m_geometry->parse(elem.attribute("value").toUtf8().data(), maxframe - minframe, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    else
        m_geometry = new Mlt::Geometry(elem.attribute("value").toUtf8().data(), maxframe - minframe, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());

    if (elem.attribute("fixed") == "1" || maxframe < minframe) {
        // Keyframes are disabled
        m_fixedGeom = true;
        m_ui.widgetTimeWrapper->setHidden(true);
    } else {
        m_fixedGeom = false;
        m_ui.widgetTimeWrapper->setHidden(false);
        m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
    }
    m_timePos->setRange(0, m_outPoint - m_inPoint);

    // no opacity
    if (elem.attribute("opacity") == "false") {
        m_opacity->setHidden(true);
        m_ui.horizontalLayout2->addStretch(2);
    }

    Mlt::GeometryItem item;
    int framePos = qBound<int>(0, m_monitor->render->seekFramePosition() - m_clipPos, maxframe - minframe);
    m_geometry->fetch(&item, framePos);
    checkSingleKeyframe();
    m_monitor->slotShowEffectScene(MonitorSceneGeometry);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    slotPositionChanged(framePos, false);
}

void GeometryWidget::addParameter(const QDomElement &elem)
{
    Mlt::Geometry *geometry = new Mlt::Geometry(elem.attribute("value").toUtf8().data(), m_outPoint - m_inPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    m_extraGeometries.append(geometry);
    m_timeline->addGeometry(geometry);
    m_extraFactors.append(elem.attribute("factor", "1"));
    m_extraGeometryNames.append(elem.attribute("name"));
}

void GeometryWidget::slotSyncPosition(int relTimelinePos)
{
    // do only sync if this effect is keyframable
    if (m_timePos->maximum() > 0 && KdenliveSettings::transitionfollowcursor()) {
        relTimelinePos = qBound(0, relTimelinePos, m_timePos->maximum());
        slotPositionChanged(relTimelinePos, false);
        /*if (relTimelinePos != m_timePos->getValue()) {
            slotPositionChanged(relTimelinePos, false);
        }*/
    }
}

int GeometryWidget::currentPosition() const
{
    return m_inPoint + m_timePos->getValue();
}

void GeometryWidget::slotRequestSeek(int pos)
{
    if (KdenliveSettings::transitionfollowcursor())
        emit seekToPos(pos);
}

void GeometryWidget::checkSingleKeyframe()
{
    if (!m_geometry) return;
    QString serial = m_geometry->serialise();
    m_singleKeyframe = !serial.contains(";");
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
    if (!m_fixedGeom && (m_geometry->fetch(&item, pos) || item.key() == false)) {
        // no keyframe
        if (m_singleKeyframe) {
            // Special case: only one keyframe, allow adjusting whatever the position is
            m_monitor->setEffectKeyframe(true);
            m_ui.widgetGeometry->setEnabled(true);
        } else {
            m_monitor->setEffectKeyframe(false);
            m_ui.widgetGeometry->setEnabled(false);
        }
        m_ui.buttonAddDelete->setIcon(KoIconUtils::themedIcon("list-add"));
        m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    } else {
        // keyframe
        m_monitor->setEffectKeyframe(true);
        m_ui.widgetGeometry->setEnabled(true);
        m_ui.buttonAddDelete->setIcon(KoIconUtils::themedIcon("list-remove"));
        m_ui.buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    }
    m_opacity->blockSignals(true);
    m_opacity->setValue(item.mix());
    m_opacity->blockSignals(false);

    for (int i = 0; i < m_extraGeometries.count(); ++i) {
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
    m_geometry->fetch(&item, pos);
    QRect r((int) item.x(), (int) item.y(), (int) item.w(), (int) item.h());
    m_monitor->setUpEffectGeometry(r, calculateCenters());
    slotUpdateProperties();

    if (seek && KdenliveSettings::transitionfollowcursor())
        emit seekToPos(pos);
}

void GeometryWidget::slotInitScene(int pos)
{
    slotPositionChanged(pos, false);
}

void GeometryWidget::slotKeyframeMoved(int pos)
{
    slotPositionChanged(pos);
    slotUpdateGeometry();
    QTimer::singleShot(100, this, SIGNAL(parameterChanged()));
}

void GeometryWidget::slotSeekToKeyframe(int index)
{
    // "fixed" effect: don't allow keyframe (FIXME: find a better way to access this property!)
    Mlt::GeometryItem item;
    int pos = 0;
    int ix = 0;
    while (!m_geometry->next_key(&item, pos) && ix < index) {
        ix++;
        pos = item.frame() + 1;
    }
    slotPositionChanged(item.frame(), true);
}


void GeometryWidget::slotAddKeyframe(int pos)
{
    // "fixed" effect: don't allow keyframe (FIXME: find a better way to access this property!)
    if (m_ui.widgetTimeWrapper->isHidden())
        return;
    Mlt::GeometryItem item;
    if (pos == -1)
        pos = m_timeline->value(); // m_timePos->getValue();
    item.frame(pos);
    QRect rect = m_monitor->effectRect().normalized();
    item.x(rect.x());
    item.y(rect.y());
    item.w(rect.width());
    item.h(rect.height());
    item.mix(m_opacity->value());
    m_geometry->insert(item);

    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        QString name = m_extraGeometryNames.at(i);
        DragValue *widget = findChild<DragValue *>(name);
        if (widget) {
            Mlt::GeometryItem item2;
            item2.frame(pos);
            item2.x((double) widget->value() / m_extraFactors.at(i).toInt());
            item2.y(0);
            geom->insert(item2);
        }
    }
    checkSingleKeyframe();
    m_timeline->update();
    slotPositionChanged(pos, false);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
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

    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        geom->remove(pos);
    }

    m_timeline->update();
    m_geometry->fetch(&item, pos);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    checkSingleKeyframe();
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
    int pos = 0;
    if (m_singleKeyframe) {
        if (m_geometry->next_key(&item, pos)) {
            // No keyframe found, abort
            return;
        }
        pos = item.frame();
    } else {
        if (!m_fixedGeom) {
            pos = m_timePos->getValue();
        }
        // get keyframe and make sure it is the correct one
        if (m_geometry->next_key(&item, pos) || item.frame() != pos) {
            return;
        }
    }

    QRectF rect = m_monitor->effectRect().normalized();
    item.x(rect.x());
    item.y(rect.y());
    item.w(rect.width());
    item.h(rect.height());
    m_geometry->insert(item);

    for (int i = 0; i < m_extraGeometries.count(); ++i) {
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

void GeometryWidget::slotUpdateGeometry(const QRect r)
{
    Mlt::GeometryItem item;
    int pos = 0;
    if (m_singleKeyframe) {
        if (m_geometry->next_key(&item, pos)) {
            // No keyframe found, abort
            return;
        }
        pos = item.frame();
    } else {
        if (!m_fixedGeom) {
            pos = m_timePos->getValue();
        }
        // get keyframe and make sure it is the correct one
        if (m_geometry->next_key(&item, pos) || item.frame() != pos) {
            return;
        }
    }

    QRectF rectSize = r.normalized();
    QPointF rectPos = r.topLeft();
    item.x(rectPos.x());
    item.y(rectPos.y());
    item.w(rectSize.width());
    item.h(rectSize.height());
    m_geometry->insert(item);

    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        QString name = m_extraGeometryNames.at(i);
        Mlt::GeometryItem item2;
        DragValue *widget = findChild<DragValue *>(name);
        if (widget && !geom->next_key(&item2, pos) && item2.frame() == pos) {
            item2.x((double) widget->value() / m_extraFactors.at(i).toInt());
            geom->insert(item2);
        }
    }
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
    emit parameterChanged();
}

void GeometryWidget::slotUpdateProperties()
{
    QRect rect = m_monitor->effectRect().normalized();
    double size;
    if (rect.width() / m_monitor->render->dar() > rect.height())
        size = rect.width() * 100.0 / m_monitor->render->frameRenderWidth();
    else
        size = rect.height() * 100.0 / m_monitor->render->renderHeight();

    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinSize->blockSignals(true);

    m_spinX->setValue(rect.x());
    m_spinY->setValue(rect.y());
    m_spinWidth->setValue(rect.width());
    m_spinHeight->setValue(rect.height());
    m_spinSize->setValue(size);

    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    m_spinSize->blockSignals(false);
}


QVariantList GeometryWidget::calculateCenters()
{
    QVariantList points;
    Mlt::GeometryItem item;
    int pos = 0;
    while (!m_geometry->next_key(&item, pos)) {
        QRect r(item.x(), item.y(), item.w(), item.h());
        points.append(QVariant(r.center()));
        pos = item.frame() + 1;
    }
    return points;
}

void GeometryWidget::slotSetX(double value)
{
    m_monitor->setUpEffectGeometry(QRect(value, m_spinY->value(), m_spinWidth->value(), m_spinHeight->value()), calculateCenters());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetY(double value)
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), value, m_spinWidth->value(), m_spinHeight->value()), calculateCenters());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetWidth(double value)
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), value, m_spinHeight->value()), calculateCenters());
    slotUpdateGeometry();
}

void GeometryWidget::slotSetHeight(double value)
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), value), calculateCenters());
    slotUpdateGeometry();
}

void GeometryWidget::updateMonitorGeometry()
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value()), calculateCenters());
    slotUpdateGeometry();
}


void GeometryWidget::slotResize(double value)
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), (int)((m_monitor->render->frameRenderWidth() * value / 100.0) + 0.5), (int)((m_monitor->render->renderHeight() * value / 100.0) + 0.5)), calculateCenters());
    slotUpdateGeometry();
}


void GeometryWidget::slotSetOpacity(double value)
{
    int pos = m_timePos->getValue();
    Mlt::GeometryItem item;
    if (m_geometry->fetch(&item, pos) || item.key() == false) {
        // Check if we are in a "one keyframe" situation
        if (m_singleKeyframe && !m_geometry->next_key(&item, 0)) {
            // Ok we have only one keyframe, edit this one
        } else {
            //TODO: show error message
            return;
        }
    }
    item.mix(value);
    m_geometry->insert(item);
    emit parameterChanged();
}


void GeometryWidget::slotMoveLeft()
{
    m_spinX->setValue(0);
}

void GeometryWidget::slotCenterH()
{
    m_spinX->setValue((m_monitor->render->frameRenderWidth() - m_spinWidth->value()) / 2);
}

void GeometryWidget::slotMoveRight()
{
    m_spinX->setValue(m_monitor->render->frameRenderWidth() - m_spinWidth->value());
}

void GeometryWidget::slotMoveTop()
{
    m_spinY->setValue(0);
}

void GeometryWidget::slotCenterV()
{
    m_spinY->setValue((m_monitor->render->renderHeight() - m_spinHeight->value()) / 2);
}

void GeometryWidget::slotMoveBottom()
{
    m_spinY->setValue(m_monitor->render->renderHeight() - m_spinHeight->value());
}


void GeometryWidget::slotSetSynchronize(bool sync)
{
    KdenliveSettings::setTransitionfollowcursor(sync);
    if (sync)
        emit seekToPos(m_timePos->getValue());
}

void GeometryWidget::setFrameSize(const QPoint &size)
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

void GeometryWidget::slotResetKeyframes()
{
    // Delete existing keyframes
    Mlt::GeometryItem item;
    while (!m_geometry->next_key(&item, 1)) {
        m_geometry->remove(item.frame());
    }

    // Delete extra geometry keyframes too
    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        while (!geom->next_key(&item, 1)) {
            geom->remove(item.frame());
        }
    }

    // Create neutral first keyframe
    item.frame(0);
    item.x(0);
    item.y(0);
    item.w(m_monitor->render->frameRenderWidth());
    item.h(m_monitor->render->renderHeight());
    item.mix(100);
    m_geometry->insert(item);
    m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    slotPositionChanged(-1, false);
    emit parameterChanged();
}

void GeometryWidget::slotResetNextKeyframes()
{
    // Delete keyframes after cursor pos
    Mlt::GeometryItem item;
    int pos = m_timePos->getValue();
    while (!m_geometry->next_key(&item, pos)) {
        m_geometry->remove(item.frame());
    }

    // Delete extra geometry keyframes too
    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        while (!geom->next_key(&item, pos)) {
            geom->remove(item.frame());
        }
    }

    // Make sure we have at least one keyframe
    if (m_geometry->next_key(&item, 0)) {
        item.frame(0);
        item.x(0);
        item.y(0);
        item.w(m_monitor->render->frameRenderWidth());
        item.h(m_monitor->render->renderHeight());
        item.mix(100);
        m_geometry->insert(item);
    }
    m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    slotPositionChanged(-1, false);
    emit parameterChanged();
}

void GeometryWidget::slotResetPreviousKeyframes()
{
    // Delete keyframes before cursor pos
    Mlt::GeometryItem item;
    int pos = 0;
    while (!m_geometry->next_key(&item, pos) && pos < m_timePos->getValue()) {
        pos = item.frame() + 1;
        m_geometry->remove(item.frame());
    }

    // Delete extra geometry keyframes too
    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        pos = 0;
        while (!geom->next_key(&item, pos) && pos < m_timePos->getValue()) {
            pos = item.frame() + 1;
            geom->remove(item.frame());
        }
    }

    // Make sure we have at least one keyframe
    if (!m_geometry->next_key(&item, 0)) {
        item.frame(0);
        /*item.x(0);
    item.y(0);
    item.w(m_monitor->render->frameRenderWidth());
    item.h(m_monitor->render->renderHeight());
    item.mix(100);*/
        m_geometry->insert(item);
    }
    else {
        item.frame(0);
        item.x(0);
        item.y(0);
        item.w(m_monitor->render->frameRenderWidth());
        item.h(m_monitor->render->renderHeight());
        item.mix(100);
        m_geometry->insert(item);
    }
    m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    slotPositionChanged(-1, false);
    emit parameterChanged();
}

void GeometryWidget::importKeyframes(const QString &data, int maximum)
{
    QStringList list = data.split(';', QString::SkipEmptyParts);
    if (list.isEmpty()) return;
    QPoint screenSize = m_frameSize;
    if (screenSize == QPoint() || screenSize.x() == 0 || screenSize.y() == 0) {
        screenSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }
    // Clear existing keyframes
    Mlt::GeometryItem item;
    
    while (!m_geometry->next_key(&item, 0)) {
        m_geometry->remove(item.frame());
    }
    
    int offset = 1;
    if (maximum > 0 && list.count() > maximum) {
        offset = list.count() / maximum;
    }
    for (int i = 0; i < list.count(); i += offset) {
        QString geom = list.at(i);
        if (geom.contains('=')) {
            item.frame(geom.section('=', 0, 0).toInt());
            geom = geom.section('=', 1);
        }
        else item.frame(0);
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
    m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    slotPositionChanged(-1, false);
    emit parameterChanged();
}

void GeometryWidget::slotUpdateRange(int inPoint, int outPoint)
{
    m_inPoint = inPoint;
    m_outPoint = outPoint;
    m_timeline->setKeyGeometry(m_geometry, m_outPoint - m_inPoint);
    m_timePos->setRange(0, m_outPoint - m_inPoint);
}


