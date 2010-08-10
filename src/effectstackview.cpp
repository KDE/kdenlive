/***************************************************************************
                          effecstackview.cpp  -  description
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


#include "effectstackview.h"
#include "effectslist.h"
#include "clipitem.h"
#include "mainwindow.h"
#include "docclipbase.h"
#include "projectlist.h"
#include "kthumb.h"
#include "monitorscene.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>
#include <KFileDialog>

#include <QMenu>
#include <QTextStream>
#include <QFile>
#include <QInputDialog>


EffectStackView::EffectStackView(Monitor *monitor, QWidget *parent) :
        QWidget(parent),
        m_monitor(monitor)
{
    m_ui.setupUi(this);
    QVBoxLayout *vbox1 = new QVBoxLayout(m_ui.frame);
    m_effectedit = new EffectStackEdit(monitor, m_ui.frame);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);
    vbox1->addWidget(m_effectedit);
    m_ui.frame->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    m_ui.region_url->fileDialog()->setFilter(ProjectList::getExtensions());
    //m_ui.effectlist->horizontalHeader()->setVisible(false);
    //m_ui.effectlist->verticalHeader()->setVisible(false);
    m_clipref = NULL;

    m_ui.buttonNew->setIcon(KIcon("document-new"));
    m_ui.buttonNew->setToolTip(i18n("Add new effect"));
    m_ui.buttonUp->setIcon(KIcon("go-up"));
    m_ui.buttonUp->setToolTip(i18n("Move effect up"));
    m_ui.buttonDown->setIcon(KIcon("go-down"));
    m_ui.buttonDown->setToolTip(i18n("Move effect down"));
    m_ui.buttonDel->setIcon(KIcon("edit-delete"));
    m_ui.buttonDel->setToolTip(i18n("Delete effect"));
    m_ui.buttonSave->setIcon(KIcon("document-save"));
    m_ui.buttonSave->setToolTip(i18n("Save effect"));
    m_ui.buttonReset->setIcon(KIcon("view-refresh"));
    m_ui.buttonReset->setToolTip(i18n("Reset effect"));
    m_ui.checkAll->setToolTip(i18n("Enable/Disable all effects"));


    m_ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop); //use internal if drop is recognised right

    connect(m_ui.region_url, SIGNAL(urlSelected(const KUrl &)), this , SLOT(slotRegionChanged()));
    connect(m_ui.region_url, SIGNAL(returnPressed()), this , SLOT(slotRegionChanged()));
    connect(m_ui.effectlist, SIGNAL(itemSelectionChanged()), this , SLOT(slotItemSelectionChanged()));
    connect(m_ui.effectlist, SIGNAL(itemChanged(QListWidgetItem *)), this , SLOT(slotItemChanged(QListWidgetItem *)));
    connect(m_ui.buttonUp, SIGNAL(clicked()), this, SLOT(slotItemUp()));
    connect(m_ui.buttonDown, SIGNAL(clicked()), this, SLOT(slotItemDown()));
    connect(m_ui.buttonDel, SIGNAL(clicked()), this, SLOT(slotItemDel()));
    connect(m_ui.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveEffect()));
    connect(m_ui.buttonReset, SIGNAL(clicked()), this, SLOT(slotResetEffect()));
    connect(m_ui.checkAll, SIGNAL(stateChanged(int)), this, SLOT(slotCheckAll(int)));
    connect(m_effectedit, SIGNAL(parameterChanged(const QDomElement, const QDomElement)), this , SLOT(slotUpdateEffectParams(const QDomElement, const QDomElement)));
    connect(m_effectedit, SIGNAL(seekTimeline(int)), this , SLOT(slotSeekTimeline(int)));
    connect(m_effectedit, SIGNAL(displayMessage(const QString&, int)), this, SIGNAL(displayMessage(const QString&, int)));
    connect(m_effectedit, SIGNAL(checkMonitorPosition(int)), this, SLOT(slotCheckMonitorPosition(int)));
    m_effectLists["audio"] = &MainWindow::audioEffects;
    m_effectLists["video"] = &MainWindow::videoEffects;
    m_effectLists["custom"] = &MainWindow::customEffects;
    m_ui.splitter->setStretchFactor(1, 10);
    m_ui.splitter->setStretchFactor(0, 1);
    setEnabled(false);
}

EffectStackView::~EffectStackView()
{
    m_effectLists.clear();
    delete m_effectedit;
}

void EffectStackView::updateTimecodeFormat()
{
    m_effectedit->updateTimecodeFormat();
}

void EffectStackView::setMenu(QMenu *menu)
{
    m_ui.buttonNew->setMenu(menu);
}

void EffectStackView::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    m_effectedit->updateProjectFormat(profile, t);
}

void EffectStackView::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.isEmpty()) return;
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
    path = path + name + ".xml";
    if (QFile::exists(path)) if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it?")) == KMessageBox::No) return;

    int i = m_ui.effectlist->currentRow();
    QDomDocument doc;
    QDomElement effect = m_clipref->effectAt(i).cloneNode().toElement();
    doc.appendChild(doc.importNode(effect, true));
    effect = doc.firstChild().toElement();
    effect.removeAttribute("kdenlive_ix");
    effect.setAttribute("id", name);
    effect.setAttribute("type", "custom");
    QDomElement effectname = effect.firstChildElement("name");
    effect.removeChild(effectname);
    effectname = doc.createElement("name");
    QDomText nametext = doc.createTextNode(name);
    effectname.appendChild(nametext);
    effect.insertBefore(effectname, QDomNode());
    QDomElement effectprops = effect.firstChildElement("properties");
    effectprops.setAttribute("id", name);
    effectprops.setAttribute("type", "custom");

    QFile file(path);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << doc.toString();
    }
    file.close();
    emit reloadEffects();
}

void EffectStackView::slotUpdateEffectParams(const QDomElement old, const QDomElement e)
{
    if (m_clipref)
        emit updateClipEffect(m_clipref, old, e, m_ui.effectlist->currentRow());
}

void EffectStackView::slotClipItemSelected(ClipItem* c, int ix)
{
    if (c && !c->isEnabled()) return;
    if (c && c == m_clipref) {
        if (ix == -1) ix = m_ui.effectlist->currentRow();
        //if (ix == -1 || ix == m_ui.effectlist->currentRow()) return;
    } else {
        m_clipref = c;
        if (c) {
            ix = c->selectedEffectIndex();
            QString size = c->baseClip()->getProperty("frame_size");
            double factor = c->baseClip()->getProperty("aspect_ratio").toDouble();
            QPoint p((int)(size.section('x', 0, 0).toInt() * factor + 0.5), size.section('x', 1, 1).toInt());
            m_effectedit->setFrameSize(p);
            m_effectedit->setFrameSize(p);
        } else ix = 0;
    }
    if (m_clipref == NULL) {
        m_ui.effectlist->blockSignals(true);
        m_ui.effectlist->clear();
        m_effectedit->transferParamDesc(QDomElement(), 0, 0, 0);
        m_ui.region_url->clear();
        m_ui.effectlist->blockSignals(false);
        setEnabled(false);
        return;
    }
    setEnabled(true);
    setupListView(ix);
}

void EffectStackView::slotItemChanged(QListWidgetItem *item)
{
    bool disable = true;
    if (item->checkState() == Qt::Checked) disable = false;
    m_ui.frame_layout->setEnabled(!disable);
    m_ui.buttonReset->setEnabled(!disable);
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow >= 0) {
        m_effectedit->updateParameter("disable", QString::number((int) disable));
        emit changeEffectState(m_clipref, activeRow, disable);
    }
    slotUpdateCheckAllButton();
}


void EffectStackView::setupListView(int ix)
{
    m_ui.effectlist->blockSignals(true);
    m_ui.effectlist->clear();

    // Issue 238: Add icons for effect type in effectstack.
    KIcon videoIcon("kdenlive-show-video");
    KIcon audioIcon("kdenlive-show-audio");
    KIcon customIcon("kdenlive-custom-effect");
    QListWidgetItem* item;

    for (int i = 0; i < m_clipref->effectsCount(); i++) {
        const QDomElement d = m_clipref->effectAt(i);
        if (d.isNull()) {
            kDebug() << " . . . . WARNING, NULL EFFECT IN STACK!!!!!!!!!";
            continue;
        }

        /*QDomDocument doc;
        doc.appendChild(doc.importNode(d, true));
        kDebug() << "IMPORTED STK: " << doc.toString();*/

        QDomNode namenode = d.elementsByTagName("name").item(0);
        if (!namenode.isNull()) {
            // Issue 238: Add icons for effect type in effectstack.
            // Logic more or less copied from initeffects.cpp
            QString type = d.attribute("type", QString());
            if ("audio" == type) {
                item = new QListWidgetItem(audioIcon, i18n(namenode.toElement().text().toUtf8().data()), m_ui.effectlist);
            } else if ("custom" == type) {
                item = new QListWidgetItem(customIcon, i18n(namenode.toElement().text().toUtf8().data()), m_ui.effectlist);
            } else {
                item = new QListWidgetItem(videoIcon, i18n(namenode.toElement().text().toUtf8().data()), m_ui.effectlist);
            }
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            if (d.attribute("disable") == "1") item->setCheckState(Qt::Unchecked);
            else item->setCheckState(Qt::Checked);
        }
    }
    if (m_ui.effectlist->count() == 0) {
        m_ui.buttonDel->setEnabled(false);
        m_ui.buttonSave->setEnabled(false);
        m_ui.buttonReset->setEnabled(false);
        m_ui.buttonUp->setEnabled(false);
        m_ui.buttonDown->setEnabled(false);
        m_ui.checkAll->setEnabled(false);
    } else {
        if (ix < 0) ix = 0;
        if (ix > m_ui.effectlist->count() - 1) ix = m_ui.effectlist->count() - 1;
        m_ui.effectlist->setCurrentRow(ix);
        m_ui.checkAll->setEnabled(true);
    }
    m_ui.effectlist->blockSignals(false);
    if (m_ui.effectlist->count() == 0) {
        m_effectedit->transferParamDesc(QDomElement(), 0, 0, 0);
        m_ui.region_url->clear();
    } else slotItemSelectionChanged(false);
    slotUpdateCheckAllButton();
}

void EffectStackView::slotItemSelectionChanged(bool update)
{
    bool hasItem = m_ui.effectlist->currentItem();
    int activeRow = m_ui.effectlist->currentRow();
    bool isChecked = false;
    if (hasItem && m_ui.effectlist->currentItem()->checkState() == Qt::Checked) isChecked = true;
    if (hasItem && m_ui.effectlist->currentItem()->isSelected()) {
        QDomElement eff = m_clipref->effectAt(activeRow);
        m_effectedit->transferParamDesc(eff,
                                        0,
                                        m_clipref->cropStart().frames(KdenliveSettings::project_fps()),
                                        (m_clipref->cropStart() + m_clipref->cropDuration()).frames(KdenliveSettings::project_fps())); //minx max frame
        m_ui.region_url->setUrl(KUrl(eff.attribute("region")));
    }
    if (m_clipref && update) m_clipref->setSelectedEffect(activeRow);
    m_ui.buttonDel->setEnabled(hasItem);
    m_ui.buttonSave->setEnabled(hasItem);
    m_ui.buttonReset->setEnabled(hasItem && isChecked);
    m_ui.buttonUp->setEnabled(activeRow > 0);
    m_ui.buttonDown->setEnabled((activeRow < m_ui.effectlist->count() - 1) && hasItem);
    m_ui.frame_layout->setEnabled(isChecked);
}

void EffectStackView::slotItemUp()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow <= 0) return;
    emit changeEffectPosition(m_clipref, activeRow + 1, activeRow);
}

void EffectStackView::slotItemDown()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow >= m_ui.effectlist->count() - 1) return;
    emit changeEffectPosition(m_clipref, activeRow + 1, activeRow + 2);
}

void EffectStackView::slotItemDel()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow >= 0) {
        emit removeEffect(m_clipref, m_clipref->effectAt(activeRow));
        slotUpdateCheckAllButton();
    }
}

void EffectStackView::slotResetEffect()
{
    int activeRow = m_ui.effectlist->currentRow();
    if (activeRow < 0) return;
    QDomElement old = m_clipref->effectAt(activeRow).cloneNode().toElement();
    QDomElement dom;
    QString effectName = m_ui.effectlist->currentItem()->text();
    foreach(const QString &type, m_effectLists.keys()) {
        EffectsList *list = m_effectLists[type];
        if (list->effectNames().contains(effectName)) {
            dom = list->getEffectByName(effectName).cloneNode().toElement();
            break;
        }
    }
    if (!dom.isNull()) {
        dom.setAttribute("kdenlive_ix", old.attribute("kdenlive_ix"));
        m_clipref->initEffect(dom);
        m_effectedit->transferParamDesc(dom, 0, m_clipref->cropStart().frames(KdenliveSettings::project_fps()), (m_clipref->cropStart() + m_clipref->cropDuration()).frames(KdenliveSettings::project_fps()));//minx max frame
        m_ui.region_url->setUrl(KUrl(dom.attribute("region")));
        emit updateClipEffect(m_clipref, old, dom, activeRow);
    }
}


void EffectStackView::raiseWindow(QWidget* dock)
{
    if (m_clipref && dock)
        dock->raise();
}

void EffectStackView::clear()
{
    m_ui.effectlist->blockSignals(true);
    m_ui.effectlist->clear();
    m_ui.buttonDel->setEnabled(false);
    m_ui.buttonSave->setEnabled(false);
    m_ui.buttonReset->setEnabled(false);
    m_ui.buttonUp->setEnabled(false);
    m_ui.buttonDown->setEnabled(false);
    m_ui.checkAll->setEnabled(false);
    m_effectedit->transferParamDesc(QDomElement(), 0, 0, 0);
    m_ui.region_url->clear();
    m_ui.effectlist->blockSignals(false);
}


void EffectStackView::slotSeekTimeline(int pos)
{
    if (m_clipref)
        emit seekTimeline(m_clipref->startPos().frames(KdenliveSettings::project_fps()) + pos);
}

void EffectStackView::slotUpdateCheckAllButton()
{
    bool hasEnabled = false;
    bool hasDisabled = false;
    for (int i = 0; i < m_ui.effectlist->count(); ++i) {
        if (m_ui.effectlist->item(i)->checkState() == Qt::Checked)
            hasEnabled = true;
        else
            hasDisabled = true;
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

void EffectStackView::slotCheckAll(int state)
{
    if (state == 1) {
        state = 2;
        m_ui.checkAll->blockSignals(true);
        m_ui.checkAll->setCheckState(Qt::Checked);
        m_ui.checkAll->blockSignals(false);
    }

    for (int i = 0; i < m_ui.effectlist->count(); ++i)
        m_ui.effectlist->item(i)->setCheckState((Qt::CheckState)state);
}

void EffectStackView::slotRegionChanged()
{
    emit updateClipRegion(m_clipref, m_ui.effectlist->currentRow(), m_ui.region_url->text());
}

void EffectStackView::slotCheckMonitorPosition(int renderPos)
{
    if (renderPos >= m_clipref->startPos().frames(KdenliveSettings::project_fps()) && renderPos <= m_clipref->endPos().frames(KdenliveSettings::project_fps())) {
        if (!m_monitor->getEffectScene()->views().at(0)->isVisible())
            m_monitor->slotEffectScene(true);
    } else {
        m_monitor->slotEffectScene(false);
    }
}

#include "effectstackview.moc"
