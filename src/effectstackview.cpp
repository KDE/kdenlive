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
#include "kdenlivesettings.h"

#include <KDebug>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>

#include <QMenu>
#include <QTextStream>
#include <QFile>
#include <QInputDialog>


EffectStackView::EffectStackView(QWidget *parent) :
        QWidget(parent)
{
    ui.setupUi(this);
    effectedit = new EffectStackEdit(ui.frame);
    //ui.effectlist->horizontalHeader()->setVisible(false);
    //ui.effectlist->verticalHeader()->setVisible(false);
    clipref = NULL;

    ui.buttonNew->setIcon(KIcon("document-new"));
    ui.buttonNew->setToolTip(i18n("Add new effect"));
    ui.buttonUp->setIcon(KIcon("go-up"));
    ui.buttonUp->setToolTip(i18n("Move effect up"));
    ui.buttonDown->setIcon(KIcon("go-down"));
    ui.buttonDown->setToolTip(i18n("Move effect down"));
    ui.buttonDel->setIcon(KIcon("trash-empty"));
    ui.buttonDel->setToolTip(i18n("Delete effect"));
    ui.buttonSave->setIcon(KIcon("document-save"));
    ui.buttonSave->setToolTip(i18n("Save effect"));
    ui.buttonReset->setIcon(KIcon("view-refresh"));
    ui.buttonReset->setToolTip(i18n("Reset effect"));


    ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop);//use internal if drop is recognised right

    connect(ui.effectlist, SIGNAL(itemSelectionChanged()), this , SLOT(slotItemSelectionChanged()));
    connect(ui.effectlist, SIGNAL(itemChanged(QListWidgetItem *)), this , SLOT(slotItemChanged(QListWidgetItem *)));
    connect(ui.buttonUp, SIGNAL(clicked()), this, SLOT(slotItemUp()));
    connect(ui.buttonDown, SIGNAL(clicked()), this, SLOT(slotItemDown()));
    connect(ui.buttonDel, SIGNAL(clicked()), this, SLOT(slotItemDel()));
    connect(ui.buttonSave, SIGNAL(clicked()), this, SLOT(slotSaveEffect()));
    connect(ui.buttonReset, SIGNAL(clicked()), this, SLOT(slotResetEffect()));
    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), effectedit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(effectedit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
    effectLists["audio"] = &MainWindow::audioEffects;
    effectLists["video"] = &MainWindow::videoEffects;
    effectLists["custom"] = &MainWindow::customEffects;
    ui.splitter->setStretchFactor(1, 10);
    ui.splitter->setStretchFactor(0, 1);
    setEnabled(false);
}

void EffectStackView::setMenu(QMenu *menu)
{
    ui.buttonNew->setMenu(menu);
}

void EffectStackView::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    effectedit->updateProjectFormat(profile, t);
}

void EffectStackView::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.isEmpty()) return;
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
    path = path + name + ".xml";
    if (QFile::exists(path)) if (KMessageBox::questionYesNo(this, i18n("File already exists.\nDo you want to overwrite it?")) == KMessageBox::No) return;

    int i = ui.effectlist->currentRow();
    QDomDocument doc;
    QDomElement effect = clipref->effectAt(i).cloneNode().toElement();
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

void EffectStackView::slotUpdateEffectParams(const QDomElement& old, const QDomElement& e)
{
    if (clipref)
        emit updateClipEffect(clipref, old, e, ui.effectlist->currentRow());
}

void EffectStackView::slotClipItemSelected(ClipItem* c, int ix)
{
    if (c && c == clipref) {
        if (ix == -1) ix = ui.effectlist->currentRow();
    } else {
        clipref = c;
        if (c) ix = c->selectedEffectIndex();
        else ix = 0;
    }
    if (clipref == NULL) {
        ui.effectlist->clear();
        effectedit->transferParamDesc(QDomElement(), 0, 0);
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
    ui.buttonReset->setEnabled(!disable);
    int activeRow = ui.effectlist->currentRow();
    if (activeRow >= 0) {
        emit changeEffectState(clipref, activeRow, disable);
    }
}


void EffectStackView::setupListView(int ix)
{
    ui.effectlist->clear();

    // Issue 238: Add icons for effect type in effectstack.
    KIcon videoIcon("kdenlive-show-video");
    KIcon audioIcon("kdenlive-show-audio");
    QListWidgetItem* item;

    for (int i = 0;i < clipref->effectsCount();i++) {
        QDomElement d = clipref->effectAt(i);

        QDomNode namenode = d.elementsByTagName("name").item(0);
        if (!namenode.isNull()) {
            // Issue 238: Add icons for effect type in effectstack.
            // Logic more or less copied from initeffects.cpp
            QString type = d.attribute("type", QString());
            if ("audio" == type) {
                item = new QListWidgetItem(audioIcon, i18n(namenode.toElement().text().toUtf8().data()), ui.effectlist);
            } else if ("custom" == type) {
                item = new QListWidgetItem(i18n(namenode.toElement().text().toUtf8().data()), ui.effectlist);
            } else {
                item = new QListWidgetItem(videoIcon, i18n(namenode.toElement().text().toUtf8().data()), ui.effectlist);
            }
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            if (d.attribute("disabled") == "1") item->setCheckState(Qt::Unchecked);
            else item->setCheckState(Qt::Checked);
        }
    }
    if (clipref->effectsCount() == 0) {
        emit transferParamDesc(QDomElement(), 0, 0);
        ui.buttonDel->setEnabled(false);
        ui.buttonSave->setEnabled(false);
        ui.buttonReset->setEnabled(false);
        ui.buttonUp->setEnabled(false);
        ui.buttonDown->setEnabled(false);
    } else {
        if (ix < 0) ix = 0;
        if (ix > ui.effectlist->count() - 1) ix = ui.effectlist->count() - 1;
        ui.effectlist->setCurrentRow(ix);
        ui.buttonDel->setEnabled(true);
        ui.buttonSave->setEnabled(true);
        ui.buttonReset->setEnabled(true);
        ui.buttonUp->setEnabled(ix > 0);
        ui.buttonDown->setEnabled(ix < clipref->effectsCount() - 1);
    }
}

void EffectStackView::slotItemSelectionChanged()
{
    bool hasItem = ui.effectlist->currentItem();
    int activeRow = ui.effectlist->currentRow();
    bool isChecked = false;
    if (hasItem && ui.effectlist->currentItem()->checkState() == Qt::Checked) isChecked = true;
    if (hasItem && ui.effectlist->currentItem()->isSelected()) {
        emit transferParamDesc(clipref->effectAt(activeRow), clipref->cropStart().frames(KdenliveSettings::project_fps()), clipref->cropDuration().frames(KdenliveSettings::project_fps()));//minx max frame
    }
    if (clipref) clipref->setSelectedEffect(activeRow);
    ui.buttonDel->setEnabled(hasItem);
    ui.buttonSave->setEnabled(hasItem);
    ui.buttonReset->setEnabled(hasItem && isChecked);
    ui.buttonUp->setEnabled(activeRow > 0);
    ui.buttonDown->setEnabled((activeRow < ui.effectlist->count() - 1) && hasItem);
}

void EffectStackView::slotItemUp()
{
    int activeRow = ui.effectlist->currentRow();
    if (activeRow <= 0) return;
    emit changeEffectPosition(clipref, activeRow + 1, activeRow);
}

void EffectStackView::slotItemDown()
{
    int activeRow = ui.effectlist->currentRow();
    if (activeRow >= ui.effectlist->count() - 1) return;
    emit changeEffectPosition(clipref, activeRow + 1, activeRow + 2);
}

void EffectStackView::slotItemDel()
{
    int activeRow = ui.effectlist->currentRow();
    if (activeRow >= 0) {
        emit removeEffect(clipref, clipref->effectAt(activeRow));
    }
}

void EffectStackView::slotResetEffect()
{
    int activeRow = ui.effectlist->currentRow();
    if (activeRow < 0) return;
    QDomElement old = clipref->effectAt(activeRow).cloneNode().toElement();
    QDomElement dom;
    QString effectName = ui.effectlist->currentItem()->text();
    foreach(const QString &type, effectLists.keys()) {
        EffectsList *list = effectLists[type];
        if (list->effectNames().contains(effectName)) {
            dom = list->getEffectByName(effectName);
            break;
        }
    }
    if (!dom.isNull()) {
        dom.setAttribute("kdenlive_ix", old.attribute("kdenlive_ix"));
        emit transferParamDesc(dom, clipref->cropStart().frames(KdenliveSettings::project_fps()), clipref->cropDuration().frames(KdenliveSettings::project_fps()));//minx max frame
        emit updateClipEffect(clipref, old, dom, activeRow);
    }
}


void EffectStackView::raiseWindow(QWidget* dock)
{
    if (clipref && dock)
        dock->raise();
}

void EffectStackView::clear()
{
    ui.effectlist->clear();
    ui.buttonDel->setEnabled(false);
    ui.buttonSave->setEnabled(false);
    ui.buttonReset->setEnabled(false);
    ui.buttonUp->setEnabled(false);
    ui.buttonDown->setEnabled(false);
    effectedit->transferParamDesc(QDomElement(), 0, 0);
}

#include "effectstackview.moc"
