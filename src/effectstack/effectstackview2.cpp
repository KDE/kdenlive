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
#include "project/projectlist.h"
#include "effectslist/effectslist.h"
#include "timeline/clipitem.h"
#include "monitor/monitoreditwidget.h"
#include "monitor/monitorscene.h"
#include "utils/KoIconUtils.h"
#include "mltcontroller/clipcontroller.h"

#include <QDebug>
#include <klocalizedstring.h>
#include <KColorScheme>
#include <QFontDatabase>
#include <KColorUtils>

#include <QScrollBar>
#include <QDrag>
#include <QMimeData>

EffectStackView2::EffectStackView2(QWidget *parent) :
        QWidget(parent),
        m_clipref(NULL),
        m_masterclipref(NULL),
        m_status(EMPTY),
        m_trackindex(-1),
        m_draggedEffect(NULL),
        m_draggedGroup(NULL),
        m_groupIndex(0),
        m_monitorSceneWanted(false),
        m_trackInfo()
{
    m_effectMetaInfo.monitor = NULL;
    m_effects = QList <CollapsibleEffect*>();
    setAcceptDrops(true);

    m_ui.setupUi(this);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_ui.checkAll->setToolTip(i18n("Enable/Disable all effects"));
    m_ui.buttonShowComments->setIcon(KoIconUtils::themedIcon("help-about"));
    m_ui.buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));

    connect(m_ui.checkAll, SIGNAL(stateChanged(int)), this, SLOT(slotCheckAll(int)));
    connect(m_ui.buttonShowComments, SIGNAL(clicked()), this, SLOT(slotShowComments()));
    m_ui.labelComment->setHidden(true);

    setEnabled(false);


    setStyleSheet(getStyleSheet());
}

EffectStackView2::~EffectStackView2()
{
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
        if (ic.isNull()) continue;
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
    QList<QToolButton *> allButtons = this->findChildren<QToolButton *>();
    for (int i = 0; i < allButtons.count(); i++) {
        QToolButton *m = allButtons.at(i);
        QIcon ic = m->icon();
        if (ic.isNull()) continue;
        QIcon newIcon = KoIconUtils::themedIcon(ic.name());
        m->setIcon(newIcon);
    }
}

void EffectStackView2::slotRenderPos(int pos)
{
    if (m_effects.isEmpty()) return;
    if (m_monitorSceneWanted) slotCheckMonitorPosition(pos);
    if (m_status == TIMELINE_CLIP && m_clipref) pos = pos - m_clipref->startPos().frames(KdenliveSettings::project_fps());

    for (int i = 0; i< m_effects.count(); ++i)
        m_effects.at(i)->slotSyncEffectsPos(pos);
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

void EffectStackView2::slotClipItemSelected(ClipItem* c, Monitor *m)
{
    m_masterclipref = NULL;
    m_trackindex = -1;
    if (c && !c->isEnabled()) return;
    if (c && c == m_clipref) {
    } else {
        m_effectMetaInfo.monitor = m;
        if (m_clipref) disconnect(m_clipref, SIGNAL(updateRange()), this, SLOT(slotClipItemUpdate()));
        m_clipref = c;
        if (c) {
            connect(m_clipref, SIGNAL(updateRange()), this, SLOT(slotClipItemUpdate()));
            QString cname = m_clipref->clipName();
            if (cname.length() > 30) {
                m_ui.checkAll->setToolTip(i18n("Effects for %1", cname));
                cname.truncate(27);
                m_ui.checkAll->setText(i18n("Effects for %1", cname) + "...");
            } else {
                m_ui.checkAll->setToolTip(QString());
                m_ui.checkAll->setText(i18n("Effects for %1", cname));
            }
            m_ui.checkAll->setEnabled(true);
            //TODO
            int frameWidth = c->binClip()->getProducerIntProperty("meta.media.width");
            int frameHeight = c->binClip()->getProducerIntProperty("meta.media.height");
            double factor = c->binClip()->getProducerProperty("aspect_ratio").toDouble();
            m_effectMetaInfo.frameSize = QPoint((int)(frameWidth * factor + 0.5), frameHeight);
        }
    }
    if (m_clipref == NULL) {
        //TODO: clear list, reset paramdesc and info
        // If monitor scene is displayed, hide it
        if (m_monitorSceneWanted) {
            m_effectMetaInfo.monitor->slotShowEffectScene(false);
            m_monitorSceneWanted = false;
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

void EffectStackView2::slotMasterClipItemSelected(ClipController* c, Monitor *m)
{
    m_clipref = NULL;
    m_trackindex = -1;
    if (c && c == m_masterclipref) {
    } else {
        m_masterclipref = c;
        m_effectMetaInfo.monitor = m;
        if (m_masterclipref) {
            QString cname = m_masterclipref->clipName();
            if (cname.length() > 30) {
                m_ui.checkAll->setToolTip(i18n("Bin effects for %1", cname));
                cname.truncate(27);
                m_ui.checkAll->setText(i18n("Bin effects for %1", cname) + "...");
            } else {
                m_ui.checkAll->setToolTip(QString());
                m_ui.checkAll->setText(i18n("Bin effects for %1", cname));
            }
            m_ui.checkAll->setEnabled(true);
            //TODO
            int frameWidth = m_masterclipref->int_property("meta.media.width");
            int frameHeight = m_masterclipref->int_property("meta.media.height");
            double factor = m_masterclipref->double_property("aspect_ratio");
            m_effectMetaInfo.frameSize = QPoint((int)(frameWidth * factor + 0.5), frameHeight);
        }
    }
    if (m_masterclipref == NULL) {
        //TODO: clear list, reset paramdesc and info
        // If monitor scene is displayed, hide it
        if (m_monitorSceneWanted) {
            m_effectMetaInfo.monitor->slotShowEffectScene(false);
            m_monitorSceneWanted = false;
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
    if (m_status == TIMELINE_TRACK && ix == m_trackindex) {
        // Track effects already displayed
        return;
    }
    m_clipref = NULL;
    m_status = TIMELINE_TRACK;
    m_effectMetaInfo.monitor = m;
    m_currentEffectList = info.effectsList;
    m_trackInfo = info;
    m_clipref = NULL;
    m_masterclipref = NULL;
    setEnabled(true);
    m_ui.checkAll->setToolTip(QString());
    m_ui.checkAll->setText(i18n("Effects for track %1", info.trackName.isEmpty() ? QString::number(ix) : info.trackName));
    m_ui.checkAll->setEnabled(true);
    m_trackindex = ix;
    setupListView();
}


void EffectStackView2::setupListView()
{
    blockSignals(true);
    m_monitorSceneWanted = false;
    m_draggedEffect = NULL;
    m_draggedGroup = NULL;
    disconnect(m_effectMetaInfo.monitor, SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    m_effects.clear();
    m_groupIndex = 0;
    QWidget *view = m_ui.container->takeWidget();
    if (view) {
        delete view;
    }
    blockSignals(false);
    view = new QWidget(m_ui.container);
    m_ui.container->setWidget(view);

    QVBoxLayout *vbox1 = new QVBoxLayout(view);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);

    int effectsCount = m_currentEffectList.count();

    // Make sure we always have one effect selected
    if (m_status == TIMELINE_CLIP) {
        int selectedEffect = m_clipref->selectedEffectIndex();
        if (selectedEffect < 1 && effectsCount > 0) m_clipref->setSelectedEffect(1);
        else if (selectedEffect > effectsCount) m_clipref->setSelectedEffect(effectsCount);
    }

    for (int i = 0; i < effectsCount; ++i) {
        QDomElement d = m_currentEffectList.at(i).cloneNode().toElement();
        if (d.isNull()) {
            //qDebug() << " . . . . WARNING, NULL EFFECT IN STACK!!!!!!!!!";
            continue;
        }

        CollapsibleGroup *group = NULL;
        EffectInfo effectInfo;
        effectInfo.fromString(d.attribute("kdenlive_info"));
        if (effectInfo.groupIndex >= 0) {
            // effect is in a group
            for (int j = 0; j < vbox1->count(); ++j) {
                CollapsibleGroup *eff = static_cast<CollapsibleGroup *>(vbox1->itemAt(j)->widget());
                if (eff->isGroup() &&  eff->groupIndex() == effectInfo.groupIndex) {
                    group = eff;
                    break;
                }
            }

            if (group == NULL) {
                group = new CollapsibleGroup(effectInfo.groupIndex, i == 0, i == effectsCount - 1, effectInfo, m_ui.container->widget());
                connectGroup(group);
                vbox1->addWidget(group);
                group->installEventFilter( this );
            }
            if (effectInfo.groupIndex >= m_groupIndex) m_groupIndex = effectInfo.groupIndex + 1;
        }

        /*QDomDocument doc;
        doc.appendChild(doc.importNode(d, true));
        //qDebug() << "IMPORTED STK: " << doc.toString();*/

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

        CollapsibleEffect *currentEffect = new CollapsibleEffect(d, m_currentEffectList.at(i), info, &m_effectMetaInfo, i == effectsCount - 1, view);
        if (m_status == TIMELINE_TRACK) {
            isSelected = currentEffect->effectIndex() == 1;
        }
        else if (m_status == TIMELINE_CLIP) {
            isSelected = currentEffect->effectIndex() == m_clipref->selectedEffectIndex();
        }
        else if (m_status == MASTER_CLIP) {
            isSelected = currentEffect->effectIndex() == m_masterclipref->selectedEffectIndex;
        }
        if (isSelected) {
            if (currentEffect->needsMonitorEffectScene()) m_monitorSceneWanted = true;
        }
        currentEffect->setActive(isSelected);
        m_effects.append(currentEffect);
        if (group) {
            group->addGroupEffect(currentEffect);
        } else {
            vbox1->addWidget(currentEffect);
        }
        connectEffect(currentEffect);
    }

    if (m_currentEffectList.isEmpty()) {
        m_ui.labelComment->setHidden(true);
    }
    else {
        // Adjust group effects (up / down buttons)
        QList<CollapsibleGroup *> allGroups = m_ui.container->widget()->findChildren<CollapsibleGroup *>();
        for (int i = 0; i < allGroups.count(); ++i) {
            allGroups.at(i)->adjustEffects();
        }
        connect(m_effectMetaInfo.monitor, SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    }

    vbox1->addStretch(10);
    slotUpdateCheckAllButton();
    // show monitor scene if necessary
    if (!m_monitorSceneWanted) m_effectMetaInfo.monitor->slotShowEffectScene(false);

    // Wait a little bit for the new layout to be ready, then check if we have a scrollbar
    QTimer::singleShot(200, this, SLOT(slotCheckWheelEventFilter()));
}

void EffectStackView2::connectEffect(CollapsibleEffect *currentEffect)
{
    // Check drag & drop
    currentEffect->installEventFilter( this );
    connect(currentEffect, SIGNAL(parameterChanged(QDomElement,QDomElement,int)), this , SLOT(slotUpdateEffectParams(QDomElement,QDomElement,int)));
    connect(currentEffect, SIGNAL(startFilterJob(QMap<QString,QString>&, QMap<QString,QString>&,QMap <QString, QString>&)), this , SLOT(slotStartFilterJob(QMap<QString,QString>&, QMap<QString,QString>&,QMap <QString, QString>&)));
    connect(currentEffect, SIGNAL(deleteEffect(QDomElement)), this , SLOT(slotDeleteEffect(QDomElement)));
    connect(currentEffect, SIGNAL(reloadEffects()), this , SIGNAL(reloadEffects()));
    connect(currentEffect, SIGNAL(resetEffect(int)), this , SLOT(slotResetEffect(int)));
    connect(currentEffect, SIGNAL(changeEffectPosition(QList<int>,bool)), this , SLOT(slotMoveEffectUp(QList<int>,bool)));
    connect(currentEffect, SIGNAL(effectStateChanged(bool,int,bool)), this, SLOT(slotUpdateEffectState(bool,int,bool)));
    connect(currentEffect, SIGNAL(activateEffect(int)), this, SLOT(slotSetCurrentEffect(int)));
    connect(currentEffect, SIGNAL(seekTimeline(int)), this , SLOT(slotSeekTimeline(int)));
    connect(currentEffect, SIGNAL(createGroup(int)), this , SLOT(slotCreateGroup(int)));
    connect(currentEffect, SIGNAL(moveEffect(QList<int>,int,int,QString)), this , SLOT(slotMoveEffect(QList<int>,int,int,QString)));
    connect(currentEffect, SIGNAL(addEffect(QDomElement)), this , SLOT(slotAddEffect(QDomElement)));
    connect(currentEffect, SIGNAL(createRegion(int,QUrl)), this, SLOT(slotCreateRegion(int,QUrl)));
    connect(currentEffect, SIGNAL(deleteGroup(QDomDocument)), this , SLOT(slotDeleteGroup(QDomDocument)));
    connect(currentEffect, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));
}

void EffectStackView2::slotCheckWheelEventFilter()
{
    // If the effect stack widget has no scrollbar, we will not filter the
    // mouse wheel events, so that user can easily adjust effect params
    bool filterWheelEvent = false;
    if (m_ui.container->verticalScrollBar() && m_ui.container->verticalScrollBar()->isVisible()) {
        // widget has scroll bar,
        filterWheelEvent = true;
    }
    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->filterWheelEvent = filterWheelEvent;
    }
}

void EffectStackView2::resizeEvent ( QResizeEvent * event )
{
    slotCheckWheelEventFilter();
    QWidget::resizeEvent(event);
}

bool EffectStackView2::eventFilter( QObject * o, QEvent * e )
{
    // Check if user clicked in an effect's top bar to start dragging it
    if (e->type() == QEvent::MouseButtonPress)  {
        m_draggedEffect = qobject_cast<CollapsibleEffect*>(o);
        if (m_draggedEffect) {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton && (m_draggedEffect->frame->underMouse() || m_draggedEffect->title->underMouse())) {
                m_clickPoint = me->globalPos();
            }
            else {
                m_clickPoint = QPoint();
                m_draggedEffect = NULL;
            }
            e->accept();
            return true;
        }
        m_draggedGroup = qobject_cast<CollapsibleGroup*>(o);
        if (m_draggedGroup) {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton && (m_draggedGroup->frame->underMouse() || m_draggedGroup->title()->underMouse()))
                m_clickPoint = me->globalPos();
            else {
                m_clickPoint = QPoint();
                m_draggedGroup = NULL;
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

void EffectStackView2::mouseMoveEvent(QMouseEvent * event)
{
    if (m_draggedEffect || m_draggedGroup) {
        if ((event->buttons() & Qt::LeftButton) && (m_clickPoint != QPoint()) && ((event->globalPos() - m_clickPoint).manhattanLength() >= QApplication::startDragDistance())) {
            startDrag();
        }
    }
}

void EffectStackView2::mouseReleaseEvent(QMouseEvent * event)
{
    m_draggedEffect = NULL;
    m_draggedGroup = NULL;
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
	    effect.setAttribute("clipstart", 0);
	}
	else {
	    // Keep clip crop start in case we want to paste effect
	    effect.setAttribute("clipstart", m_clipref->cropStart().frames(KdenliveSettings::project_fps()));
	}
        doc.appendChild(doc.importNode(effect, true));
        pixmap = m_draggedEffect->title->grab();
    }
    else if (m_draggedGroup) {
        doc = m_draggedGroup->effectsData();
	if (m_status == TIMELINE_TRACK || m_status == MASTER_CLIP) {
	    doc.documentElement().setAttribute("clipstart", 0);
	}
	else {
	    doc.documentElement().setAttribute("clipstart", m_clipref->cropStart().frames(KdenliveSettings::project_fps()));
	}
        pixmap = m_draggedGroup->title()->grab();
    }
    else return;
    QDrag *drag = new QDrag(this);
    drag->setPixmap(pixmap);
    QMimeData *mime = new QMimeData;
    QByteArray data;
    data.append(doc.toString().toUtf8());
    mime->setData("kdenlive/effectslist", data);

    // Assign ownership of the QMimeData object to the QDrag object.
    drag->setMimeData(mime);
    // Start the drag and drop operation
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}


void EffectStackView2::slotUpdateEffectState(bool disable, int index, bool needsMonitorEffectScene)
{
    if (m_monitorSceneWanted && disable) {
        m_effectMetaInfo.monitor->slotShowEffectScene(false);
        m_monitorSceneWanted = false;
    }
    else if (!disable && !m_monitorSceneWanted && needsMonitorEffectScene) {
        m_effectMetaInfo.monitor->slotShowEffectScene(true);
        m_monitorSceneWanted = true;
    }
    switch (m_status) {
        case TIMELINE_TRACK:
            emit changeEffectState(NULL, m_trackindex, QList <int>() << index, disable);
            break;
        case MASTER_CLIP:
            m_masterclipref->changeEffectState(QList <int>() << index, disable);
            m_effectMetaInfo.monitor->refreshMonitor();
            break;
        default:
            // timeline clip effect
            emit changeEffectState(m_clipref, -1, QList <int>() <<index, disable);
    }
    slotUpdateCheckAllButton();
}


void EffectStackView2::raiseWindow(QWidget* dock)
{
    if (m_status != EMPTY && dock)
        dock->raise();
}


void EffectStackView2::slotSeekTimeline(int pos)
{
    if (m_status == TIMELINE_TRACK) {
        emit seekTimeline(pos);
    } else if (m_status == TIMELINE_CLIP) {
        emit seekTimeline(m_clipref->startPos().frames(KdenliveSettings::project_fps()) + pos);
    } else if (m_status == MASTER_CLIP) {
        
    }
}


/*void EffectStackView2::slotRegionChanged()
{
    if (!m_trackMode) emit updateClipRegion(m_clipref, m_ui.effectlist->currentRow(), m_ui.region_url->text());
}*/

void EffectStackView2::slotCheckMonitorPosition(int renderPos)
{
    if (m_monitorSceneWanted) {
        if (m_status == TIMELINE_TRACK || m_status == MASTER_CLIP || (m_clipref && renderPos >= m_clipref->startPos().frames(KdenliveSettings::project_fps()) && renderPos <= m_clipref->endPos().frames(KdenliveSettings::project_fps()))) {
            if (!m_effectMetaInfo.monitor->effectSceneDisplayed()) {
                m_effectMetaInfo.monitor->slotShowEffectScene(true);
            }
        } else {
            m_effectMetaInfo.monitor->slotShowEffectScene(false);
        }
    }
    else {
        m_effectMetaInfo.monitor->slotShowEffectScene(false);
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
    m_monitorSceneWanted = false;
    QWidget *view = m_ui.container->takeWidget();
    if (view) {
        delete view;
    }
    m_ui.checkAll->setToolTip(QString());
    m_ui.checkAll->setText(QString());
    m_ui.checkAll->setEnabled(false);
    m_ui.labelComment->setText(QString());
    setEnabled(false);
}

void EffectStackView2::slotCheckAll(int state)
{
    if (state == Qt::PartiallyChecked) {
        state = Qt::Checked;
        m_ui.checkAll->blockSignals(true);
        m_ui.checkAll->setCheckState(Qt::Checked);
        m_ui.checkAll->blockSignals(false);
    }

    bool disabled = state == Qt::Unchecked;
    // Disable all effects
    QList <int> indexes;
    for (int i = 0; i < m_effects.count(); ++i) {
        m_effects.at(i)->slotDisable(disabled, false);
        indexes << m_effects.at(i)->effectIndex();
    }
    // Take care of groups
    QList<CollapsibleGroup *> allGroups = m_ui.container->widget()->findChildren<CollapsibleGroup *>();
    for (int i = 0; i < allGroups.count(); ++i) {
        allGroups.at(i)->slotEnable(disabled, false);
    }

    if (m_status == TIMELINE_TRACK)
        emit changeEffectState(NULL, m_trackindex, indexes, disabled);
    else if (m_status == TIMELINE_CLIP)
        emit changeEffectState(m_clipref, -1, indexes, disabled);
    else if (m_status == MASTER_CLIP)
        m_masterclipref->changeEffectState(indexes, disabled);
}

void EffectStackView2::slotUpdateCheckAllButton()
{
    bool hasEnabled = false;
    bool hasDisabled = false;

    for (int i = 0; i < m_effects.count(); ++i) {
        if (!m_effects.at(i)->isEnabled()) hasEnabled = true;
        else hasDisabled = true;
    }

    m_ui.checkAll->blockSignals(true);
    if (hasEnabled && hasDisabled)
        m_ui.checkAll->setCheckState(Qt::PartiallyChecked);
    else if (hasEnabled)
        m_ui.checkAll->setCheckState(Qt::Checked);
    else
        m_ui.checkAll->setCheckState(Qt::Unchecked);
    m_ui.checkAll->blockSignals(false);
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

void EffectStackView2::updateProjectFormat(const Timecode &t)
{
    m_effectMetaInfo.timecode = t;
}

void EffectStackView2::updateTimecodeFormat()
{
    for (int i = 0; i< m_effects.count(); ++i)
        m_effects.at(i)->updateTimecodeFormat();
}

CollapsibleEffect *EffectStackView2::getEffectByIndex(int ix)
{
    for (int i = 0; i< m_effects.count(); ++i) {
        if (m_effects.at(i)->effectIndex() == ix) {
            return m_effects.at(i);
        }
    }
    return NULL;
}

void EffectStackView2::slotUpdateEffectParams(const QDomElement &old, const QDomElement &e, int ix)
{
    if (m_status == TIMELINE_TRACK) {
        emit updateEffect(NULL, m_trackindex, old, e, ix,false);
    }
    else if (m_status == TIMELINE_CLIP && m_clipref) {
        emit updateEffect(m_clipref, -1, old, e, ix, false);
        // Make sure the changed effect is currently displayed
        slotSetCurrentEffect(ix);
    }
    else if (m_status == MASTER_CLIP) {
        m_masterclipref->updateEffect(m_effectMetaInfo.monitor->profileInfo(), e, ix);
        m_effectMetaInfo.monitor->refreshMonitor();
    }
    QTimer::singleShot(200, this, SLOT(slotCheckWheelEventFilter()));
}

void EffectStackView2::slotSetCurrentEffect(int ix)
{
    if (m_status == TIMELINE_CLIP) {
        if (m_clipref && ix != m_clipref->selectedEffectIndex()) {
            m_clipref->setSelectedEffect(ix);
            for (int i = 0; i < m_effects.count(); ++i) {
                if (m_effects.at(i)->effectIndex() == ix) {
                    if (m_effects.at(i)->isActive()) return;
                    m_effects.at(i)->setActive(true);
                    m_monitorSceneWanted = m_effects.at(i)->needsMonitorEffectScene();
                    slotCheckMonitorPosition(m_effectMetaInfo.monitor->render->seekFramePosition());
                    m_ui.labelComment->setText(i18n(m_effects.at(i)->effect().firstChildElement("description").firstChildElement("full").text().toUtf8().data()));
                    m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || m_ui.labelComment->text().isEmpty());
                }
                else m_effects.at(i)->setActive(false);
            }
        }
    }
}

void EffectStackView2::slotDeleteGroup(QDomDocument doc)
{
    QDomNodeList effects = doc.elementsByTagName("effect");
    ClipItem * clip = NULL;
    int ix;
    if (m_status == MASTER_CLIP) {
        //TODO
        return;
    }
    if (m_status == TIMELINE_TRACK) {
        ix = m_trackindex;
    }
    else if (m_status == TIMELINE_CLIP) {
        clip = m_clipref;
        ix = -1;
    }

    for (int i = 0; i < effects.count(); ++i)
        emit removeEffect(clip, ix, effects.at(i).toElement());
}

void EffectStackView2::slotDeleteEffect(const QDomElement &effect)
{
    if (m_status == TIMELINE_TRACK)
        emit removeEffect(NULL, m_trackindex, effect);
    else if (m_status == TIMELINE_CLIP)
        emit removeEffect(m_clipref, -1, effect);
    if (m_status == MASTER_CLIP) {
        emit removeMasterEffect(m_masterclipref->clipId(), effect);
    }
}

void EffectStackView2::slotAddEffect(const QDomElement &effect)
{
    emit addEffect(m_clipref, effect, m_trackindex);
}

void EffectStackView2::slotMoveEffectUp(const QList<int> &indexes, bool up)
{
    if (up && indexes.first() <= 1) return;
    if (!up && indexes.last() >= m_currentEffectList.count()) return;
    int endPos;
    if (up) {
        endPos = indexes.first() - 1;
    }
    else {
        endPos =  indexes.last() + 1;
    }
    if (m_status == TIMELINE_TRACK) emit changeEffectPosition(NULL, m_trackindex, indexes, endPos);
    else if (m_status == TIMELINE_CLIP) emit changeEffectPosition(m_clipref, -1, indexes, endPos);
    else if (m_status == MASTER_CLIP) {
        //TODO
    }
}

void EffectStackView2::slotStartFilterJob(QMap <QString, QString> &filterParams, QMap <QString, QString> &consumerParams, QMap <QString, QString> &extraParams)
{
    if (!m_clipref) return;
    emit startFilterJob(m_clipref->info(), m_clipref->getBinId(), filterParams, consumerParams, extraParams);
}

void EffectStackView2::slotResetEffect(int ix)
{
    QDomElement old = m_currentEffectList.itemFromIndex(ix);
    QDomElement dom;
    QString effectId = old.attribute("id");
    QMap<QString, EffectsList*> effectLists;
    effectLists["audio"] = &MainWindow::audioEffects;
    effectLists["video"] = &MainWindow::videoEffects;
    effectLists["custom"] = &MainWindow::customEffects;
    foreach(const EffectsList* list, effectLists) {
        dom = list->getEffectByTag(QString(), effectId).cloneNode().toElement();
        if (!dom.isNull()) break;
    }
    if (!dom.isNull()) {
        dom.setAttribute("kdenlive_ix", old.attribute("kdenlive_ix"));
        if (m_status == TIMELINE_TRACK) {
            EffectsList::setParameter(dom, "in", QString::number(0));
            EffectsList::setParameter(dom, "out", QString::number(m_trackInfo.duration));
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
            emit updateEffect(NULL, m_trackindex, old, dom, ix,false);
        } else if (m_status == TIMELINE_CLIP) {
            m_clipref->initEffect(m_effectMetaInfo.monitor->profileInfo(), dom);
            for (int i = 0; i < m_effects.count(); ++i) {
                if (m_effects.at(i)->effectIndex() == ix) {
                    m_effects.at(i)->updateWidget(m_clipref->info(), dom, &m_effectMetaInfo);
                    break;
                }
            }
            //m_ui.region_url->setUrl(QUrl(dom.attribute("region")));
            emit updateEffect(m_clipref, -1, old, dom, ix,false);
        } else if (m_status == MASTER_CLIP) {
            //TODO
        }
    }

    emit showComments(m_ui.buttonShowComments->isChecked());
    m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || m_ui.labelComment->text().isEmpty());
}

void EffectStackView2::slotShowComments()
{
    m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || m_ui.labelComment->text().isEmpty());
    emit showComments(m_ui.buttonShowComments->isChecked());
}

void EffectStackView2::slotCreateRegion(int ix, QUrl url)
{
    QDomElement oldeffect = m_currentEffectList.itemFromIndex(ix);
    QDomElement neweffect = oldeffect.cloneNode().toElement();
    QDomElement region = MainWindow::videoEffects.getEffectByTag("region", "region").cloneNode().toElement();
    region.appendChild(region.ownerDocument().importNode(neweffect, true));
    region.setAttribute("kdenlive_ix", ix);
    EffectsList::setParameter(region, "resource", url.path());
    if (m_status == TIMELINE_TRACK) {
        emit updateEffect(NULL, m_trackindex, oldeffect, region, ix,false);
    }
    else if (m_status == TIMELINE_CLIP && m_clipref) {
        emit updateEffect(m_clipref, -1, oldeffect, region, ix, false);
        // Make sure the changed effect is currently displayed
        //slotSetCurrentEffect(ix);
    }
    else if (m_status == MASTER_CLIP) {
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
    }
    else if (m_status == TIMELINE_CLIP && m_clipref) {
        info = m_clipref->info();
    }
    else if (m_status == MASTER_CLIP) {
        //TODO
    }
    CollapsibleEffect *current = getEffectByIndex(ix);
    m_effects.removeAll(current);
    current->setEnabled(false);
    m_currentEffectList.removeAt(ix);
    m_currentEffectList.insert(region);
    current->deleteLater();
    CollapsibleEffect *currentEffect = new CollapsibleEffect(region, m_currentEffectList.itemFromIndex(ix), info, &m_effectMetaInfo, ix == m_currentEffectList.count() - 1, m_ui.container->widget());
    connectEffect(currentEffect);

    if (m_status == TIMELINE_TRACK) {
        isSelected = currentEffect->effectIndex() == 1;
    }
    else if (m_status == TIMELINE_CLIP && m_clipref) {
        isSelected = currentEffect->effectIndex() == m_clipref->selectedEffectIndex();
    }
    else if (m_status == MASTER_CLIP) {
        //TODO
    }
    if (isSelected) currentEffect->setActive(true);
    m_effects.append(currentEffect);
    // TODO: region in group?
    //if (group) {
    //	group->addGroupEffect(currentEffect);
    //} else {
    QVBoxLayout *vbox = static_cast <QVBoxLayout *> (m_ui.container->widget()->layout());
    vbox->insertWidget(ix, currentEffect);
    //}

    // Check drag & drop
    currentEffect->installEventFilter( this );

    QTimer::singleShot(200, this, SLOT(slotCheckWheelEventFilter()));

}

void EffectStackView2::slotCreateGroup(int ix)
{
    QDomElement oldeffect = m_currentEffectList.itemFromIndex(ix);
    QDomElement neweffect = oldeffect.cloneNode().toElement();
    EffectInfo effectinfo;
    effectinfo.fromString(oldeffect.attribute("kdenlive_info"));
    effectinfo.groupIndex = m_groupIndex;
    neweffect.setAttribute("kdenlive_info", effectinfo.toString());

    ItemInfo info;
    if (m_status == TIMELINE_TRACK) {
        info.track = m_trackInfo.type;
        info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
        info.cropStart = GenTime(0);
        info.startPos = GenTime(-1);
        info.track = 0;
        emit updateEffect(NULL, m_trackindex, oldeffect, neweffect, ix, false);
    } else if (m_status == TIMELINE_CLIP) {
        emit updateEffect(m_clipref, -1, oldeffect, neweffect, ix, false);
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }

    QVBoxLayout *l = static_cast<QVBoxLayout *>(m_ui.container->widget()->layout());
    int groupPos = 0;
    CollapsibleEffect *effectToMove = NULL;
    for (int i = 0; i < m_effects.count(); ++i) {
        if (m_effects.at(i)->effectIndex() == ix) {
            effectToMove = m_effects.at(i);
            groupPos = l->indexOf(effectToMove);
            l->removeWidget(effectToMove);
            break;
        }
    }

    CollapsibleGroup *group = new CollapsibleGroup(m_groupIndex, ix == 1, ix == m_currentEffectList.count() - 2, effectinfo, m_ui.container->widget());
    m_groupIndex++;
    connectGroup(group);
    l->insertWidget(groupPos, group);
    group->installEventFilter( this );
    if (effectToMove)
        group->addGroupEffect(effectToMove);
}

void EffectStackView2::connectGroup(CollapsibleGroup *group)
{
    connect(group, SIGNAL(moveEffect(QList<int>,int,int,QString)), this , SLOT(slotMoveEffect(QList<int>,int,int,QString)));
    connect(group, SIGNAL(addEffect(QDomElement)), this , SLOT(slotAddEffect(QDomElement)));
    connect(group, SIGNAL(unGroup(CollapsibleGroup*)), this , SLOT(slotUnGroup(CollapsibleGroup*)));
    connect(group, SIGNAL(groupRenamed(CollapsibleGroup*)), this , SLOT(slotRenameGroup(CollapsibleGroup*)));
    connect(group, SIGNAL(reloadEffects()), this , SIGNAL(reloadEffects()));
    connect(group, SIGNAL(deleteGroup(QDomDocument)), this , SLOT(slotDeleteGroup(QDomDocument)));
    connect(group, SIGNAL(changeEffectPosition(QList<int>,bool)), this , SLOT(slotMoveEffectUp(QList<int>,bool)));
}

void EffectStackView2::slotMoveEffect(QList <int> currentIndexes, int newIndex, int groupIndex, QString groupName)
{
    if (currentIndexes.count() == 1) {
        CollapsibleEffect *effectToMove = getEffectByIndex(currentIndexes.at(0));
        if (effectToMove == NULL) return;

        QDomElement oldeffect = effectToMove->effect();
        QDomElement neweffect = oldeffect.cloneNode().toElement();

        EffectInfo effectinfo;
        effectinfo.fromString(oldeffect.attribute("kdenlive_info"));
        effectinfo.groupIndex = groupIndex;
        effectinfo.groupName = groupName;
        neweffect.setAttribute("kdenlive_info", effectinfo.toString());

        if (oldeffect.attribute("kdenlive_info") != effectinfo.toString()) {
            // effect's group info or collapsed state changed
            ItemInfo info;
            if (m_status == TIMELINE_TRACK) {
                info.track = m_trackInfo.type;
                info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
                info.cropStart = GenTime(0);
                info.startPos = GenTime(-1);
                info.track = 0;
                emit updateEffect(NULL, m_trackindex, oldeffect, neweffect, effectToMove->effectIndex(),false);
            } else if (m_status == TIMELINE_CLIP) {
                emit updateEffect(m_clipref, -1, oldeffect, neweffect, effectToMove->effectIndex(),false);
            } else if (m_status == MASTER_CLIP) {
                //TODO
            }
        }
    }

    // Update effect index with new position
    if (m_status == TIMELINE_TRACK) {
        emit changeEffectPosition(NULL, m_trackindex, currentIndexes, newIndex);
    } else if (m_status == TIMELINE_CLIP) {
        emit changeEffectPosition(m_clipref, -1, currentIndexes, newIndex);
    } else if (m_status == MASTER_CLIP) {
        //TODO
    }
}

void EffectStackView2::slotUnGroup(CollapsibleGroup* group)
{
    QVBoxLayout *l = static_cast<QVBoxLayout *>(m_ui.container->widget()->layout());
    int ix = l->indexOf(group);
    group->removeGroup(ix, l);
    group->deleteLater();
}

void EffectStackView2::slotRenameGroup(CollapsibleGroup *group)
{
    QList <CollapsibleEffect*> effects = group->effects();
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement origin = effects.at(i)->effect();
        QDomElement changed = origin.cloneNode().toElement();
        changed.setAttribute("kdenlive_info", effects.at(i)->infoString());
        if (m_status == TIMELINE_TRACK) {
            emit updateEffect(NULL, m_trackindex, origin, changed, effects.at(i)->effectIndex(),false);
        } else if (m_status == TIMELINE_CLIP) {
            emit updateEffect(m_clipref, -1, origin, changed, effects.at(i)->effectIndex(),false);
        } else if (m_status == MASTER_CLIP) {
            //TODO
        }
    }
}

void EffectStackView2::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
        event->acceptProposedAction();
    }
}

void EffectStackView2::processDroppedEffect(QDomElement e, QDropEvent *event)
{
    int ix = e.attribute("kdenlive_ix").toInt();
    if (e.tagName() == "effectgroup") {
        // We are dropping a group, all effects in group should be moved
        QDomNodeList effects = e.elementsByTagName("effect");
        if (effects.count() == 0) {
            event->ignore();
            return;
        }
        EffectInfo info;
        info.fromString(effects.at(0).toElement().attribute("kdenlive_info"));
        if (info.groupIndex < 0) {
            //qDebug()<<"// ADDING EFFECT!!!";
            // Adding a new group effect to the stack
            event->setDropAction(Qt::CopyAction);
            event->accept();
            slotAddEffect(e);
            return;
        }
        // Moving group: delete all effects and re-add them
        QList <int> indexes;
        for (int i = 0; i < effects.count(); ++i) {
            QDomElement effect = effects.at(i).cloneNode().toElement();
            indexes << effect.attribute("kdenlive_ix").toInt();
        }
        //qDebug()<<"// Moving: "<<indexes<<" TO "<<m_currentEffectList.count();
        slotMoveEffect(indexes, m_currentEffectList.count(), info.groupIndex, info.groupName);
    }
    else if (ix == 0) {
        // effect dropped from effects list, add it
        e.setAttribute("kdenlive_ix", m_currentEffectList.count() + 1);
        event->setDropAction(Qt::CopyAction);
        event->accept();
        slotAddEffect(e);
        return;
    }
    else {
        // User is moving an effect
        slotMoveEffect(QList<int> () << ix, m_currentEffectList.count() + 1, -1);
    }
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void EffectStackView2::dropEvent(QDropEvent *event)
{
    const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
    //event->acceptProposedAction();
    QDomDocument doc;
    doc.setContent(effects, true);
    processDroppedEffect(doc.documentElement(), event);
}

void EffectStackView2::setKeyframes(const QString &data, int maximum)
{
    for (int i = 0; i < m_effects.count(); ++i) {
        if (m_effects.at(i)->isActive()) {
	    m_effects.at(i)->setKeyframes(data, maximum);
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
    stylesheet.append(QString("QFrame#decoframe {border-top-left-radius:5px;border-top-right-radius:5px;border-bottom:2px solid palette(mid);border-top:1px solid palette(light);} QFrame#decoframe[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // effect in group background
    stylesheet.append(QString("QFrame#decoframesub {border-top:1px solid palette(light);}  QFrame#decoframesub[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // group background
    stylesheet.append(QString("QFrame#decoframegroup {border-top-left-radius:5px;border-top-right-radius:5px;border:2px solid palette(dark);margin:0px;margin-top:2px;} "));

    // effect title bar
    stylesheet.append(QString("QFrame#frame {margin-bottom:2px;border-top-left-radius:5px;border-top-right-radius:5px;}  QFrame#frame[target=\"true\"] {background: palette(highlight);}"));

    // group effect title bar
    stylesheet.append(QString("QFrame#framegroup {border-top-left-radius:2px;border-top-right-radius:2px;background: palette(dark);}  QFrame#framegroup[target=\"true\"] {background: palette(highlight);} "));

    // draggable effect bar content
    stylesheet.append(QString("QProgressBar::chunk:horizontal {background: palette(button);border-top-left-radius: 4px;border-bottom-left-radius: 4px;} QProgressBar::chunk:horizontal#dragOnly {background: %1;border-top-left-radius: 4px;border-bottom-left-radius: 4px;} QProgressBar::chunk:horizontal:hover {background: %2;}").arg(alt_bg.name()).arg(selected_bg.name()));

    // draggable effect bar
    stylesheet.append(QString("QProgressBar:horizontal {border: 1px solid palette(dark);border-top-left-radius: 4px;border-bottom-left-radius: 4px;border-right:0px;background:%3;padding: 0px;text-align:left center} QProgressBar:horizontal:disabled {border: 1px solid palette(button)} QProgressBar:horizontal#dragOnly {background: %3} QProgressBar:horizontal[inTimeline=\"true\"] { border: 1px solid %1;border-right: 0px;background: %2;padding: 0px;text-align:left center } QProgressBar::chunk:horizontal[inTimeline=\"true\"] {background: %1;}").arg(hover_bg.name()).arg(light_bg.name()).arg(alt_bg.name()));

    // spin box for draggable widget
    stylesheet.append(QString("QAbstractSpinBox#dragBox {border: 1px solid palette(dark);border-top-right-radius: 4px;border-bottom-right-radius: 4px;padding-right:0px;} QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox:disabled#dragBox {border: 1px solid palette(button);} QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { border: 1px solid %1;} QAbstractSpinBox:hover#dragBox {border: 1px solid %2;} ").arg(hover_bg.name()).arg(selected_bg.name()));

    // group editable labels
    stylesheet.append(QString("MyEditableLabel { background-color: transparent; color: palette(bright-text); border-radius: 2px;border: 1px solid transparent;} MyEditableLabel:hover {border: 1px solid palette(highlight);} "));

    return stylesheet;
}



