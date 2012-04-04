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
#include "effectslist.h"
#include "clipitem.h"
#include "mainwindow.h"
#include "docclipbase.h"
#include "projectlist.h"
#include "kthumb.h"
#include "monitoreditwidget.h"
#include "monitorscene.h"
#include "kdenlivesettings.h"
#include "collapsibleeffect.h"
#include "collapsiblegroup.h"

#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>
#include <KFileDialog>
#include <KColorScheme>

#include <QMenu>
#include <QTextStream>
#include <QFile>
#include <QInputDialog>
#include <QScrollBar>


EffectStackView2::EffectStackView2(Monitor *monitor, QWidget *parent) :
        QWidget(parent),
        m_clipref(NULL),
        m_trackindex(-1),
        m_draggedEffect(NULL),
        m_draggedGroup(NULL),
        m_groupIndex(0)
{
    m_effectMetaInfo.trackMode = false;
    m_effectMetaInfo.monitor = monitor;
    m_effects = QList <CollapsibleEffect*>();

    m_ui.setupUi(this);
    setFont(KGlobalSettings::smallestReadableFont());
    m_ui.checkAll->setToolTip(i18n("Enable/Disable all effects"));
    m_ui.buttonShowComments->setIcon(KIcon("help-about"));
    m_ui.buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));
    
    connect(m_ui.checkAll, SIGNAL(stateChanged(int)), this, SLOT(slotCheckAll(int)));
    connect(m_ui.buttonShowComments, SIGNAL(clicked()), this, SLOT(slotShowComments()));
    m_ui.labelComment->setHidden(true);

    setEnabled(false);

    
    setStyleSheet(CollapsibleEffect::getStyleSheet());
}

EffectStackView2::~EffectStackView2()
{
}

void EffectStackView2::updatePalette()
{
    setStyleSheet(CollapsibleEffect::getStyleSheet());
}

void EffectStackView2::slotRenderPos(int pos)
{
    if (m_effects.isEmpty()) return;
    if (!m_effectMetaInfo.trackMode && m_clipref) pos = pos - m_clipref->startPos().frames(KdenliveSettings::project_fps());

    for (int i = 0; i< m_effects.count(); i++)
        m_effects.at(i)->slotSyncEffectsPos(pos);
}

void EffectStackView2::slotClipItemSelected(ClipItem* c, int ix)
{
    if (c && !c->isEnabled()) return;
    if (c && c == m_clipref) {
        
    } else {
        m_clipref = c;
        if (c) {
            QString cname = m_clipref->clipName();
            if (cname.length() > 30) {
                m_ui.checkAll->setToolTip(i18n("Effects for %1").arg(cname));
                cname.truncate(27);
                m_ui.checkAll->setText(i18n("Effects for %1").arg(cname) + "...");
            } else {
                m_ui.checkAll->setToolTip(QString());
                m_ui.checkAll->setText(i18n("Effects for %1").arg(cname));
            }
            m_ui.checkAll->setEnabled(true);
            ix = c->selectedEffectIndex();
            QString size = c->baseClip()->getProperty("frame_size");
            double factor = c->baseClip()->getProperty("aspect_ratio").toDouble();
            m_effectMetaInfo.frameSize = QPoint((int)(size.section('x', 0, 0).toInt() * factor + 0.5), size.section('x', 1, 1).toInt());
        } else {
            ix = 0;
        }
    }
    if (m_clipref == NULL) {
        //TODO: clear list, reset paramdesc and info
        //ItemInfo info;
        //m_effectedit->transferParamDesc(QDomElement(), info);
	clear();
        return;
    }
    setEnabled(true);
    m_effectMetaInfo.trackMode = false;
    m_currentEffectList = m_clipref->effectList();
    setupListView(ix);
}

void EffectStackView2::slotTrackItemSelected(int ix, const TrackInfo info)
{
    m_clipref = NULL;
    m_effectMetaInfo.trackMode = true;
    m_currentEffectList = info.effectsList;
    m_trackInfo = info;
    setEnabled(true);
    m_ui.checkAll->setToolTip(QString());
    m_ui.checkAll->setText(i18n("Effects for track %1").arg(info.trackName.isEmpty() ? QString::number(ix) : info.trackName));
    m_trackindex = ix;
    setupListView(0);
}


void EffectStackView2::setupListView(int ix)
{
    blockSignals(true);
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
    
    if (m_currentEffectList.isEmpty()) m_ui.labelComment->setHidden(true);

    for (int i = 0; i < m_currentEffectList.count(); i++) {
        QDomElement d = m_currentEffectList.at(i).cloneNode().toElement();
        if (d.isNull()) {
            kDebug() << " . . . . WARNING, NULL EFFECT IN STACK!!!!!!!!!";
            continue;
        }
        
        CollapsibleGroup *group = NULL;
	EffectInfo effectInfo;
	effectInfo.fromString(d.attribute("kdenlive_info"));
	if (effectInfo.groupIndex >= 0) {
	    // effect is in a group
	    
	    for (int j = 0; j < vbox1->count(); j++) {
		CollapsibleGroup *eff = static_cast<CollapsibleGroup *>(vbox1->itemAt(j)->widget());
		if (eff->isGroup() &&  eff->groupIndex() == effectInfo.groupIndex) {
		    group = eff;
		    break;
		}
	    }
	    
	    if (group == NULL) {
		group = new CollapsibleGroup(effectInfo.groupIndex, i == 0, i == m_currentEffectList.count() - 1, effectInfo.groupName, m_ui.container->widget());
		connect(group, SIGNAL(moveEffect(int,int,int,QString)), this, SLOT(slotMoveEffect(int,int,int,QString)));
		connect(group, SIGNAL(unGroup(CollapsibleGroup*)), this , SLOT(slotUnGroup(CollapsibleGroup*)));
		connect(group, SIGNAL(groupRenamed(CollapsibleGroup *)), this, SLOT(slotRenameGroup(CollapsibleGroup*)));
                connect(group, SIGNAL(reloadEffects()), this , SIGNAL(reloadEffects()));
		connect(group, SIGNAL(deleteGroup(int, QDomDocument)), this , SLOT(slotDeleteGroup(int,QDomDocument)));
		vbox1->addWidget(group);
		group->installEventFilter( this );
	    }
	    if (effectInfo.groupIndex >= m_groupIndex) m_groupIndex = effectInfo.groupIndex + 1;
	}

        /*QDomDocument doc;
        doc.appendChild(doc.importNode(d, true));
        kDebug() << "IMPORTED STK: " << doc.toString();*/
	
	ItemInfo info;
	if (m_effectMetaInfo.trackMode) { 
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
	}
	else info = m_clipref->info();

        CollapsibleEffect *currentEffect = new CollapsibleEffect(d, m_currentEffectList.at(i), info, &m_effectMetaInfo, i == m_currentEffectList.count() - 1, view);
        m_effects.append(currentEffect);
        if (group) {
	    group->addGroupEffect(currentEffect);
	} else {
	    vbox1->addWidget(currentEffect);
	}
	if (currentEffect->effectIndex() == ix) currentEffect->setActive(true);

	// Check drag & drop
	currentEffect->installEventFilter( this );

        connect(currentEffect, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)), this , SLOT(slotUpdateEffectParams(const QDomElement, const QDomElement, int)));
	connect(currentEffect, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QString)), this , SLOT(slotStartFilterJob(QString,QString,QString,QString,QString,QString)));
        connect(currentEffect, SIGNAL(deleteEffect(const QDomElement)), this , SLOT(slotDeleteEffect(const QDomElement)));
	connect(currentEffect, SIGNAL(reloadEffects()), this , SIGNAL(reloadEffects()));
	connect(currentEffect, SIGNAL(resetEffect(int)), this , SLOT(slotResetEffect(int)));
        connect(currentEffect, SIGNAL(changeEffectPosition(int,bool)), this , SLOT(slotMoveEffectUp(int , bool)));
        connect(currentEffect, SIGNAL(effectStateChanged(bool, int)), this, SLOT(slotUpdateEffectState(bool, int)));
        connect(currentEffect, SIGNAL(activateEffect(int)), this, SLOT(slotSetCurrentEffect(int)));
        connect(currentEffect, SIGNAL(checkMonitorPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
        connect(currentEffect, SIGNAL(seekTimeline(int)), this , SLOT(slotSeekTimeline(int)));
	connect(currentEffect, SIGNAL(createGroup(int)), this , SLOT(slotCreateGroup(int)));
	connect(currentEffect, SIGNAL(moveEffect(int,int,int,QString)), this , SLOT(slotMoveEffect(int,int,int,QString)));
	connect(currentEffect, SIGNAL(addEffect(QDomElement)), this , SLOT(slotAddEffect(QDomElement)));
	
        //ui.title->setPixmap(icon.pixmap(QSize(12, 12)));
    }
    vbox1->addStretch(10);
    slotUpdateCheckAllButton();
    connect(m_effectMetaInfo.monitor, SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    
    // Wait a little bit for the new layout to be ready, then check if we have a scrollbar
    QTimer::singleShot(200, this, SLOT(slotCheckWheelEventFilter()));
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
    for (int i = 0; i < m_effects.count(); i++) {
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
	    if (me->button() == Qt::LeftButton && (m_draggedGroup->framegroup->underMouse() || m_draggedGroup->title()->underMouse()))
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
	doc.appendChild(doc.importNode(effect, true));
	pixmap = QPixmap::grabWidget(m_draggedEffect->title);
    }
    else if (m_draggedGroup) {
	doc = m_draggedGroup->effectsData();
	pixmap = QPixmap::grabWidget(m_draggedGroup->title());
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


void EffectStackView2::slotUpdateEffectState(bool disable, int index)
{
    if (m_effectMetaInfo.trackMode)
        emit changeEffectState(NULL, m_trackindex, index, disable);
    else
        emit changeEffectState(m_clipref, -1, index, disable);
    slotUpdateCheckAllButton();
}


void EffectStackView2::raiseWindow(QWidget* dock)
{
    if ((m_clipref || m_effectMetaInfo.trackMode) && dock)
        dock->raise();
}


void EffectStackView2::slotSeekTimeline(int pos)
{
    if (m_effectMetaInfo.trackMode) {
        emit seekTimeline(pos);
    } else if (m_clipref) {
        emit seekTimeline(m_clipref->startPos().frames(KdenliveSettings::project_fps()) + pos);
    }
}


/*void EffectStackView2::slotRegionChanged()
{
    if (!m_trackMode) emit updateClipRegion(m_clipref, m_ui.effectlist->currentRow(), m_ui.region_url->text());
}*/

void EffectStackView2::slotCheckMonitorPosition(int renderPos)
{
    if (m_effectMetaInfo.trackMode || (m_clipref && renderPos >= m_clipref->startPos().frames(KdenliveSettings::project_fps()) && renderPos <= m_clipref->endPos().frames(KdenliveSettings::project_fps()))) {
        if (!m_effectMetaInfo.monitor->getEffectEdit()->getScene()->views().at(0)->isVisible())
            m_effectMetaInfo.monitor->slotEffectScene(true);
    } else {
        m_effectMetaInfo.monitor->slotEffectScene(false);
    }
}

int EffectStackView2::isTrackMode(bool *ok) const
{
    *ok = m_effectMetaInfo.trackMode;
    return m_trackindex;
}

void EffectStackView2::clear()
{
    m_effects.clear();
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
    if (state == 1) {
        state = 2;
        m_ui.checkAll->blockSignals(true);
        m_ui.checkAll->setCheckState(Qt::Checked);
        m_ui.checkAll->blockSignals(false);
    }

    bool disabled = (state != 2);
    for (int i = 0; i < m_effects.count(); i++) {
	if (!m_effects.at(i)->isGroup()) {
	    m_effects.at(i)->slotEnable(!disabled);
	}
    }
}

void EffectStackView2::slotUpdateCheckAllButton()
{
    bool hasEnabled = false;
    bool hasDisabled = false;
    
    for (int i = 0; i < m_effects.count(); i++) {
	if (m_effects.at(i)->enabledBox->isChecked()) hasEnabled = true;
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
    for (int i = 0; i < m_effects.count(); i++) {
        if (m_effects.at(i)->isActive()) {
	    slotDeleteEffect(m_effects.at(i)->effect());
	    break;
	}
    }
}

void EffectStackView2::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    m_effectMetaInfo.profile = profile;
    m_effectMetaInfo.timecode = t;
}

void EffectStackView2::updateTimecodeFormat()
{
    for (int i = 0; i< m_effects.count(); i++)
        m_effects.at(i)->updateTimecodeFormat();
}

CollapsibleEffect *EffectStackView2::getEffectByIndex(int ix)
{
    for (int i = 0; i< m_effects.count(); i++) {
        if (m_effects.at(i)->effectIndex() == ix) {
	    return m_effects.at(i);
	}
    }
    return NULL;
}

void EffectStackView2::slotUpdateEffectParams(const QDomElement old, const QDomElement e, int ix)
{
    if (m_effectMetaInfo.trackMode)
        emit updateEffect(NULL, m_trackindex, old, e, ix,false);
    else if (m_clipref) {
        emit updateEffect(m_clipref, -1, old, e, ix, false);
        // Make sure the changed effect is currently displayed
        slotSetCurrentEffect(ix);
    }
    QTimer::singleShot(200, this, SLOT(slotCheckWheelEventFilter()));
}

void EffectStackView2::slotSetCurrentEffect(int ix)
{
    if (m_clipref && ix != m_clipref->selectedEffectIndex())
        m_clipref->setSelectedEffect(ix);
    for (int i = 0; i < m_effects.count(); i++) {
	if (m_effects.at(i)->effectIndex() == ix) {
	    m_effects.at(i)->setActive(true);
	    m_ui.labelComment->setText(i18n(m_effects.at(i)->effect().firstChildElement("description").firstChildElement("full").text().toUtf8().data()));
	     m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || m_ui.labelComment->text().isEmpty());
	}
        else m_effects.at(i)->setActive(false);
    }
}

void EffectStackView2::slotDeleteGroup(int groupIndex, QDomDocument doc)
{
    QDomNodeList effects = doc.elementsByTagName("effect");
    ClipItem * clip = NULL;
    int ix;
    if (m_effectMetaInfo.trackMode) {
	ix = m_trackindex;
    }
    else {
	clip = m_clipref;
	ix = -1;
    }
    
    for (int i = 0; i < effects.count(); i++)
	emit removeEffect(clip, ix, effects.at(i).toElement());
}

void EffectStackView2::slotDeleteEffect(const QDomElement effect)
{
    if (m_effectMetaInfo.trackMode)
        emit removeEffect(NULL, m_trackindex, effect);
    else
        emit removeEffect(m_clipref, -1, effect);
}

void EffectStackView2::slotAddEffect(QDomElement effect)
{
    emit addEffect(m_clipref, effect);
}

void EffectStackView2::slotMoveEffectUp(int index, bool up)
{
    if (up && index <= 1) return;
    if (!up && index >= m_currentEffectList.count()) return;
    int endPos;
    if (up) {
        endPos = index - 1;
    }
    else {
        endPos =  index + 1;
    }
    if (m_effectMetaInfo.trackMode) emit changeEffectPosition(NULL, m_trackindex, index, endPos);
    else emit changeEffectPosition(m_clipref, -1, index, endPos);
}

void EffectStackView2::slotStartFilterJob(const QString&filterName, const QString&filterParams, const QString&finalFilterName, const QString&consumer, const QString&consumerParams, const QString&properties)
{
    if (!m_clipref) return;
    emit startFilterJob(m_clipref->info(), m_clipref->clipProducer(), filterName, filterParams, finalFilterName, consumer, consumerParams, properties);
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
    foreach(const QString &type, effectLists.keys()) {
        EffectsList *list = effectLists[type];
        dom = list->getEffectByTag(QString(), effectId).cloneNode().toElement();
        if (!dom.isNull()) break;
    }
    if (!dom.isNull()) {
        dom.setAttribute("kdenlive_ix", old.attribute("kdenlive_ix"));
        if (m_effectMetaInfo.trackMode) {
            EffectsList::setParameter(dom, "in", QString::number(0));
            EffectsList::setParameter(dom, "out", QString::number(m_trackInfo.duration));
            ItemInfo info;
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
	    m_effects.at(ix)->updateWidget(info, dom, &m_effectMetaInfo);
            emit updateEffect(NULL, m_trackindex, old, dom, ix,false);
        } else {
            m_clipref->initEffect(dom);
            for (int i = 0; i < m_effects.count(); i++) {
                if (m_effects.at(i)->effectIndex() == ix) {
                    m_effects.at(i)->updateWidget(m_clipref->info(), dom, &m_effectMetaInfo);
                    break;
                }
            }
            //m_ui.region_url->setUrl(KUrl(dom.attribute("region")));
            emit updateEffect(m_clipref, -1, old, dom, ix,false);
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

void EffectStackView2::slotCreateGroup(int ix)
{
    QDomElement oldeffect = m_currentEffectList.itemFromIndex(ix);
    QDomElement neweffect = oldeffect.cloneNode().toElement();
    EffectInfo effectinfo;
    effectinfo.fromString(oldeffect.attribute("kdenlive_info"));
    effectinfo.groupIndex = m_groupIndex;
    neweffect.setAttribute("kdenlive_info", effectinfo.toString());

    ItemInfo info;
    if (m_effectMetaInfo.trackMode) { 
	info.track = m_trackInfo.type;
        info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
        info.cropStart = GenTime(0);
        info.startPos = GenTime(-1);
        info.track = 0;
	emit updateEffect(NULL, m_trackindex, oldeffect, neweffect, ix, false);
    } else {
	emit updateEffect(m_clipref, -1, oldeffect, neweffect, ix, false);
    }
    
    QVBoxLayout *l = static_cast<QVBoxLayout *>(m_ui.container->widget()->layout());
    int groupPos = 0;
    CollapsibleEffect *effectToMove = NULL;
    for (int i = 0; i < m_effects.count(); i++) {
        if (m_effects.at(i)->effectIndex() == ix) {
	    effectToMove = m_effects.at(i);
	    groupPos = l->indexOf(effectToMove);
	    l->removeWidget(effectToMove);
	    break;
	}
    }
    
    CollapsibleGroup *group = new CollapsibleGroup(m_groupIndex, ix == 1, ix == m_currentEffectList.count() - 2, QString(), m_ui.container->widget());
    m_groupIndex++;
    connect(group, SIGNAL(moveEffect(int,int,int,QString)), this , SLOT(slotMoveEffect(int,int,int,QString)));
    connect(group, SIGNAL(unGroup(CollapsibleGroup*)), this , SLOT(slotUnGroup(CollapsibleGroup*)));
    connect(group, SIGNAL(groupRenamed(CollapsibleGroup *)), this , SLOT(slotRenameGroup(CollapsibleGroup*)));
    connect(group, SIGNAL(reloadEffects()), this , SIGNAL(reloadEffects()));
    connect(group, SIGNAL(deleteGroup(int, QDomDocument)), this , SLOT(slotDeleteGroup(int,QDomDocument)));
    l->insertWidget(groupPos, group);
    group->installEventFilter( this );
    group->addGroupEffect(effectToMove);
}

void EffectStackView2::slotMoveEffect(int currentIndex, int newIndex, int groupIndex, QString groupName)
{
    CollapsibleEffect *effectToMove = getEffectByIndex(currentIndex);
    if (effectToMove == NULL) return;

    QDomElement oldeffect = effectToMove->effect();
    QDomElement neweffect = oldeffect.cloneNode().toElement();
    
    EffectInfo effectinfo;
    effectinfo.fromString(oldeffect.attribute("kdenlive_info"));
    effectinfo.groupIndex = groupIndex;
    effectinfo.groupName = groupName;
    neweffect.setAttribute("kdenlive_info", effectinfo.toString());

    ItemInfo info;
    if (m_effectMetaInfo.trackMode) { 
	info.track = m_trackInfo.type;
        info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
        info.cropStart = GenTime(0);
        info.startPos = GenTime(-1);
        info.track = 0;
	emit updateEffect(NULL, m_trackindex, oldeffect, neweffect, effectToMove->effectIndex(),false);
    } else {
	emit updateEffect(m_clipref, -1, oldeffect, neweffect, effectToMove->effectIndex(),false);
    }
    //if (currentIndex == newIndex) return;
    // Update effect index with new position
    if (m_effectMetaInfo.trackMode) {
	emit changeEffectPosition(NULL, m_trackindex, currentIndex, newIndex);
    }
    else {
	emit changeEffectPosition(m_clipref, -1, currentIndex, newIndex);
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
    for (int i = 0; i < effects.count(); i++) {
	QDomElement origin = effects.at(i)->effect();
	QDomElement changed = origin.cloneNode().toElement();
	changed.setAttribute("kdenlive_info", effects.at(i)->infoString());
	if (m_effectMetaInfo.trackMode) { 
	    emit updateEffect(NULL, m_trackindex, origin, changed, effects.at(i)->effectIndex(),false);
	} else {
	    emit updateEffect(m_clipref, -1, origin, changed, effects.at(i)->effectIndex(),false);
	}
    }
}

#include "effectstackview2.moc"
