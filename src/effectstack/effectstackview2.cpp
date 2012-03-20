/***************************************************************************
                          effecstackview.cpp2  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
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


EffectStackView2::EffectStackView2(Monitor *monitor, QWidget *parent) :
        QWidget(parent),
        m_clipref(NULL),
        m_trackindex(-1)
{
    m_effectMetaInfo.trackMode = false;
    m_effectMetaInfo.monitor = monitor;

    m_ui.setupUi(this);
    setFont(KGlobalSettings::smallestReadableFont());
    m_ui.checkAll->setToolTip(i18n("Enable/Disable all effects"));
    m_ui.buttonShowComments->setIcon(KIcon("help-about"));
    m_ui.buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));


    setEnabled(false);

    QPalette p = palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor dark_bg = scheme.shade(KColorScheme::DarkShade);
    QColor selected_bg = scheme.decoration(KColorScheme::FocusColor).color();
    QColor hover_bg = scheme.decoration(KColorScheme::HoverColor).color();
    QColor light_bg = scheme.shade(KColorScheme::LightShade);
    
    QString stylesheet(QString("QProgressBar:horizontal {border: 1px solid %1;border-radius:0px;border-top-left-radius: 4px;border-bottom-left-radius: 4px;border-right: 0px;background:%4;padding: 0px;text-align:left center}\
                                QProgressBar:horizontal#dragOnly {background: %1} QProgressBar:horizontal:hover#dragOnly {background: %3} QProgressBar:horizontal:hover {border: 1px solid %3;border-right: 0px;}\
                                QProgressBar::chunk:horizontal {background: %1;} QProgressBar::chunk:horizontal:hover {background: %3;}\
                                QProgressBar:horizontal[inTimeline=\"true\"] { border: 1px solid %2;border-right: 0px;background: %4;padding: 0px;text-align:left center } QProgressBar::chunk:horizontal[inTimeline=\"true\"] {background: %2;}\
                                QAbstractSpinBox#dragBox {border: 1px solid %1;border-top-right-radius: 4px;border-bottom-right-radius: 4px;padding-right:0px;} QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;}\
                                QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { border: 1px solid %2;} QAbstractSpinBox:hover#dragBox {border: 1px solid %3;} ")
                                .arg(dark_bg.name()).arg(selected_bg.name()).arg(hover_bg.name()).arg(light_bg.name()));
    setStyleSheet(stylesheet);
}

EffectStackView2::~EffectStackView2()
{
}


void EffectStackView2::slotRenderPos(int pos)
{
    if (m_effects.isEmpty()) return;
    if (!m_effectMetaInfo.trackMode && m_clipref) pos = pos - m_clipref->startPos().frames(KdenliveSettings::project_fps());

    for (int i = 0; i< m_effects.count(); i++)
        m_effects.at(i)->slotSyncEffectsPos(pos);
}

void EffectStackView2::setMenu(QMenu *menu)
{
    //m_ui.buttonNew->setMenu(menu);
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
        m_effects.clear();
	QWidget *view = m_ui.container->takeWidget();
	if (view) {
	    if (view->layout()) clearLayout(view->layout());
	    delete view;
	}
        m_ui.checkAll->setToolTip(QString());
        m_ui.checkAll->setText(QString());
        setEnabled(false);
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

void EffectStackView2::clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while((item = layout->takeAt(0))) {
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void EffectStackView2::setupListView(int ix)
{
    //TODO: clear list

    kDebug()<<"++++++++++++++++++++++++++++++++   setup: "<<children().count();

    blockSignals(true);
    disconnect(m_effectMetaInfo.monitor, SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    m_effects.clear();
    QWidget *view = m_ui.container->takeWidget();
    if (view) {
	if (view->layout()) clearLayout(view->layout());
	delete view;
    }
    blockSignals(false);
    view = new QWidget(m_ui.container);
    m_ui.container->setWidget(view);

    QVBoxLayout *vbox1 = new QVBoxLayout(view);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);

    for (int i = 0; i < m_currentEffectList.count(); i++) {
        QDomElement d = m_currentEffectList.at(i).cloneNode().toElement();
        if (d.isNull()) {
            kDebug() << " . . . . WARNING, NULL EFFECT IN STACK!!!!!!!!!";
            continue;
        }

        /*QDomDocument doc;
        doc.appendChild(doc.importNode(d, true));
        kDebug() << "IMPORTED STK: " << doc.toString();*/

        CollapsibleEffect *currentEffect = new CollapsibleEffect(d, m_currentEffectList.at(i), m_clipref->info(), i, &m_effectMetaInfo, i == m_currentEffectList.count() - 1, view);
        m_effects.append(currentEffect);
        vbox1->addWidget(currentEffect);
        connect(currentEffect, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)), this , SLOT(slotUpdateEffectParams(const QDomElement, const QDomElement, int)));
	connect(currentEffect, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QString)), this , SLOT(slotStartFilterJob(QString,QString,QString,QString,QString,QString)));
        connect(currentEffect, SIGNAL(deleteEffect(const QDomElement, int)), this , SLOT(slotDeleteEffect(const QDomElement, int)));
	connect(currentEffect, SIGNAL(reloadEffects()), this , SIGNAL(reloadEffects()));
	connect(currentEffect, SIGNAL(resetEffect(int)), this , SLOT(slotResetEffect(int)));
        connect(currentEffect, SIGNAL(changeEffectPosition(int,bool)), this , SLOT(slotMoveEffect(int , bool)));
        connect(currentEffect, SIGNAL(effectStateChanged(bool, int)), this, SLOT(slotUpdateEffectState(bool, int)));
        connect(currentEffect, SIGNAL(activateEffect(int)), this, SLOT(slotSetCurrentEffect(int)));
        connect(currentEffect, SIGNAL(checkMonitorPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
        connect(currentEffect, SIGNAL(seekTimeline(int)), this , SLOT(slotSeekTimeline(int)));
            //ui.title->setPixmap(icon.pixmap(QSize(12, 12)));
    }
    vbox1->addStretch(10);
    connect(m_effectMetaInfo.monitor, SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
}


void EffectStackView2::slotUpdateEffectState(bool disable, int index)
{
    if (m_effectMetaInfo.trackMode)
        emit changeEffectState(NULL, m_trackindex, index, disable);
    else
        emit changeEffectState(m_clipref, -1, index, disable);
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
}

void EffectStackView2::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    m_effectMetaInfo.profile = profile;
    m_effectMetaInfo.timecode = t;
}

void EffectStackView2::slotItemDel()
{

}

void EffectStackView2::updateTimecodeFormat()
{
    for (int i = 0; i< m_effects.count(); i++)
        m_effects.at(i)->updateTimecodeFormat();
}

void EffectStackView2::slotUpdateEffectParams(const QDomElement old, const QDomElement e, int ix)
{
    if (m_effectMetaInfo.trackMode)
        emit updateEffect(NULL, m_trackindex, old, e, ix);
    else if (m_clipref) {
        emit updateEffect(m_clipref, -1, old, e, ix);
        // Make sure the changed effect is currently displayed
        slotSetCurrentEffect(ix);
    }
}

void EffectStackView2::slotSetCurrentEffect(int ix)
{
    if (ix != m_clipref->selectedEffectIndex())
        m_clipref->setSelectedEffect(ix);
    for (int i = 0; i < m_effects.count(); i++) {
        m_effects.at(i)->setActive(i == ix);
    }
}

void EffectStackView2::slotDeleteEffect(const QDomElement effect, int index)
{
    
    if (m_effectMetaInfo.trackMode)
        emit removeEffect(NULL, m_trackindex, effect);
    else
        emit removeEffect(m_clipref, -1, effect);
}

void EffectStackView2::slotMoveEffect(int index, bool up)
{
    if (up && index <= 0) return;
    if (!up && index >= m_currentEffectList.count() - 1) return;
    int startPos;
    int endPos;
    if (up) {
        startPos =  index + 1;
        endPos = index;
    }
    else {
        startPos =  index + 1;
        endPos =  index + 2;
    }
    if (m_effectMetaInfo.trackMode) emit changeEffectPosition(NULL, m_trackindex, startPos, endPos);
    else emit changeEffectPosition(m_clipref, -1, startPos, endPos);
}

void EffectStackView2::slotStartFilterJob(const QString&filterName, const QString&filterParams, const QString&finalFilterName, const QString&consumer, const QString&consumerParams, const QString&properties)
{
    if (!m_clipref) return;
    emit startFilterJob(m_clipref->info(), m_clipref->clipProducer(), filterName, filterParams, finalFilterName, consumer, consumerParams, properties);
}

void EffectStackView2::slotResetEffect(int ix)
{
    QDomElement old = m_currentEffectList.at(ix);
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
	//TODO: Track mode
        /*if (m_trackMode) {
            EffectsList::setParameter(dom, "in", QString::number(0));
            EffectsList::setParameter(dom, "out", QString::number(m_trackInfo.duration));
            ItemInfo info;
            info.track = m_trackInfo.type;
            info.cropDuration = GenTime(m_trackInfo.duration, KdenliveSettings::project_fps());
            info.cropStart = GenTime(0);
            info.startPos = GenTime(-1);
            info.track = 0;
            m_effectedit->transferParamDesc(dom, info);
            emit updateEffect(NULL, m_trackindex, old, dom, activeRow);
        } else*/ {
            m_clipref->initEffect(dom);
	    m_effects.at(ix)->updateWidget(m_clipref->info(), ix, dom, &m_effectMetaInfo);
            //m_effectedit->transferParamDesc(dom, m_clipref->info());
            //m_ui.region_url->setUrl(KUrl(dom.attribute("region")));
            emit updateEffect(m_clipref, -1, old, dom, ix);
        }
    }

    /*emit showComments(m_ui.buttonShowComments->isChecked());
    m_ui.labelComment->setHidden(!m_ui.buttonShowComments->isChecked() || !m_ui.labelComment->text().count());*/
}

#include "effectstackview2.moc"
