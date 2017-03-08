/***************************************************************************
                          effecstackview.cpp2  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler (g.marco@freenet.de)
    copyright            : (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "effectstackview2.h"

#include "collapsibleeffect.h"
#include "collapsiblegroup.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "doc/kthumb.h"
#include "bin/projectclip.h"
#include "effectslist/effectslist.h"
#include "timeline/clipitem.h"
#include "project/effectsettings.h"
#include "project/transitionsettings.h"
#include "utils/KoIconUtils.h"
#include "mltcontroller/clipcontroller.h"
#include "timeline/transition.h"

#include "kdenlive_debug.h"
#include <klocalizedstring.h>
#include <KColorScheme>
#include <QFontDatabase>
#include <KColorUtils>

#include <QScrollBar>
#include <QDrag>
#include <QMimeData>

EffectStackView2::EffectStackView2(Monitor *projectMonitor, QWidget *parent) :
    QWidget(parent),
    m_clipref(nullptr),
    m_masterclipref(nullptr),
    m_status(EMPTY),
    m_stateStatus(NORMALSTATUS),
    m_trackindex(-1),
    m_draggedEffect(nullptr),
    m_draggedGroup(nullptr),
    m_groupIndex(0),
    m_monitorSceneWanted(MonitorSceneDefault),
    m_trackInfo(),
    m_transition(nullptr)
{
    m_effectMetaInfo.monitor = projectMonitor;
    m_effects = QList<CollapsibleEffect *>();
    setAcceptDrops(true);
    setLayout(&m_layout);

    m_effect = new EffectSettings(this);
    m_transition = new TransitionSettings(projectMonitor, this);
    connect(m_transition, SIGNAL(importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString, QString>)), this, SIGNAL(importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString, QString>)));
    connect(m_effect->checkAll, &QCheckBox::stateChanged, this, &EffectStackView2::slotCheckAll);
    connect(m_effect->effectCompare, &QToolButton::toggled, this, &EffectStackView2::slotSwitchCompare);

    m_scrollTimer.setSingleShot(true);
    m_scrollTimer.setInterval(200);
    connect(&m_scrollTimer, &QTimer::timeout, this, &EffectStackView2::slotCheckWheelEventFilter);

    m_layout.addWidget(m_effect);
    m_layout.addWidget(m_transition);
    m_transition->setHidden(true);
    //setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setEnabled(false);
    setStyleSheet(getStyleSheet());
}

EffectStackView2::~EffectStackView2()
{
    delete m_effect;
    delete m_transition;
}

TransitionSettings *EffectStackView2::transitionConfig()
{
    return m_transition;
}

void EffectStackView2::updatePalette()
{
    setStyleSheet(getStyleSheet());
}

void EffectStackView2::refreshIcons()
{
    QList<QAction *> allMenus = this->findChildren<QAction *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QAction *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
    QList<QToolButton *> allButtons = this->findChildren<QToolButton *>();
    for (int i = 0; i < allButtons.count(); i++) {
        QToolButton *m = allButtons.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
}

void EffectStackView2::slotTransitionItemSelected(Transition *t, int nextTrack, const QPoint &p, bool update)
{
    if (t) {
        m_effect->setHidden(true);
        m_transition->setHidden(false);
        setEnabled(true);
        m_status = TIMELINE_TRANSITION;
    }
    m_transition->slotTransitionItemSelected(t, nextTrack, p, update);
}

void EffectStackView2::slotRenderPos(int pos)
{
    if (m_effects.isEmpty()) {
        return;
    }
    if (m_monitorSceneWanted != MonitorSceneDefault) {
        slotCheckMonitorPosition(pos);
    }
    if (m_status == TIMELINE_CLIP && m_clipref) {
        pos = pos - m_clipref->startPos().frames(KdenliveSettings::project_fps());
    }

    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->slotSyncEffectsPos(pos);
    }
}

void EffectStackView2::slotClipItemUpdate()
{
    if (m_status != TIMELINE_CLIP || !m_clipref) {
        return;
    }
    int inPoint = m_clipref->cropStart().frames(KdenliveSettings::project_fps());
    int outPoint = m_clipref->cropDuration().frames(KdenliveSettings::project_fps()) + inPoint;
    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->setRange(inPoint, outPoint);
    }
}

void EffectStackView2::slotClipItemSelected(ClipItem *c, Monitor *m, bool reloadStack)
{
    QMutexLocker lock(&m_mutex);
    if (m_effect->effectCompare->isChecked()) {
        // disable split effect when changing clip
        m_effect->effectCompare->setChecked(false);
    }
    if (c) {
        m_transition->setHidden(true);
        m_effect->setHidden(false);
        m_effect->setEnabled(m_stateStatus != DISABLETIMELINE && m_stateStatus != DISABLEALL);
    } else if (m_status == TIMELINE_TRANSITION) {
        return;
    }
    m_masterclipref = nullptr;
    m_trackindex = -1;
    if (c && !c->isEnabled()) {
        return;
    }
    if (c && c == m_clipref) {
        if (!reloadStack) {
            return;
        }
    } else {
        m_effectMetaInfo.monitor = m;
        if (m_clipref) {
            disconnect(m_clipref, &ClipItem::updateRange, this, &EffectStackView2::slotClipItemUpdate);
        }
        m_clipref = c;
        if (c) {
            connect(m_clipref, &ClipItem::updateRange, this, &EffectStackView2::slotClipItemUpdate);
            m_effect->setLabel(i18n("Effects for %1", m_clipref->clipName()), m_clipref->clipName());
            int frameWidth = c->binClip()->getProducerIntProperty(QStringLiteral("meta.media.width"));
            int frameHeight = c->binClip()->getProducerIntProperty(QStringLiteral("meta.media.height"));
            double factor = c->binClip()->getDoubleProducerProperty(QStringLiteral("aspect_ratio"));
            m_effectMetaInfo.frameSize = QPoint(frameWidth, frameHeight);// (int)(frameWidth * factor + 0.5), frameHeight);
            m_effectMetaInfo.stretchFactor = factor;
        }
    }
    if (m_clipref == nullptr) {
        //TODO: clear list, reset paramdesc and info
        // If monitor scene is displayed, hide it
        if (m_monitorSceneWanted != MonitorSceneDefault) {
            m_monitorSceneWanted = MonitorSceneDefault;
            m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted);
        }
        m_status = EMPTY;
        clear();
        return;
    }
    setEnabled(true);
    m_status = TIMELINE_CLIP;
    m_currentEffectList = m_clipref->effectList();
    setupListView();
}

void EffectStackView2::slotRefreshMasterClipEffects(ClipController *c, Monitor *m)
{
    if (c && m_status == MASTER_CLIP && m_masterclipref && m_masterclipref->clipId() == c->clipId()) {
        slotMasterClipItemSelected(c, m);
    }
}

void EffectStackView2::slotMasterClipItemSelected(ClipController *c, Monitor *m)
{
    QMutexLocker lock(&m_mutex);
    if (m_effect->effectCompare->isChecked()) {
        // disable split effect when changing clip
        m_effect->effectCompare->setChecked(false);
    }
    m_clipref = nullptr;
    m_trackindex = -1;
    if (c) {
        m_transition->setHidden(true);
        m_effect->setHidden(false);
        if (!c->isValid()) {
            m_effect->setEnabled(false);
            c = nullptr;
        } else {
            m_effect->setEnabled(m_stateStatus != DISABLEBIN && m_stateStatus != DISABLEALL);
        }
    }
    if (c && c == m_masterclipref) {
    } else {
        m_masterclipref = c;
        m_effectMetaInfo.monitor = m;
        if (m_masterclipref) {
            m_effect->setLabel(i18n("Bin effects for %1", m_masterclipref->clipName()), m_masterclipref->clipName());
            int frameWidth = m_masterclipref->int_property(QStringLiteral("meta.media.width"));
            int frameHeight = m_masterclipref->int_property(QStringLiteral("meta.media.height"));
            double factor = m_masterclipref->double_property(QStringLiteral("aspect_ratio"));
            m_effectMetaInfo.frameSize = QPoint((int)(frameWidth * factor + 0.5), frameHeight);
        }
    }
    if (m_masterclipref == nullptr) {
        //TODO: clear list, reset paramdesc and info
        // If monitor scene is displayed, hide it
        if (m_monitorSceneWanted != MonitorSceneDefault) {
            m_monitorSceneWanted = MonitorSceneDefault;
            m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted);
        }
        m_status = EMPTY;
        clear();
        return;
    }
    setEnabled(true);
    m_status = MASTER_CLIP;
    m_currentEffectList = m_masterclipref->effectList();
    setupListView();
}

void EffectStackView2::slotTrackItemSelected(int ix, const TrackInfo &info, Monitor *m)
{
    QMutexLocker lock(&m_mutex);
    if (m_effect->effectCompare->isChecked()) {
        // disable split effect when changing clip
        m_effect->effectCompare->setChecked(false);
    }
    if (m_status != TIMELINE_TRACK || ix != m_trackindex) {
        m_transition->setHidden(true);
        m_effect->setHidden(false);
        m_clipref = nullptr;
        m_status = TIMELINE_TRACK;
        m_effectMetaInfo.monitor = m;
        m_currentEffectList = info.effectsList;
        m_trackInfo = info;
        m_masterclipref = nullptr;
        QString trackName = info.trackName.isEmpty() ? QString::number(ix) : info.trackName;
        m_effect->setLabel(i18n("Effects for track %1", trackName), trackName);
    }
    setEnabled(true);
    m_trackindex = ix;
    setupListView();
}

void EffectStackView2::setupListView()
{
    blockSignals(true);
    m_scrollTimer.stop();
    m_monitorSceneWanted = MonitorSceneDefault;
    m_draggedEffect = nullptr;
    m_draggedGroup = nullptr;
    disconnect(m_effectMetaInfo.monitor, &Monitor::renderPosition, this, &EffectStackView2::slotRenderPos);
    QWidget *view = m_effect->container->takeWidget();
    if (view) {
        /*QList<CollapsibleEffect *> allChildren = view->findChildren<CollapsibleEffect *>();
        qCDebug(KDENLIVE_LOG)<<" * * *FOUND CHLD: "<<allChildren.count();
        foreach(CollapsibleEffect *eff, allChildren) {
            eff->setEnabled(false);
        }*/
        //delete view;
        view->setEnabled(false);
        view->setHidden(true);
        view->deleteLater();
    }
    m_effects.clear();
    m_groupIndex = 0;
    blockSignals(false);
    view = new QWidget(this);
    QPalette p = qApp->palette();
    p.setBrush(QPalette::Window, QBrush(Qt::transparent));
    view->setPalette(p);
    m_effect->container->setWidget(view);

    QVBoxLayout *vbox1 = new QVBoxLayout(view);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);

    int effectsCount = m_currentEffectList.count();
    m_effect->effectCompare->setEnabled(effectsCount > 0);
    if (effectsCount == 0) {
        // No effect, make sure to display normal monitor scene
        m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted);
    }

    // Make sure we always have one effect selected
    if (m_status == TIMELINE_CLIP) {
        int selectedEffect = m_clipref->selectedEffectIndex();
        if (selectedEffect < 1 && effectsCount > 0) {
            m_clipref->setSelectedEffect(1);
        } else if (selectedEffect > effectsCount) {
            m_clipref->setSelectedEffect(effectsCount);
        }
    }

    CollapsibleEffect *selectedCollapsibleEffect = nullptr;
    for (int i = 0; i < effectsCount; ++i) {
        QDomElement d = m_currentEffectList.at(i).cloneNode().toElement();
        if (d.isNull()) {
            //qCDebug(KDENLIVE_LOG) << " . . . . WARNING, nullptr EFFECT IN STACK!!!!!!!!!";
            continue;
        }

        CollapsibleGroup *group = nullptr;
        EffectInfo effectInfo;
        effectInfo.fromString(d.attribute(QStringLiteral("kdenlive_info")));
        if (effectInfo.groupIndex >= 0) {
            // effect is in a group
            for (int j = 0; j < vbox1->count(); ++j) {
                CollapsibleGroup *eff = static_cast<CollapsibleGroup *>(vbox1->itemAt(j)->widget());
                if (eff->isGroup() &&  eff->groupIndex() == effectInfo.groupIndex) {
                    group = eff;
                    break;
                }
            }

            if (group == nullptr) {
                group = new CollapsibleGroup(effectInfo.groupIndex, i == 0, i == effectsCount - 1, effectInfo, view);
                connectGroup(group);
                vbox1->addWidget(group);
                group->installEventFilter(this);
            }
            if (effectInfo.groupIndex >= m_groupIndex) {
                m_groupIndex = effectInfo.groupIndex + 1;
            }
        }

        /*QDomDocument doc;
        doc.appendChild(doc.importNode(d, true));
        //qCDebug(KDENLIVE_LOG) << "IMPORTED STK: " << doc.toString();*/

        ItemInfo info;
        bool isSelected = false;
        if (m_status == TIMELINE_TRACK) {
            // ?? cleanup following line
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
        } else if (m_status == TIMELINE_CLIP) {
            info = m_clipref->info();
        } else if (m_status == MASTER_CLIP) {
            info.cropDuration = m_masterclipref->getPlaytime();
            info.cropStart = GenTime(0);
            info.startPos = GenTime(0);
        }
        bool canMoveUp = true;
        if (i == 0 || m_currentEffectList.at(i - 1).attribute(QStringLiteral("id")) == QLatin1String("speed")) {
            canMoveUp = false;
        }
        CollapsibleEffect *currentEffect = new CollapsibleEffect(d, m_currentEffectList.at(i), info, &m_effectMetaInfo, canMoveUp, i == effectsCount - 1, view);
        isSelected = currentEffect->effectIndex() == activeEffectIndex();
        if (isSelected) {
            m_monitorSceneWanted = currentEffect->needsMonitorEffectScene();
            selectedCollapsibleEffect = currentEffect;
            // show monitor scene if necessary
            m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted);
        }
        int position = (m_effectMetaInfo.monitor->position() - (m_status == TIMELINE_CLIP ? m_clipref->startPos() : GenTime())).frames(KdenliveSettings::project_fps());
        currentEffect->slotSyncEffectsPos(position);
        currentEffect->setActive(isSelected);
        m_effects.append(currentEffect);
        if (group) {
            group->addGroupEffect(currentEffect);
        } else {
            vbox1->addWidget(currentEffect);
        }
        connectEffect(currentEffect);
    }

    if (selectedCollapsibleEffect) {
        // pass frame size info to effect, so it can update the newly created qml scene
        selectedCollapsibleEffect->updateFrameInfo();
    }
    if (m_currentEffectList.isEmpty()) {
        //m_ui.labelComment->setHidden(true);
    } else {
        // Adjust group effects (up / down buttons)
        QList<CollapsibleGroup *> allGroups = m_effect->container->widget()->findChildren<CollapsibleGroup *>();
        for (int i = 0; i < allGroups.count(); ++i) {
            allGroups.at(i)->adjustEffects();
        }
        connect(m_effectMetaInfo.monitor, &Monitor::renderPosition, this, &EffectStackView2::slotRenderPos);
    }

    vbox1->addStretch(10);
    slotUpdateCheckAllButton();

    // Wait a little bit for the new layout to be ready, then check if we have a scrollbar
    m_scrollTimer.start();
}

int EffectStackView2::activeEffectIndex() const
{
    int index = 0;
    switch (m_status) {
    case TIMELINE_CLIP:
        index = m_clipref->selectedEffectIndex();
        break;
    case MASTER_CLIP:
        index = m_masterclipref->selectedEffectIndex;
        break;
    case TIMELINE_TRACK:
    default:
        // TODO
        index = 1;
    }
    return index;
}

void EffectStackView2::connectEffect(CollapsibleEffect *currentEffect)
{
    // Check drag & drop
    currentEffect->installEventFilter(this);
    connect(currentEffect, &CollapsibleEffect::parameterChanged, this, &EffectStackView2::slotUpdateEffectParams);
    connect(currentEffect, &CollapsibleEffect::startFilterJob, this, &EffectStackView2::slotStartFilterJob);
    connect(currentEffect, &CollapsibleEffect::deleteEffect, this, &EffectStackView2::slotDeleteEffect);
    connect(currentEffect, &AbstractCollapsibleWidget::reloadEffects, this, &EffectStackView2::reloadEffects);
    connect(currentEffect, &CollapsibleEffect::resetEffect, this, &EffectStackView2::slotResetEffect);
    connect(currentEffect, &AbstractCollapsibleWidget::changeEffectPosition, this, &EffectStackView2::slotMoveEffectUp);
    connect(currentEffect, &CollapsibleEffect::effectStateChanged, this, &EffectStackView2::slotUpdateEffectState);
    connect(currentEffect, &CollapsibleEffect::activateEffect, this, &EffectStackView2::slotSetCurrentEffect);
    connect(currentEffect, &CollapsibleEffect::seekTimeline, this, &EffectStackView2::slotSeekTimeline);
    connect(currentEffect, &CollapsibleEffect::createGroup, this, &EffectStackView2::slotCreateGroup);
    connect(currentEffect, &AbstractCollapsibleWidget::moveEffect, this, &EffectStackView2::slotMoveEffect);
    connect(currentEffect, &AbstractCollapsibleWidget::addEffect, this, &EffectStackView2::slotAddEffect);
    connect(currentEffect, &CollapsibleEffect::createRegion, this, &EffectStackView2::slotCreateRegion);
    connect(currentEffect, &CollapsibleEffect::deleteGroup, this, &EffectStackView2::slotDeleteGroup);
    connect(currentEffect, SIGNAL(importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString, QString>)), this, SIGNAL(importClipKeyframes(GraphicsRectItem, ItemInfo, QDomElement, QMap<QString, QString>)));
}

void EffectStackView2::slotCheckWheelEventFilter()
{
    // If the effect stack widget has no scrollbar, we will not filter the
    // mouse wheel events, so that user can easily adjust effect params
    bool filterWheelEvent = false;
    if (m_effect->container->verticalScrollBar() && m_effect->container->verticalScrollBar()->isVisible()) {
        // widget has scroll bar,
        filterWheelEvent = true;
    }
    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->filterWheelEvent = filterWheelEvent;
    }
}

void EffectStackView2::resizeEvent(QResizeEvent *event)
{
    slotCheckWheelEventFilter();
    QWidget::resizeEvent(event);
}

bool EffectStackView2::eventFilter(QObject *o, QEvent *e)
{
    // Check if user clicked in an effect's top bar to start dragging it
    if (e->type() == QEvent::MouseButtonPress)  {
        m_draggedEffect = qobject_cast<CollapsibleEffect *>(o);
        if (m_draggedEffect) {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton && (m_draggedEffect->frame->underMouse() || m_draggedEffect->title->underMouse())) {
                m_clickPoint = me->globalPos();
            } else {
                m_clickPoint = QPoint();
                m_draggedEffect = nullptr;
            }
            e->accept();
            return true;
        }
        m_draggedGroup = qobject_cast<CollapsibleGroup *>(o);
        if (m_draggedGroup) {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton && (m_draggedGroup->frame->underMouse() || m_draggedGroup->title()->underMouse())) {
                m_clickPoint = me->globalPos();
            } else {
                m_clickPoint = QPoint();
                m_draggedGroup = nullptr;
            }
            e->accept();
            return true;
        }
    }
    /*if (e->type() == QEvent::MouseMove)  {
    if (qobject_cast<CollapsibleEffect*>(o)) {
        QMouseEvent *me = static_cast<QMouseEvent *>(e);
        if (me->buttons() != Qt::LeftButton) {
        e->accept();
        return false;
        }
        else {
        e->ignore();
        return true;
        }
    }
    }*/
    return QWidget::eventFilter(o, e);
}

void EffectStackView2::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggedEffect || m_draggedGroup) {
        if ((event->buttons() & Qt::LeftButton) && (m_clickPoint != QPoint()) && ((event->globalPos() - m_clickPoint).manhattanLength() >= QApplication::startDragDistance())) {
            startDrag();
        }
    }
}

void EffectStackView2::mouseReleaseEvent(QMouseEvent *event)
{
    m_draggedEffect = nullptr;
    m_draggedGroup = nullptr;
    QWidget::mouseReleaseEvent(event);
}

void EffectStackView2::startDrag()
{
    // The data to be transferred by the drag and drop operation is contained in a QMimeData object
    QDomDocument doc;
    QPixmap pixmap;
    if (m_draggedEffect) {
        QDomElement effect = m_draggedEffect->effect().cloneNode().toElement();
        if (m_status == TIMELINE_TRACK || m_status == MASTER_CLIP) {
            // Keep clip crop start in case we want to paste effect
            effect.setAttribute(QStringLiteral("clipstart"), 0);
        } else {
            // Keep clip crop start in case we want to paste effect
            effect.setAttribute(QStringLiteral("clipstart"), m_clipref->cropStart().frames(KdenliveSettings::project_fps()));
        }
        doc.appendChild(doc.importNode(effect, true));
        pixmap = m_draggedEffect->title->grab();
    } else if (m_draggedGroup) {
        doc = m_draggedGroup->effectsData();
        if (m_status == TIMELINE_TRACK || m_status == MASTER_CLIP) {
            doc.documentElement().setAttribute(QStringLiteral("clipstart"), 0);
        } else {
            doc.documentElement().setAttribute(QStringLiteral("clipstart"), m_clipref->cropStart().frames(KdenliveSettings::project_fps()));
        }
        pixmap = m_draggedGroup->title()->grab();
    } else {
        return;
    }
    QDrag *drag = new QDrag(this);
    drag->setPixmap(pixmap);
    QMimeData *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/effectslist"), doc.toString().toUtf8());

    // Assign ownership of the QMimeData object to the QDrag object.
    drag->setMimeData(mime);
    // Start the drag and drop operation
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void EffectStackView2::slotUpdateEffectState(bool disable, int index, MonitorSceneType needsMonitorEffectScene)
{
    if (m_monitorSceneWanted != MonitorSceneDefault && disable) {
        m_monitorSceneWanted = MonitorSceneDefault;
        m_effectMetaInfo.monitor->slotShowEffectScene(MonitorSceneDefault);
    } else if (!disable) {
        m_monitorSceneWanted = needsMonitorEffectScene;
        m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted == MonitorSceneDefault ? MonitorSceneNone : m_monitorSceneWanted);
        if (m_monitorSceneWanted != MonitorSceneDefault) {
            CollapsibleEffect *activeEffect = getEffectByIndex(index);
            if (activeEffect) {
                int position = (m_effectMetaInfo.monitor->position() - (m_status == TIMELINE_CLIP ? m_clipref->startPos() : GenTime())).frames(KdenliveSettings::project_fps());
                activeEffect->slotSyncEffectsPos(position);
            }
        }
    }
    switch (m_status) {
    case TIMELINE_TRACK:
        emit changeEffectState(nullptr, m_trackindex, QList<int>() << index, disable);
        break;
    case MASTER_CLIP:
        emit changeMasterEffectState(m_masterclipref->clipId(), QList<int>() << index, disable);
        break;
    default:
        // timeline clip effect
        emit changeEffectState(m_clipref, -1, QList<int>() << index, disable);
    }
    slotUpdateCheckAllButton();
}

void EffectStackView2::raiseWindow(QWidget *dock)
{
    if (m_status != EMPTY && dock) {
        dock->raise();
    }
}

void EffectStackView2::slotSeekTimeline(int pos)
{
    if (m_status == TIMELINE_TRACK) {
        emit seekTimeline(pos);
    } else if (m_status == TIMELINE_CLIP) {
        emit seekTimeline(m_clipref->startPos().frames(KdenliveSettings::project_fps()) + pos);
    } else if (m_status == MASTER_CLIP) {
        m_effectMetaInfo.monitor->slotSeek(pos);
    }
}

/*void EffectStackView2::slotRegionChanged()
{
    if (!m_trackMode) emit updateClipRegion(m_clipref, m_ui.effectlist->currentRow(), m_ui.region_url->text());
}*/

void EffectStackView2::slotCheckMonitorPosition(int renderPos)
{
    if (m_monitorSceneWanted != MonitorSceneDefault) {
        if (m_status == TIMELINE_TRACK || m_status == MASTER_CLIP || (m_clipref && renderPos >= m_clipref->startPos().frames(KdenliveSettings::project_fps()) && renderPos <= m_clipref->endPos().frames(KdenliveSettings::project_fps()))) {
            if (!m_effectMetaInfo.monitor->effectSceneDisplayed(m_monitorSceneWanted)) {
                m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted);
                // Find active effect and refresh frame info
                CollapsibleEffect *activeEffect = getEffectByIndex(activeEffectIndex());
                if (activeEffect) {
                    activeEffect->updateFrameInfo();
                }
            }
        } else {
            m_effectMetaInfo.monitor->slotShowEffectScene(MonitorSceneDefault);
        }
    } else {
        m_effectMetaInfo.monitor->slotShowEffectScene(MonitorSceneDefault);
    }
}

EFFECTMODE EffectStackView2::effectStatus() const
{
    return m_status;
}

int EffectStackView2::trackIndex() const
{
    return m_trackindex;
}

void EffectStackView2::clear()
{
    m_effects.clear();
    m_monitorSceneWanted = MonitorSceneDefault;
    QWidget *view = m_effect->container->takeWidget();
    if (view) {
        delete view;
    }
    m_effect->setLabel(QString());
    //m_ui.labelComment->setText(QString());
    if (m_status != TIMELINE_TRANSITION) {
        setEnabled(false);
    }
}

void EffectStackView2::slotCheckAll(int state)
{
    if (state == Qt::PartiallyChecked) {
        state = Qt::Checked;
        m_effect->updateCheckState(state);
    }

    bool disabled = state == Qt::Unchecked;
    // Disable all effects
    QList<int> indexes;
    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->slotDisable(disabled, false);
        indexes << m_effects.at(i)->effectIndex();
    }
    // Take care of groups
    QList<CollapsibleGroup *> allGroups = m_effect->container->widget()->findChildren<CollapsibleGroup *>();
    for (int i = 0; i < allGroups.count(); ++i) {
        allGroups.at(i)->slotEnable(disabled, false);
    }

    if (m_status == TIMELINE_TRACK) {
        emit changeEffectState(nullptr, m_trackindex, indexes, disabled);
    } else if (m_status == TIMELINE_CLIP) {
        emit changeEffectState(m_clipref, -1, indexes, disabled);
    } else if (m_status == MASTER_CLIP) {
        emit changeMasterEffectState(m_masterclipref->clipId(), indexes, disabled);
    }
}

void EffectStackView2::slotUpdateCheckAllButton()
{
    bool hasEnabled = false;
    bool hasDisabled = false;

    for (int i = 0; i < m_effects.count(); ++i) {
        if (!m_effects.at(i)->isEnabled()) {
            hasEnabled = true;
        } else {
            hasDisabled = true;
        }
    }

    if (hasEnabled && hasDisabled) {
        m_effect->updateCheckState(Qt::PartiallyChecked);
    } else if (hasEnabled) {
        m_effect->updateCheckState(Qt::Checked);
    } else {
        m_effect->updateCheckState(Qt::Unchecked);
    }
}

void EffectStackView2::deleteCurrentEffect()
{
    for (int i = 0; i < m_effects.count(); ++i) {
        if (m_effects.at(i)->isActive()) {
            slotDeleteEffect(m_effects.at(i)->effect());
            break;
        }
    }
}

void EffectStackView2::updateTimecodeFormat()
{
    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->updateTimecodeFormat();
    }
}

CollapsibleEffect *EffectStackView2::getEffectByIndex(int ix)
{
    for (int i = 0; i < m_effects.count(); ++i) {
        if (m_effects.at(i)->effectIndex() == ix) {
            return m_effects.at(i);
        }
    }
    return nullptr;
}

void EffectStackView2::slotUpdateEffectParams(const QDomElement &old, const QDomElement &e, int ix)
{
    if (m_status == TIMELINE_TRACK) {
        emit updateEffect(nullptr, m_trackindex, old, e, ix, false);
    } else if (m_status == TIMELINE_CLIP && m_clipref) {
        emit updateEffect(m_clipref, -1, old, e, ix, false);
        // Make sure the changed effect is currently displayed
        slotSetCurrentEffect(ix);
    } else if (m_status == MASTER_CLIP) {
        emit updateMasterEffect(m_masterclipref->clipId(), old, e, ix);
    }
    m_scrollTimer.start();
}

void EffectStackView2::slotSetCurrentEffect(int ix)
{
    if (m_status == TIMELINE_CLIP) {
        if (m_clipref && ix != m_clipref->selectedEffectIndex()) {
            m_clipref->setSelectedEffect(ix);
            for (int i = 0; i < m_effects.count(); ++i) {
                CollapsibleEffect *effect = m_effects.at(i);
                if (effect->effectIndex() == ix) {
                    if (effect->isActive()) {
                        return;
                    }
                    effect->setActive(true);
                    m_monitorSceneWanted = effect->needsMonitorEffectScene();
                    m_effectMetaInfo.monitor->slotShowEffectScene(m_monitorSceneWanted);
                    int position = (m_effectMetaInfo.monitor->position() - (m_status == TIMELINE_CLIP ? m_clipref->startPos() : GenTime())).frames(KdenliveSettings::project_fps());
                    effect->slotSyncEffectsPos(position);
                } else {
                    effect->setActive(false);
                }
            }
        }
    }
}

void EffectStackView2::setActiveKeyframe(int frame)
{
    if (m_status == TIMELINE_CLIP) {
        CollapsibleEffect *activeEffect = getEffectByIndex(activeEffectIndex());
        if (activeEffect) {
            activeEffect->setActiveKeyframe(frame);
        }
    }
}

void EffectStackView2::slotDeleteGroup(const QDomDocument &doc)
{
    ClipItem *clip = nullptr;
    int ix = -1;
    if (m_status == MASTER_CLIP) {
        //TODO
        return;
    }
    if (m_status == TIMELINE_TRACK) {
        ix = m_trackindex;
    } else if (m_status == TIMELINE_CLIP) {
        clip = m_clipref;
        ix = -1;
    }
    emit removeEffectGroup(clip, ix, doc);
}

void EffectStackView2::slotDeleteEffect(const QDomElement &effect)
{
    if (m_status == TIMELINE_TRACK) {
        emit removeEffect(nullptr, m_trackindex, effect);
    } else if (m_status == TIMELINE_CLIP) {
        emit removeEffect(m_clipref, -1, effect);
    }
    if (m_status == MASTER_CLIP) {
        emit removeMasterEffect(m_masterclipref->clipId(), effect);
    }
}

void EffectStackView2::slotAddEffect(const QDomElement &effect)
{
    if (m_status == MASTER_CLIP) {
        emit addMasterEffect(m_masterclipref->clipId(), effect);
    } else {
        emit addEffect(m_clipref, effect, m_trackindex);
    }
}

void EffectStackView2::slotMoveEffectUp(const QList<int> &indexes, bool up)
{
    if (up && indexes.first() <= 1) {
        return;
    }
    if (!up && indexes.last() >= m_currentEffectList.count()) {
        return;
    }
    int endPos;
    if (up) {
        endPos = getPreviousIndex(indexes.first());
    } else {
        endPos = getNextIndex(indexes.last());
    }
    if (m_status == TIMELINE_TRACK) {
        emit changeEffectPosition(nullptr, m_trackindex, indexes, endPos);
    } else if (m_status == TIMELINE_CLIP) {
        emit changeEffectPosition(m_clipref, -1, indexes, endPos);
    } else if (m_status == MASTER_CLIP) {
        emit changeEffectPosition(m_masterclipref->clipId(), indexes, endPos);
    }
}

int EffectStackView2::getPreviousIndex(int ix)
{
    CollapsibleEffect *current = getEffectByIndex(ix);
    int previousIx = ix - 1;
    CollapsibleEffect *destination = getEffectByIndex(previousIx);
    while (previousIx > 1 && destination->groupIndex() != -1 && destination->groupIndex() != current->groupIndex()) {
        previousIx--;
        destination = getEffectByIndex(previousIx);
    }
    return previousIx;
}

int EffectStackView2::getNextIndex(int ix)
{
    CollapsibleEffect *current = getEffectByIndex(ix);
    int previousIx = ix + 1;
    CollapsibleEffect *destination = getEffectByIndex(previousIx);
    while (destination && destination->groupIndex() != -1 && destination->groupIndex() != current->groupIndex()) {
        previousIx++;
        destination = getEffectByIndex(previousIx);
    }
    return previousIx;
}

void EffectStackView2::slotStartFilterJob(QMap<QString, QString> &filterParams, QMap<QString, QString> &consumerParams, QMap<QString, QString> &extraParams)
{
    if (m_status == TIMELINE_CLIP && m_clipref) {
        emit startFilterJob(m_clipref->info(), m_clipref->getBinId(), filterParams, consumerParams, extraParams);
    } else if (m_status == MASTER_CLIP && m_masterclipref) {
        ItemInfo info;
        emit startFilterJob(info, m_masterclipref->clipId(), filterParams, consumerParams, extraParams);
    }
}

void EffectStackView2::slotResetEffect(int ix)
{
    QDomElement old = m_currentEffectList.itemFromIndex(ix);
    QDomElement dom;
    QString effectId = old.attribute(QStringLiteral("id"));
    QMap<QString, EffectsList *> effectLists;
    effectLists[QStringLiteral("audio")] = &MainWindow::audioEffects;
    effectLists[QStringLiteral("video")] = &MainWindow::videoEffects;
    effectLists[QStringLiteral("custom")] = &MainWindow::customEffects;
    foreach (const EffectsList *list, effectLists) {
        dom = list->getEffectByTag(QString(), effectId).cloneNode().toElement();
        if (!dom.isNull()) {
            break;
        }
    }
    if (!dom.isNull()) {
        dom.setAttribute(QStringLiteral("kdenlive_ix"), old.attribute(QStringLiteral("kdenlive_ix")));
        if (m_status == TIMELINE_TRACK) {
            EffectsList::setParameter(dom, QStringLiteral("in"), QString::number(0));
            EffectsList::setParameter(dom, QStringLiteral("out"), QString::number(m_trackInfo.duration));
            ItemInfo info;
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
            for (int i = 0; i < m_effects.count(); ++i) {
                if (m_effects.at(i)->effectIndex() == ix) {
                    m_effects.at(i)->updateWidget(info, dom, &m_effectMetaInfo);
                    break;
                }
            }
            emit updateEffect(nullptr, m_trackindex, old, dom, ix, false);
        } else if (m_status == TIMELINE_CLIP) {
            m_clipref->initEffect(m_effectMetaInfo.monitor->profileInfo(), dom);
            emit updateEffect(m_clipref, -1, old, dom, ix, true);
        } else if (m_status == MASTER_CLIP) {
            m_masterclipref->initEffect(m_effectMetaInfo.monitor->profileInfo(), dom);
            emit updateMasterEffect(m_masterclipref->clipId(), old, dom, ix,true);
        }
    }

    //emit showComments(m_ui.buttonShowComments->isChecked());
    //m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || m_ui.labelComment->text().isEmpty());
}

void EffectStackView2::slotCreateRegion(int ix, const QUrl &url)
{
    QDomElement oldeffect = m_currentEffectList.itemFromIndex(ix);
    QDomElement neweffect = oldeffect.cloneNode().toElement();
    QDomElement region = MainWindow::videoEffects.getEffectByTag(QStringLiteral("region"), QStringLiteral("region")).cloneNode().toElement();
    region.appendChild(region.ownerDocument().importNode(neweffect, true));
    region.setAttribute(QStringLiteral("kdenlive_ix"), ix);
    EffectsList::setParameter(region, QStringLiteral("resource"), url.toLocalFile());
    if (m_status == TIMELINE_TRACK) {
        emit updateEffect(nullptr, m_trackindex, oldeffect, region, ix, false);
    } else if (m_status == TIMELINE_CLIP && m_clipref) {
        emit updateEffect(m_clipref, -1, oldeffect, region, ix, false);
        // Make sure the changed effect is currently displayed
        //slotSetCurrentEffect(ix);
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }
    // refresh effect stack
    ItemInfo info;
    bool isSelected = false;
    if (m_status == TIMELINE_TRACK) {
        info.track = m_trackInfo.type;
        info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
        info.cropStart = GenTime(0);
        info.startPos = GenTime(-1);
        info.track = 0;
    } else if (m_status == TIMELINE_CLIP && m_clipref) {
        info = m_clipref->info();
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }
    CollapsibleEffect *current = getEffectByIndex(ix);
    m_effects.removeAll(current);
    current->setEnabled(false);
    m_currentEffectList.removeAt(ix);
    m_currentEffectList.insert(region);
    current->deleteLater();
    CollapsibleEffect *currentEffect = new CollapsibleEffect(region, m_currentEffectList.itemFromIndex(ix), info, &m_effectMetaInfo, false, ix == m_currentEffectList.count() - 1, m_effect->container->widget());
    connectEffect(currentEffect);

    if (m_status == TIMELINE_TRACK) {
        isSelected = currentEffect->effectIndex() == 1;
    } else if (m_status == TIMELINE_CLIP && m_clipref) {
        isSelected = currentEffect->effectIndex() == m_clipref->selectedEffectIndex();
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }
    if (isSelected) {
        currentEffect->setActive(true);
    }
    m_effects.append(currentEffect);
    // TODO: region in group?
    //if (group) {
    //  group->addGroupEffect(currentEffect);
    //} else {
    QVBoxLayout *vbox = static_cast <QVBoxLayout *>(m_effect->container->widget()->layout());
    vbox->insertWidget(ix, currentEffect);
    //}

    // Check drag & drop
    currentEffect->installEventFilter(this);

    m_scrollTimer.start();

}

void EffectStackView2::slotCreateGroup(int ix)
{
    QDomElement oldeffect = m_currentEffectList.itemFromIndex(ix);
    QDomElement neweffect = oldeffect.cloneNode().toElement();
    EffectInfo effectinfo;
    effectinfo.fromString(oldeffect.attribute(QStringLiteral("kdenlive_info")));
    effectinfo.groupIndex = m_groupIndex;
    neweffect.setAttribute(QStringLiteral("kdenlive_info"), effectinfo.toString());

    ItemInfo info;
    if (m_status == TIMELINE_TRACK) {
        info.track = m_trackInfo.type;
        info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
        info.cropStart = GenTime(0);
        info.startPos = GenTime(-1);
        info.track = 0;
        emit updateEffect(nullptr, m_trackindex, oldeffect, neweffect, ix, false);
    } else if (m_status == TIMELINE_CLIP) {
        emit updateEffect(m_clipref, -1, oldeffect, neweffect, ix, false);
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }

    QVBoxLayout *l = static_cast<QVBoxLayout *>(m_effect->container->widget()->layout());
    int groupPos = 0;
    CollapsibleEffect *effectToMove = nullptr;
    for (int i = 0; i < m_effects.count(); ++i) {
        if (m_effects.at(i)->effectIndex() == ix) {
            effectToMove = m_effects.at(i);
            groupPos = l->indexOf(effectToMove);
            l->removeWidget(effectToMove);
            break;
        }
    }

    CollapsibleGroup *group = new CollapsibleGroup(m_groupIndex, false, ix == m_currentEffectList.count() - 2, effectinfo, m_effect->container->widget());
    m_groupIndex++;
    connectGroup(group);
    l->insertWidget(groupPos, group);
    group->installEventFilter(this);
    if (effectToMove) {
        group->addGroupEffect(effectToMove);
    }
}

void EffectStackView2::connectGroup(CollapsibleGroup *group)
{
    connect(group, &AbstractCollapsibleWidget::moveEffect, this, &EffectStackView2::slotMoveEffect);
    connect(group, &AbstractCollapsibleWidget::addEffect, this, &EffectStackView2::slotAddEffect);
    connect(group, &CollapsibleGroup::unGroup, this, &EffectStackView2::slotUnGroup);
    connect(group, &CollapsibleGroup::groupRenamed, this, &EffectStackView2::slotRenameGroup);
    connect(group, &AbstractCollapsibleWidget::reloadEffects, this, &EffectStackView2::reloadEffects);
    connect(group, &CollapsibleGroup::deleteGroup, this, &EffectStackView2::slotDeleteGroup);
    connect(group, &AbstractCollapsibleWidget::changeEffectPosition, this, &EffectStackView2::slotMoveEffectUp);
}

void EffectStackView2::slotMoveEffect(const QList<int> &currentIndexes, int newIndex, int groupIndex, const QString &groupName)
{
    if (currentIndexes.count() == 1) {
        CollapsibleEffect *effectToMove = getEffectByIndex(currentIndexes.at(0));
        CollapsibleEffect *destination = getEffectByIndex(newIndex);
        if (destination && !destination->isMovable()) {
            newIndex++;
        }
        if (effectToMove == nullptr) {
            return;
        }

        QDomElement oldeffect = effectToMove->effect();
        QDomElement neweffect = oldeffect.cloneNode().toElement();

        EffectInfo effectinfo;
        effectinfo.fromString(oldeffect.attribute(QStringLiteral("kdenlive_info")));
        effectinfo.groupIndex = groupIndex;
        effectinfo.groupName = groupName;
        neweffect.setAttribute(QStringLiteral("kdenlive_info"), effectinfo.toString());

        if (oldeffect.attribute(QStringLiteral("kdenlive_info")) != effectinfo.toString()) {
            // effect's group info or collapsed state changed
            ItemInfo info;
            if (m_status == TIMELINE_TRACK) {
                info.track = m_trackInfo.type;
                info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
                info.cropStart = GenTime(0);
                info.startPos = GenTime(-1);
                info.track = 0;
                emit updateEffect(nullptr, m_trackindex, oldeffect, neweffect, effectToMove->effectIndex(), false);
            } else if (m_status == TIMELINE_CLIP) {
                emit updateEffect(m_clipref, -1, oldeffect, neweffect, effectToMove->effectIndex(), false);
            } else if (m_status == MASTER_CLIP) {
                //emit updateEffect(m_masterclipref, oldeffect, neweffect, effectToMove->effectIndex(),false);
            }
        }
    }

    // Update effect index with new position
    if (m_status == TIMELINE_TRACK) {
        emit changeEffectPosition(nullptr, m_trackindex, currentIndexes, newIndex);
    } else if (m_status == TIMELINE_CLIP) {
        emit changeEffectPosition(m_clipref, -1, currentIndexes, newIndex);
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }
}

void EffectStackView2::slotUnGroup(CollapsibleGroup *group)
{
    QVBoxLayout *l = static_cast<QVBoxLayout *>(m_effect->container->widget()->layout());
    int ix = l->indexOf(group);
    group->removeGroup(ix, l);
    group->deleteLater();
}

void EffectStackView2::slotRenameGroup(CollapsibleGroup *group)
{
    QList<CollapsibleEffect *> effects = group->effects();
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement origin = effects.at(i)->effect();
        QDomElement changed = origin.cloneNode().toElement();
        changed.setAttribute(QStringLiteral("kdenlive_info"), effects.at(i)->infoString());
        if (m_status == TIMELINE_TRACK) {
            emit updateEffect(nullptr, m_trackindex, origin, changed, effects.at(i)->effectIndex(), false);
        } else if (m_status == TIMELINE_CLIP) {
            emit updateEffect(m_clipref, -1, origin, changed, effects.at(i)->effectIndex(), false);
        } else if (m_status == MASTER_CLIP) {
            //TODO
        }
    }
}

void EffectStackView2::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
        event->acceptProposedAction();
    }
}

void EffectStackView2::processDroppedEffect(QDomElement e, QDropEvent *event)
{
    int ix = e.attribute(QStringLiteral("kdenlive_ix")).toInt();
    if (e.tagName() == QLatin1String("effectgroup")) {
        // We are dropping a group, all effects in group should be moved
        QDomNodeList effects = e.elementsByTagName(QStringLiteral("effect"));
        if (effects.isEmpty()) {
            event->ignore();
            return;
        }
        EffectInfo info;
        info.fromString(effects.at(0).toElement().attribute(QStringLiteral("kdenlive_info")));
        if (info.groupIndex < 0) {
            //qCDebug(KDENLIVE_LOG)<<"// ADDING EFFECT!!!";
            // Adding a new group effect to the stack
            event->setDropAction(Qt::CopyAction);
            event->accept();
            slotAddEffect(e);
            return;
        }
        // Moving group: delete all effects and re-add them
        QList<int> indexes;
        for (int i = 0; i < effects.count(); ++i) {
            QDomElement effect = effects.at(i).cloneNode().toElement();
            indexes << effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
        }
        //qCDebug(KDENLIVE_LOG)<<"// Moving: "<<indexes<<" TO "<<m_currentEffectList.count();
        slotMoveEffect(indexes, m_currentEffectList.count(), info.groupIndex, info.groupName);
    } else if (ix == 0) {
        // effect dropped from effects list, add it
        e.setAttribute(QStringLiteral("kdenlive_ix"), m_currentEffectList.count() + 1);
        event->setDropAction(Qt::CopyAction);
        event->accept();
        slotAddEffect(e);
        return;
    } else {
        // User is moving an effect
        if (e.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
            // Speed effect cannot be moved
            event->ignore();
            return;
        }
        slotMoveEffect(QList<int> () << ix, m_currentEffectList.count() + 1, -1);
    }
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void EffectStackView2::dropEvent(QDropEvent *event)
{
    const QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
    //event->acceptProposedAction();
    QDomDocument doc;
    doc.setContent(effects, true);
    processDroppedEffect(doc.documentElement(), event);
}

void EffectStackView2::setKeyframes(const QString &tag, const QString &keyframes)
{
    for (int i = 0; i < m_effects.count(); ++i) {
        if (m_effects.at(i)->isActive()) {
            m_effects.at(i)->setKeyframes(tag, keyframes);
            break;
        }
    }
}

//static
const QString EffectStackView2::getStyleSheet()
{
    KColorScheme scheme(QApplication::palette().currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor selected_bg = scheme.decoration(KColorScheme::FocusColor).color();
    QColor hgh = KColorUtils::mix(QApplication::palette().window().color(), selected_bg, 0.2);
    QColor hover_bg = scheme.decoration(KColorScheme::HoverColor).color();
    QColor light_bg = scheme.shade(KColorScheme::LightShade);
    QColor alt_bg = scheme.background(KColorScheme::NormalBackground).color();

    QString stylesheet;

    // effect background
    stylesheet.append(QStringLiteral("QFrame#decoframe {border-top-left-radius:5px;border-top-right-radius:5px;border-bottom:2px solid palette(mid);border-top:1px solid palette(light);} QFrame#decoframe[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // effect in group background
    stylesheet.append(QStringLiteral("QFrame#decoframesub {border-top:1px solid palette(light);}  QFrame#decoframesub[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // group background
    stylesheet.append(QStringLiteral("QFrame#decoframegroup {border-top-left-radius:5px;border-top-right-radius:5px;border:2px solid palette(dark);margin:0px;margin-top:2px;} "));

    // effect title bar
    stylesheet.append(QStringLiteral("QFrame#frame {margin-bottom:2px;border-top-left-radius:5px;border-top-right-radius:5px;}  QFrame#frame[target=\"true\"] {background: palette(highlight);}"));

    // group effect title bar
    stylesheet.append(QStringLiteral("QFrame#framegroup {border-top-left-radius:2px;border-top-right-radius:2px;background: palette(dark);}  QFrame#framegroup[target=\"true\"] {background: palette(highlight);} "));

    // draggable effect bar content
    stylesheet.append(QStringLiteral("QProgressBar::chunk:horizontal {background: palette(button);border-top-left-radius: 4px;border-bottom-left-radius: 4px;} QProgressBar::chunk:horizontal#dragOnly {background: %1;border-top-left-radius: 4px;border-bottom-left-radius: 4px;} QProgressBar::chunk:horizontal:hover {background: %2;}").arg(alt_bg.name(), selected_bg.name()));

    // draggable effect bar
    stylesheet.append(QStringLiteral("QProgressBar:horizontal {border: 1px solid palette(dark);border-top-left-radius: 4px;border-bottom-left-radius: 4px;border-right:0px;background:%3;padding: 0px;text-align:left center} QProgressBar:horizontal:disabled {border: 1px solid palette(button)} QProgressBar:horizontal#dragOnly {background: %3} QProgressBar:horizontal[inTimeline=\"true\"] { border: 1px solid %1;border-right: 0px;background: %2;padding: 0px;text-align:left center } QProgressBar::chunk:horizontal[inTimeline=\"true\"] {background: %1;}").arg(hover_bg.name(), light_bg.name(), alt_bg.name()));

    // spin box for draggable widget
    stylesheet.append(QStringLiteral("QAbstractSpinBox#dragBox {border: 1px solid palette(dark);border-top-right-radius: 4px;border-bottom-right-radius: 4px;padding-right:0px;} QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox:disabled#dragBox {border: 1px solid palette(button);} QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { border: 1px solid %1;} QAbstractSpinBox:hover#dragBox {border: 1px solid %2;} ").arg(hover_bg.name(), selected_bg.name()));

    // group editable labels
    stylesheet.append(QStringLiteral("MyEditableLabel { background-color: transparent; color: palette(bright-text); border-radius: 2px;border: 1px solid transparent;} MyEditableLabel:hover {border: 1px solid palette(highlight);} "));

    // transparent qcombobox
    stylesheet.append(QStringLiteral("QComboBox { background-color: transparent;} "));

    return stylesheet;
}

void EffectStackView2::disableBinEffects(bool disable)
{
    if (disable) {
        if (m_stateStatus == NORMALSTATUS) {
            m_stateStatus = DISABLEBIN;
        } else if (m_stateStatus == DISABLETIMELINE) {
            m_stateStatus = DISABLEALL;
        }
    } else {
        if (m_stateStatus == DISABLEBIN) {
            m_stateStatus = NORMALSTATUS;
        } else if (m_stateStatus == DISABLEALL) {
            m_stateStatus = DISABLETIMELINE;
        }
    }
    if (m_status == MASTER_CLIP) {
        m_effect->setEnabled(!disable);
    }
}

void EffectStackView2::disableTimelineEffects(bool disable)
{
    if (disable) {
        if (m_stateStatus == NORMALSTATUS) {
            m_stateStatus = DISABLETIMELINE;
        } else if (m_stateStatus == DISABLEBIN) {
            m_stateStatus = DISABLEALL;
        }
    } else {
        if (m_stateStatus == DISABLETIMELINE) {
            m_stateStatus = NORMALSTATUS;
        } else if (m_stateStatus == DISABLEALL) {
            m_stateStatus = DISABLEBIN;
        }
    }
    if (m_status == TIMELINE_CLIP || m_status == TIMELINE_TRACK) {
        m_effect->setEnabled(!disable);
    }
}

void EffectStackView2::slotSwitchCompare(bool enable)
{
    int pos = 0;
    if (enable) {
        if (m_status == TIMELINE_CLIP) {
            pos = (m_effectMetaInfo.monitor->position() - m_clipref->startPos()).frames(KdenliveSettings::project_fps());
        } else {
            pos = m_effectMetaInfo.monitor->position().frames(KdenliveSettings::project_fps());
        }
        m_effectMetaInfo.monitor->slotSwitchCompare(enable, pos);
    } else {
        if (m_status == TIMELINE_CLIP) {
            pos = (m_effectMetaInfo.monitor->position() + m_clipref->startPos()).frames(KdenliveSettings::project_fps());
        } else {
            pos = m_effectMetaInfo.monitor->position().frames(KdenliveSettings::project_fps());
        }
        m_effectMetaInfo.monitor->slotSwitchCompare(enable, pos);
    }
}

