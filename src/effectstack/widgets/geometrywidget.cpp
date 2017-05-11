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
#include "effectstack/parametercontainer.h"
#include "onmonitoritems/onmonitorrectitem.h"
#include "onmonitoritems/onmonitorpathitem.h"
#include "utils/KoIconUtils.h"

#include "klocalizedstring.h"
#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QMenu>

GeometryWidget::GeometryWidget(EffectMetaInfo *info, int clipPos, bool showRotation, bool useOffset, QWidget *parent):
    AbstractParamWidget(parent),
    m_monitor(info->monitor),
    m_timePos(new TimecodeDisplay(info->monitor->timecode())),
    m_clipPos(clipPos),
    m_inPoint(0),
    m_outPoint(1),
    m_previous(nullptr),
    m_geometry(nullptr),
    m_frameSize(info->frameSize),
    m_fixedGeom(false),
    m_singleKeyframe(false),
    m_useOffset(useOffset)
{
    m_ui.setupUi(this);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    /*
        Setup of timeline and keyframe controls
    */
    m_originalSize = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-original")), i18n("Adjust to original size"), this);
    connect(m_originalSize, &QAction::triggered, this, &GeometryWidget::slotAdjustToSource);
    m_originalSize->setCheckable(true);
    if (m_frameSize == QPoint() || m_frameSize.x() == 0 || m_frameSize.y() == 0) {
        m_originalSize->setEnabled(false);
        m_frameSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }
    ((QGridLayout *)(m_ui.widgetTimeWrapper->layout()))->addWidget(m_timePos, 1, 5);

    QVBoxLayout *layout = new QVBoxLayout(m_ui.frameTimeline);
    m_timeline = new KeyframeHelper(m_ui.frameTimeline);
    layout->addWidget(m_timeline);
    layout->setContentsMargins(0, 0, 0, 0);

    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    m_ui.buttonPrevious->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-skip-backward")));
    m_ui.buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    m_ui.buttonPrevious->setIconSize(iconSize);
    m_ui.buttonNext->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-skip-forward")));
    m_ui.buttonNext->setToolTip(i18n("Go to next keyframe"));
    m_ui.buttonNext->setIconSize(iconSize);
    m_ui.buttonAddDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
    m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    m_ui.buttonAddDelete->setIconSize(iconSize);

    connect(m_timeline, &KeyframeHelper::requestSeek, this, &GeometryWidget::slotRequestSeek);
    connect(m_timeline, &KeyframeHelper::keyframeMoved,   this, &GeometryWidget::slotKeyframeMoved);
    connect(m_timeline, &KeyframeHelper::addKeyframe,     this, &GeometryWidget::slotAddKeyframe);
    connect(m_timeline, &KeyframeHelper::removeKeyframe,  this, &GeometryWidget::slotDeleteKeyframe);
    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotPositionChanged()));
    connect(m_ui.buttonPrevious,  &QAbstractButton::clicked, this, &GeometryWidget::slotPreviousKeyframe);
    connect(m_ui.buttonNext,      &QAbstractButton::clicked, this, &GeometryWidget::slotNextKeyframe);
    connect(m_ui.buttonAddDelete, &QAbstractButton::clicked, this, &GeometryWidget::slotAddDeleteKeyframe);

    m_spinX = new DragValue(i18nc("x axis position", "X"), 0, 0, -99000, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinX, 0, 0);

    m_spinY = new DragValue(i18nc("y axis position", "Y"), 0, 0, -99000, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinY, 0, 1);

    m_spinWidth = new DragValue(i18nc("Frame width", "W"), m_monitor->render->frameRenderWidth(), 0, 1, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinWidth, 0, 2);

    // Lock ratio stuff
    m_lockRatio = new QAction(KoIconUtils::themedIcon(QStringLiteral("link")), i18n("Lock aspect ratio"), this);
    m_lockRatio->setCheckable(true);
    connect(m_lockRatio, &QAction::triggered, this, &GeometryWidget::slotLockRatio);
    QToolButton *ratioButton = new QToolButton;
    ratioButton->setDefaultAction(m_lockRatio);
    m_ui.horizontalLayout->addWidget(ratioButton, 0, 3);

    m_spinHeight = new DragValue(i18nc("Frame height", "H"), m_monitor->render->renderHeight(), 0, 1, 99000, -1, QString(), false, this);
    m_ui.horizontalLayout->addWidget(m_spinHeight, 0, 4);

    m_ui.horizontalLayout->setColumnStretch(5, 10);

    QMenu *menu = new QMenu(this);
    QAction *adjustSize = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-best")), i18n("Adjust and center in frame"), this);
    connect(adjustSize, &QAction::triggered, this, &GeometryWidget::slotAdjustToFrameSize);
    QAction *fitToWidth = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-width")), i18n("Fit to width"), this);
    connect(fitToWidth, &QAction::triggered, this, &GeometryWidget::slotFitToWidth);
    QAction *fitToHeight = new QAction(KoIconUtils::themedIcon(QStringLiteral("zoom-fit-height")), i18n("Fit to height"), this);
    connect(fitToHeight, &QAction::triggered, this, &GeometryWidget::slotFitToHeight);

    QAction *importKeyframes = new QAction(i18n("Import keyframes from clip"), this);
    connect(importKeyframes, &QAction::triggered, this, &GeometryWidget::importClipKeyframes);
    menu->addAction(importKeyframes);
    QAction *resetKeyframes = new QAction(i18n("Reset all keyframes"), this);
    connect(resetKeyframes, &QAction::triggered, this, &GeometryWidget::slotResetKeyframes);
    menu->addAction(resetKeyframes);

    QAction *resetNextKeyframes = new QAction(i18n("Reset keyframes after cursor"), this);
    connect(resetNextKeyframes, &QAction::triggered, this, &GeometryWidget::slotResetNextKeyframes);
    menu->addAction(resetNextKeyframes);
    QAction *resetPreviousKeyframes = new QAction(i18n("Reset keyframes before cursor"), this);
    connect(resetPreviousKeyframes, &QAction::triggered, this, &GeometryWidget::slotResetPreviousKeyframes);
    menu->addAction(resetPreviousKeyframes);
    menu->addSeparator();

    QAction *syncTimeline = new QAction(KoIconUtils::themedIcon(QStringLiteral("edit-link")), i18n("Synchronize with timeline cursor"), this);
    syncTimeline->setCheckable(true);
    syncTimeline->setChecked(KdenliveSettings::transitionfollowcursor());
    connect(syncTimeline, &QAction::toggled, this, &GeometryWidget::slotSetSynchronize);
    menu->addAction(syncTimeline);

    QAction *alignleft = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-left")), i18n("Align left"), this);
    connect(alignleft, &QAction::triggered, this, &GeometryWidget::slotMoveLeft);
    QAction *alignhcenter = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-hor")), i18n("Center horizontally"), this);
    connect(alignhcenter, &QAction::triggered, this, &GeometryWidget::slotCenterH);
    QAction *alignright = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-right")), i18n("Align right"), this);
    connect(alignright, &QAction::triggered, this, &GeometryWidget::slotMoveRight);
    QAction *aligntop = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-top")), i18n("Align top"), this);
    connect(aligntop, &QAction::triggered, this, &GeometryWidget::slotMoveTop);
    QAction *alignvcenter = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-vert")), i18n("Center vertically"), this);
    connect(alignvcenter, &QAction::triggered, this, &GeometryWidget::slotCenterV);
    QAction *alignbottom = new QAction(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-bottom")), i18n("Align bottom"), this);
    connect(alignbottom, &QAction::triggered, this, &GeometryWidget::slotMoveBottom);

    m_ui.buttonOptions->setMenu(menu);
    m_ui.buttonOptions->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
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
    alignButton->setDefaultAction(m_originalSize);
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
    m_spinSize->setStep(10);
    m_ui.horizontalLayout2->addWidget(m_spinSize);

    m_opacity = new DragValue(i18n("Opacity"), 100, 0, 0, 100, -1, i18n("%"), true, this);
    m_ui.horizontalLayout2->addWidget(m_opacity);

    if (showRotation) {
        m_rotateX = new DragValue(i18n("Rotate X"), 0, 0, -1800, 1800, -1, QString(), true, this);
        m_rotateX->setObjectName(QStringLiteral("rotate_x"));
        m_ui.horizontalLayout3->addWidget(m_rotateX);
        m_rotateY = new DragValue(i18n("Rotate Y"), 0, 0, -1800, 1800,  -1, QString(), true, this);
        m_rotateY->setObjectName(QStringLiteral("rotate_y"));
        m_ui.horizontalLayout3->addWidget(m_rotateY);
        m_rotateZ = new DragValue(i18n("Rotate Z"), 0, 0, -1800, 1800,  -1, QString(), true, this);
        m_rotateZ->setObjectName(QStringLiteral("rotate_z"));
        m_ui.horizontalLayout3->addWidget(m_rotateZ);
        connect(m_rotateX,            &DragValue::valueChanged, this, &GeometryWidget::slotUpdateGeometry);
        connect(m_rotateY,            &DragValue::valueChanged, this, &GeometryWidget::slotUpdateGeometry);
        connect(m_rotateZ,            &DragValue::valueChanged, this, &GeometryWidget::slotUpdateGeometry);
    }

    /*
        Setup of geometry controls
    */

    connect(m_spinX,            &DragValue::valueChanged, this, &GeometryWidget::slotSetX);
    connect(m_spinY,            &DragValue::valueChanged, this, &GeometryWidget::slotSetY);
    connect(m_spinWidth,        &DragValue::valueChanged, this, &GeometryWidget::slotSetWidth);
    connect(m_spinHeight,       &DragValue::valueChanged, this, &GeometryWidget::slotSetHeight);

    connect(m_spinSize, &DragValue::valueChanged, this, &GeometryWidget::slotResize);

    connect(m_opacity, &DragValue::valueChanged, this, &GeometryWidget::slotSetOpacity);

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
    connect(m_monitor, &Monitor::seekToKeyframe, this, &GeometryWidget::slotSeekToKeyframe);
    connect(this, SIGNAL(valueChanged()), this, SLOT(slotUpdateProperties()));
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

void GeometryWidget::connectMonitor(bool activate)
{
    if (activate) {
        connect(m_monitor, &Monitor::effectChanged, this, &GeometryWidget::slotUpdateGeometryRect);
        connect(m_monitor, &Monitor::effectPointsChanged, this, &GeometryWidget::slotUpdateCenters, Qt::UniqueConnection);
    } else {
        disconnect(m_monitor, &Monitor::effectChanged, this, &GeometryWidget::slotUpdateGeometryRect);
        disconnect(m_monitor, &Monitor::effectPointsChanged, this, &GeometryWidget::slotUpdateCenters);
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
    if (m_fixedGeom) {
        return QStringLiteral("%1 %2 %3 %4").arg(m_spinX->value()).arg(m_spinY->value()).arg(m_spinWidth->value()).arg(m_spinHeight->value());
    }
    QString result = m_geometry->serialise();
    if (result.contains(QLatin1Char(';')) && !result.section(QLatin1Char(';'), 0, 0).contains(QLatin1Char('='))) {
        result.prepend(QStringLiteral("0="));
    }
    return result;
}

QString GeometryWidget::offsetAnimation(int offset, bool useOffset)
{
    Mlt::Geometry *geometry = new Mlt::Geometry((char *)nullptr, m_outPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    Mlt::GeometryItem item;
    int pos = 0;
    int ix = 0;
    while (!m_geometry->next_key(&item, pos)) {
        pos = item.frame() + 1;
        item.frame(item.frame() + offset);
        geometry->insert(item);
        ix++;
    }
    m_useOffset = useOffset;
    QString result = geometry->serialise();
    if (!m_fixedGeom && result.contains(QLatin1Char(';')) && !result.section(QLatin1Char(';'), 0, 0).contains(QLatin1Char('='))) {
        result.prepend(QStringLiteral("0="));
    }
    m_geometry->parse(result.toUtf8().data(), m_outPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    delete geometry;
    return result;
}

void GeometryWidget::reload(const QString &tag, const QString &data)
{
    if (m_extraGeometryNames.contains(tag)) {
        int ix = m_extraGeometryNames.indexOf(tag);
        Mlt::Geometry *geom = m_extraGeometries.at(ix);
        if (geom) {
            geom->parse(data.toUtf8().data(), m_outPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
            m_timeline->setKeyGeometry(geom, m_inPoint, m_outPoint, m_useOffset);
        }
    } else {
        m_geometry->parse(data.toUtf8().data(), m_outPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
        m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    }
}

QString GeometryWidget::getExtraValue(const QString &name) const
{
    int ix = m_extraGeometryNames.indexOf(name);
    QString val = m_extraGeometries.at(ix)->serialise();
    if (!val.contains(QLatin1Char('='))) {
        val = val.section(QLatin1Char('/'), 0, 0);
    } else {
        QStringList list = val.split(QLatin1Char(';'), QString::SkipEmptyParts);
        val.clear();
        val.append(list.takeFirst().section(QLatin1Char('/'), 0, 0));
        foreach (const QString &value, list) {
            val.append(QLatin1Char(';') + value.section(QLatin1Char('/'), 0, 0));
        }
    }
    return val;
}

void GeometryWidget::setupParam(const QDomElement &elem, int minframe, int maxframe)
{
    m_inPoint = minframe;
    m_outPoint = maxframe;
    if (m_geometry) {
        m_geometry->parse(elem.attribute(QStringLiteral("value")).toUtf8().data(), maxframe - minframe, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    } else {
        m_geometry = new Mlt::Geometry(elem.attribute(QStringLiteral("value")).toUtf8().data(), maxframe - minframe, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    }

    if (elem.attribute(QStringLiteral("fixed")) == QLatin1String("1") || maxframe < minframe) {
        // Keyframes are disabled
        m_fixedGeom = true;
        m_ui.widgetTimeWrapper->setHidden(true);
    } else {
        m_fixedGeom = false;
        m_ui.widgetTimeWrapper->setHidden(false);
        m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    }
    m_timePos->setRange(0, m_outPoint - m_inPoint);

    // no opacity
    if (elem.attribute(QStringLiteral("opacity")) == QLatin1String("false")) {
        m_opacity->setHidden(true);
        m_ui.horizontalLayout2->addStretch(2);
    }

    checkSingleKeyframe();
}

void GeometryWidget::addParameter(const QDomElement &elem)
{
    Mlt::Geometry *geometry = new Mlt::Geometry(elem.attribute(QStringLiteral("value")).toUtf8().data(), m_outPoint - m_inPoint, m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    m_extraGeometries.append(geometry);
    m_timeline->addGeometry(geometry);
    m_extraFactors.append(elem.attribute(QStringLiteral("factor"), QStringLiteral("1")));
    m_extraGeometryNames.append(elem.attribute(QStringLiteral("name")));
}

void GeometryWidget::slotSyncPosition(int relTimelinePos)
{
    // do only sync if this effect is keyframable
    if (m_timePos->maximum() > 0 && KdenliveSettings::transitionfollowcursor()) {
        relTimelinePos = qBound(0, relTimelinePos, m_timePos->maximum());
        slotPositionChanged(relTimelinePos, false);
    }
}

void GeometryWidget::slotRequestSeek(int pos)
{
    if (KdenliveSettings::transitionfollowcursor()) {
        emit seekToPos(pos);
    }
}

void GeometryWidget::checkSingleKeyframe()
{
    if (!m_geometry) {
        return;
    }
    QString serial = m_geometry->serialise();
    m_singleKeyframe = !serial.contains(QLatin1String(";"));
}

void GeometryWidget::slotPositionChanged(int pos, bool seek)
{
    int keyframePos;
    if (pos == -1) {
        pos = m_timePos->getValue();
        keyframePos = pos;
    } else {
        m_timePos->setValue(pos);
        m_timeline->setValue(pos);
        keyframePos = pos;
        if (m_useOffset) {
            keyframePos += m_inPoint;
        }
    }

    //m_timeline->blockSignals(true);
    Mlt::GeometryItem item;
    bool fetch = m_geometry->fetch(&item, keyframePos);
    if (!m_fixedGeom && (fetch || item.key() == false)) {
        // no keyframe
        if (m_singleKeyframe) {
            // Special case: only one keyframe, allow adjusting whatever the position is
            m_monitor->setEffectKeyframe(true);
            m_ui.widgetGeometry->setEnabled(true);
        } else {
            m_monitor->setEffectKeyframe(false);
            m_ui.widgetGeometry->setEnabled(false);
        }
        m_ui.buttonAddDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
        m_ui.buttonAddDelete->setToolTip(i18n("Add keyframe"));
    } else {
        // keyframe
        m_monitor->setEffectKeyframe(true);
        m_ui.widgetGeometry->setEnabled(true);
        m_ui.buttonAddDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));
        m_ui.buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    }
    m_opacity->blockSignals(true);
    m_opacity->setValue(item.mix());
    m_opacity->blockSignals(false);
    QRect r((int) item.x(), (int) item.y(), (int) item.w(), (int) item.h());
    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        QString name = m_extraGeometryNames.at(i);
        if (!geom->fetch(&item, keyframePos)) {
            DragValue *widget = findChild<DragValue *>(name);
            if (widget) {
                widget->blockSignals(true);
                widget->setValue(item.x() * m_extraFactors.at(i).toInt());
                widget->blockSignals(false);
            }
        }
    }
    m_monitor->setUpEffectGeometry(r, calculateCenters());
    slotUpdateProperties(r);

    // scene ratio lock
    if (m_ui.widgetGeometry->isEnabled()) {
        double ratio = m_originalSize->isChecked() ? (double)m_frameSize.x() / m_frameSize.y() : (double)m_monitor->render->frameRenderWidth() / m_monitor->render->renderHeight();
        bool lockRatio = m_spinHeight->value() == (int) (m_spinWidth->value() / ratio + 0.5);
        m_lockRatio->blockSignals(true);
        m_lockRatio->setChecked(lockRatio);
        m_lockRatio->blockSignals(false);
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_lockRatio->isChecked() ? ratio : -1);
    }

    if (seek && KdenliveSettings::transitionfollowcursor()) {
        emit seekToPos(pos);
    }
}

void GeometryWidget::slotInitScene(int pos)
{
    slotPositionChanged(pos, false);
    double ratio = (double)m_spinWidth->value() / m_spinHeight->value();
    if (m_frameSize.x() != m_monitor->render->frameRenderWidth() || m_frameSize.y() != m_monitor->render->renderHeight()) {
        // Source frame size different than project frame size, enable original size option accordingly
        bool isOriginalSize = qAbs((double)m_frameSize.x()/m_frameSize.y() - ratio) < qAbs((double)m_monitor->render->frameRenderWidth()/m_monitor->render->renderHeight() - ratio);
        if (isOriginalSize) {
            m_originalSize->blockSignals(true);
            m_originalSize->setChecked(true);
            m_originalSize->blockSignals(false);
        }
    }
}

void GeometryWidget::slotKeyframeMoved(int pos)
{
    slotPositionChanged(pos);
    slotUpdateGeometry();

    QTimer::singleShot(100, this, &GeometryWidget::valueChanged);
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
    pos = item.frame();
    if (m_useOffset) {
        pos -= m_inPoint;
    }
    slotPositionChanged(pos, true);
}

void GeometryWidget::slotAddKeyframe(int pos)
{
    // "fixed" effect: don't allow keyframe (FIXME: find a better way to access this property!)
    if (m_ui.widgetTimeWrapper->isHidden()) {
        return;
    }
    Mlt::GeometryItem item;
    int seekPos;
    if (pos == -1) {
        pos = m_timeline->value();
        seekPos = pos;
        if (m_useOffset) {
            seekPos -= m_inPoint;
        }
    } else {
        seekPos = pos;
    }
    m_geometry->fetch(&item, pos);
    item.x((int)item.x());
    item.y((int)item.y());
    item.w((int)item.w());
    item.h((int)item.h());
    item.mix((int)item.mix());
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
    slotPositionChanged(seekPos, false);
    emit valueChanged();
}

void GeometryWidget::slotDeleteKeyframe(int pos)
{
    Mlt::GeometryItem item;
    int seekPos;
    if (pos == -1) {
        pos = m_timeline->value();
        seekPos = pos;
        if (m_useOffset) {
            seekPos -= m_inPoint;
        }
    } else {
        seekPos = pos;
    }
    // check there is more than one keyframe, do not allow to delete last one
    if (m_geometry->next_key(&item, pos + 1)) {
        if (m_geometry->prev_key(&item, pos - 1) || item.frame() == pos) {
            return;
        }
    }
    m_geometry->remove(pos);
    for (int i = 0; i < m_extraGeometries.count(); ++i) {
        Mlt::Geometry *geom = m_extraGeometries.at(i);
        geom->remove(pos);
    }

    m_timeline->update();
    checkSingleKeyframe();
    slotPositionChanged(seekPos, false);
    emit valueChanged();
}

void GeometryWidget::slotPreviousKeyframe()
{
    Mlt::GeometryItem item;
    // Go to start if no keyframe is found
    int currentPos = m_timeline->value();
    int pos = 0;
    if (!m_geometry->prev_key(&item, currentPos - 1) && item.frame() < currentPos) {
        pos = item.frame();
        if (m_useOffset) {
            pos -= m_inPoint;
        }
    }
    // Make sure we don't seek past transition's in
    pos = qMax(pos, 0);
    slotPositionChanged(pos);
}

void GeometryWidget::slotNextKeyframe()
{
    Mlt::GeometryItem item;
    // Go to end if no keyframe is found
    int pos = m_timeline->frameLength;
    if (!m_geometry->next_key(&item, m_timeline->value() + 1)) {
        pos = item.frame();
        if (m_useOffset) {
            pos -= m_inPoint;
        }
    }
    // Make sure we don't seek past transition's out
    pos = qMin(pos, m_timeline->frameLength);
    slotPositionChanged(pos);
}

void GeometryWidget::slotAddDeleteKeyframe()
{
    Mlt::GeometryItem item;
    if (m_geometry->fetch(&item, m_timeline->value()) || item.key() == false) {
        slotAddKeyframe();
    } else {
        slotDeleteKeyframe();
    }
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
    emit valueChanged();
}

void GeometryWidget::slotUpdateGeometryRect(const QRect r)
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
            pos = m_timeline->value();
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
    emit valueChanged();
}

void GeometryWidget::slotUpdateCenters(const QVariantList &centers)
{
    Mlt::GeometryItem item;
    int pos = 0;
    int ix = 0;
    while (!m_geometry->next_key(&item, pos)) {
        QPoint center = centers.at(ix).toPoint();
        item.x(center.x() - item.w() / 2);
        item.y(center.y() - item.h() / 2);
        m_geometry->insert(item);
        pos = item.frame() + 1;
        ix++;
    }
    emit valueChanged();
}

void GeometryWidget::slotUpdateProperties(QRect rect)
{
    if (!rect.isValid()) {
        rect = m_monitor->effectRect().normalized();
    }
    double size;
    if (rect.width() / m_monitor->render->dar() > rect.height()) {
        if (m_originalSize->isChecked()) {
            size = rect.width() * 100.0 / m_frameSize.x();
        } else {
            size = rect.width() * 100.0 / m_monitor->render->frameRenderWidth();
        }
    } else {
        if (m_originalSize->isChecked()) {
            size = rect.height() * 100.0 / m_frameSize.y();
        } else {
            size = rect.height() * 100.0 / m_monitor->render->renderHeight();
        }
    }

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
    m_monitor->setUpEffectGeometry(QRect(value, m_spinY->value(), m_spinWidth->value(), m_spinHeight->value()));
    slotUpdateGeometry();
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
}

void GeometryWidget::slotSetY(double value)
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), value, m_spinWidth->value(), m_spinHeight->value()));
    slotUpdateGeometry();
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
}

void GeometryWidget::slotSetWidth(double value)
{
    if (m_lockRatio->isChecked()) {
        m_spinHeight->blockSignals(true);
        if (m_originalSize->isChecked()) {
            m_spinHeight->setValue((int) (value * m_frameSize.y() / m_frameSize.x() + 0.5));
        } else {
            m_spinHeight->setValue((int) (value * m_monitor->render->renderHeight() / m_monitor->render->frameRenderWidth() + 0.5));
        }
        m_spinHeight->blockSignals(false);
    }
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), value, m_spinHeight->value()));
    slotUpdateGeometry();
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
}

void GeometryWidget::slotSetHeight(double value)
{
    if (m_lockRatio->isChecked()) {
        m_spinWidth->blockSignals(true);
        if (m_originalSize->isChecked()) {
            m_spinWidth->setValue((int) (value * m_frameSize.x() / m_frameSize.y() + 0.5));
        } else {
            m_spinWidth->setValue((int) (value * m_monitor->render->frameRenderWidth() / m_monitor->render->renderHeight() + 0.5));
        }
        m_spinWidth->blockSignals(false);
    }
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), value));
    slotUpdateGeometry();
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
}

void GeometryWidget::updateMonitorGeometry()
{
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value()));
    slotUpdateGeometry();
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
}

void GeometryWidget::slotResize(double value)
{
    int w = m_originalSize->isChecked() ? m_frameSize.x() : m_monitor->render->frameRenderWidth();
    int h = m_originalSize->isChecked() ? m_frameSize.y() : m_monitor->render->renderHeight();
    m_monitor->setUpEffectGeometry(QRect(m_spinX->value(), m_spinY->value(), (int)((w * value / 100.0) + 0.5), (int)((h * value / 100.0) + 0.5)));
    slotUpdateGeometry();
    m_monitor->setUpEffectGeometry(QRect(), calculateCenters());
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
    emit valueChanged();
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
    if (sync) {
        emit seekToPos(m_timePos->getValue());
    }
}

void GeometryWidget::setFrameSize(const QPoint &size)
{
    m_frameSize = size;
    if (m_frameSize == QPoint() || m_frameSize.x() == 0 || m_frameSize.y() == 0) {
        m_originalSize->setEnabled(false);
        m_frameSize = QPoint(m_monitor->render->frameRenderWidth(), m_monitor->render->renderHeight());
    } else {
        m_originalSize->setEnabled(true);
    }
    if (m_lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_frameSize.x() / m_frameSize.y() : (double)m_monitor->render->frameRenderWidth() / m_monitor->render->renderHeight());
    } else {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), -1);
    }
}

void GeometryWidget::slotAdjustToSource()
{
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    if (m_originalSize->isChecked()) {
        // Adjust to source size
        m_spinWidth->setValue((int)(m_frameSize.x() / m_monitor->render->sar() + 0.5));
        m_spinHeight->setValue(m_frameSize.y());
    } else {
        // Adjust to profile size
        m_spinWidth->setValue((int)(m_monitor->render->frameRenderWidth() / m_monitor->render->sar() + 0.5));
        m_spinHeight->setValue(m_monitor->render->renderHeight());
    }
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    updateMonitorGeometry();
    if (m_lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_frameSize.x() / m_frameSize.y() : (double)m_monitor->render->frameRenderWidth() / m_monitor->render->renderHeight());
    }
}

void GeometryWidget::slotAdjustToFrameSize()
{
    double monitorDar = m_monitor->render->frameRenderWidth() / m_monitor->render->renderHeight();
    double sourceDar = m_frameSize.x() / m_frameSize.y();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    if (sourceDar > monitorDar) {
        // Fit to width
        double factor = (double) m_monitor->render->frameRenderWidth() / m_frameSize.x() * m_monitor->render->sar();
        m_spinHeight->setValue((int)(m_frameSize.y() * factor + 0.5));
        m_spinWidth->setValue(m_monitor->render->frameRenderWidth());
        // Center
        m_spinY->blockSignals(true);
        m_spinY->setValue((m_monitor->render->renderHeight() - m_spinHeight->value()) / 2);
        m_spinY->blockSignals(false);
    } else {
        // Fit to height
        double factor = (double) m_monitor->render->renderHeight() / m_frameSize.y();
        m_spinHeight->setValue(m_monitor->render->renderHeight());
        m_spinWidth->setValue((int)(m_frameSize.x() / m_monitor->render->sar() * factor + 0.5));
        // Center
        m_spinX->blockSignals(true);
        m_spinX->setValue((m_monitor->render->frameRenderWidth() - m_spinWidth->value()) / 2);
        m_spinX->blockSignals(false);
    }
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    updateMonitorGeometry();
}

void GeometryWidget::slotFitToWidth()
{
    double factor = (double) m_monitor->render->frameRenderWidth() / m_frameSize.x() * m_monitor->render->sar();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue((int)(m_frameSize.y() * factor + 0.5));
    m_spinWidth->setValue(m_monitor->render->frameRenderWidth());
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    updateMonitorGeometry();
}

void GeometryWidget::slotFitToHeight()
{
    double factor = (double) m_monitor->render->renderHeight() / m_frameSize.y();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue(m_monitor->render->renderHeight());
    m_spinWidth->setValue((int)(m_frameSize.x() / m_monitor->render->sar() * factor + 0.5));
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
    m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    m_monitor->setUpEffectGeometry(QRect(item.x(), item.y(), item.w(), item.h()), calculateCenters());
    slotPositionChanged(-1, false);
    emit valueChanged();
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
    m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    slotPositionChanged(-1, false);
    emit valueChanged();
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
    } else {
        item.frame(0);
        item.x(0);
        item.y(0);
        item.w(m_monitor->render->frameRenderWidth());
        item.h(m_monitor->render->renderHeight());
        item.mix(100);
        m_geometry->insert(item);
    }
    m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    slotPositionChanged(-1, false);
    emit valueChanged();
}

void GeometryWidget::importKeyframes(const QString &data, int maximum)
{
    QStringList list = data.split(QLatin1Char(';'), QString::SkipEmptyParts);
    if (list.isEmpty()) {
        return;
    }
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
            item.frame(geom.section(QLatin1Char('='), 0, 0).toInt());
            geom = geom.section(QLatin1Char('='), 1);
        } else {
            item.frame(0);
        }
        if (geom.contains(QLatin1Char('/'))) {
            item.x(geom.section(QLatin1Char('/'), 0, 0).toDouble());
            item.y(geom.section(QLatin1Char('/'), 1, 1).section(QLatin1Char(':'), 0, 0).toDouble());
        } else {
            item.x(0);
            item.y(0);
        }
        if (geom.contains('x')) {
            item.w(geom.section('x', 0, 0).section(QLatin1Char(':'), 1, 1).toDouble());
            item.h(geom.section('x', 1, 1).section(QLatin1Char(':'), 0, 0).toDouble());
        } else {
            item.w(screenSize.x());
            item.h(screenSize.y());
        }
        //TODO: opacity
        item.mix(100);
        m_geometry->insert(item);
    }
    m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    slotPositionChanged(-1, false);
    emit valueChanged();
}

void GeometryWidget::slotUpdateRange(int inPoint, int outPoint)
{
    m_inPoint = inPoint;
    m_outPoint = outPoint;
    m_timeline->setKeyGeometry(m_geometry, m_inPoint, m_outPoint, m_useOffset);
    m_timePos->setRange(0, m_outPoint - m_inPoint);
}

void GeometryWidget::slotLockRatio()
{
    QAction *lockRatio = qobject_cast<QAction*> (QObject::sender());
    if (lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_frameSize.x() / m_frameSize.y() : (double)m_monitor->render->frameRenderWidth() / m_monitor->render->renderHeight());
    } else {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), -1);
    }
}
