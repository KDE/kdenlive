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

#include <KDebug>
#include <KLocale>

#include "effectstackview.h"
#include "effectslist.h"
#include "clipitem.h"
#include <QHeaderView>
#include <QMenu>

EffectStackView::EffectStackView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent)
        : QWidget(parent) {
    ui.setupUi(this);
    effectedit = new EffectStackEdit(ui.frame, this);
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
    ui.buttonReset->setIcon(KIcon("view-refresh"));
    ui.buttonReset->setToolTip(i18n("Reset effect"));


    ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop);//use internal if drop is recognised right

    connect(ui.effectlist, SIGNAL(itemSelectionChanged()), this , SLOT(slotItemSelectionChanged()));
    connect(ui.effectlist, SIGNAL(itemChanged(QListWidgetItem *)), this , SLOT(slotItemChanged(QListWidgetItem *)));
    connect(ui.buttonNew, SIGNAL(clicked()), this, SLOT(slotNewEffect()));
    connect(ui.buttonUp, SIGNAL(clicked()), this, SLOT(slotItemUp()));
    connect(ui.buttonDown, SIGNAL(clicked()), this, SLOT(slotItemDown()));
    connect(ui.buttonDel, SIGNAL(clicked()), this, SLOT(slotItemDel()));
    connect(ui.buttonReset, SIGNAL(clicked()), this, SLOT(slotResetEffect()));
    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), effectedit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(effectedit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
    effectLists["audio"] = audioEffectList;
    effectLists["video"] = videoEffectList;
    effectLists["custom"] = customEffectList;

    ui.infoBox->hide();
    setEnabled(false);
    setEnabled(false);

}

void EffectStackView::slotUpdateEffectParams(const QDomElement& old, const QDomElement& e) {
    if (clipref)
        emit updateClipEffect(clipref, old, e);
}

void EffectStackView::slotClipItemSelected(ClipItem* c) {
    clipref = c;
    if (clipref == NULL) {
        setEnabled(false);
        return;
    }
    setEnabled(true);
    setupListView();

}

void EffectStackView::slotItemChanged(QListWidgetItem *item) {
    bool disable = true;
    if (item->checkState() == Qt::Checked) disable = false;
    ui.buttonReset->setEnabled(!disable);
    int activeRow = ui.effectlist->currentRow();
    if (activeRow >= 0) {
        emit changeEffectState(clipref, clipref->effectAt(activeRow), disable);
    }
}


void EffectStackView::setupListView() {

    ui.effectlist->clear();
    for (int i = 0;i < clipref->effectsCount();i++) {
        QDomElement d = clipref->effectAt(i);
        QDomNode namenode = d.elementsByTagName("name").item(0);
        if (!namenode.isNull()) {
            QListWidgetItem* item = new QListWidgetItem(namenode.toElement().text(), ui.effectlist);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            if (d.attribute("disabled") == "1") item->setCheckState(Qt::Unchecked);
            else item->setCheckState(Qt::Checked);
        }
    }
    if (clipref->effectsCount() == 0)
        emit transferParamDesc(QDomElement(), 0, 100);
    ui.effectlist->setCurrentRow(0);

}

void EffectStackView::slotItemSelectionChanged() {
    bool hasItem = ui.effectlist->currentItem();
    int activeRow = ui.effectlist->currentRow();
    bool isChecked = ui.effectlist->currentItem()->checkState() == Qt::Checked;
    if (hasItem && ui.effectlist->currentItem()->isSelected()) {
        emit transferParamDesc(clipref->effectAt(activeRow), 0, 100);//minx max frame
    }
    ui.buttonDel->setEnabled(hasItem);
    ui.buttonReset->setEnabled(hasItem && isChecked);
    ui.buttonUp->setEnabled(activeRow > 0);
    ui.buttonDown->setEnabled((activeRow < ui.effectlist->count() - 1) && hasItem);
}

void EffectStackView::slotItemUp() {
    int activeRow = ui.effectlist->currentRow();
    if (activeRow > 0) {
        QDomElement act = clipref->effectAt(activeRow).cloneNode().toElement();
        QDomElement before = clipref->effectAt(activeRow - 1).cloneNode().toElement();
        clipref->setEffectAt(activeRow - 1, act);
        clipref->setEffectAt(activeRow, before);
    }
    QListWidgetItem *item = ui.effectlist->takeItem(activeRow);
    ui.effectlist->insertItem(activeRow - 1, item);
    ui.effectlist->setCurrentItem(item);
    emit refreshEffectStack(clipref);
}

void EffectStackView::slotItemDown() {
    int activeRow = ui.effectlist->currentRow();
    if (activeRow < ui.effectlist->count() - 1) {
        QDomElement act = clipref->effectAt(activeRow).cloneNode().toElement();
        QDomElement after = clipref->effectAt(activeRow + 1).cloneNode().toElement();
        clipref->setEffectAt(activeRow + 1, act);
        clipref->setEffectAt(activeRow, after);
    }
    QListWidgetItem *item = ui.effectlist->takeItem(activeRow);
    ui.effectlist->insertItem(activeRow + 1, item);
    ui.effectlist->setCurrentItem(item);
    emit refreshEffectStack(clipref);
}

void EffectStackView::slotItemDel() {
    int activeRow = ui.effectlist->currentRow();
    if (activeRow >= 0) {
        emit removeEffect(clipref, clipref->effectAt(activeRow));
    }
}

void EffectStackView::slotResetEffect() {
    int activeRow = ui.effectlist->currentRow();
    QDomElement old = clipref->effectAt(activeRow).cloneNode().toElement();
    QDomElement dom;
    QString effectName = ui.effectlist->currentItem()->text();
    foreach(QString type, effectLists.keys()) {
        EffectsList *list = effectLists[type];
        if (list->effectNames().contains(effectName)) {
            dom = list->getEffectByName(effectName);
            break;
        }
    }
    if (!dom.isNull()) {
        dom.setAttribute("kdenlive_ix", old.attribute("kdenlive_ix"));
        emit transferParamDesc(dom, 0, 100);//minx max frame
        emit updateClipEffect(clipref, old, dom);
    }
}

void EffectStackView::slotNewEffect() {


    QMenu *displayMenu = new QMenu(this);
    displayMenu->setTitle("Filters");
    foreach(QString type, effectLists.keys()) {
        QAction *a = new QAction(type, displayMenu);
        EffectsList *list = effectLists[type];

        QMenu *parts = new QMenu(type, displayMenu);
        parts->setTitle(type);
        foreach(QString name, list->effectNames()) {
            QAction *entry = new QAction(name, parts);
            entry->setData(name);
            entry->setToolTip(list->getInfo(name));
            entry->setStatusTip(list->getInfo(name));
            parts->addAction(entry);
            //QAction
        }
        displayMenu->addMenu(parts);

    }

    QAction *result = displayMenu->exec(mapToGlobal(ui.buttonNew->pos() + ui.buttonNew->rect().bottomRight()));

    if (result) {
        //TODO effects.append(result->data().toString());
        foreach(EffectsList* e, effectLists.values()) {
            QDomElement dom = e->getEffectByName(result->data().toString());
            if (clipref)
                clipref->addEffect(dom);
            slotClipItemSelected(clipref);
        }

        setupListView();
        //kDebug()<< result->data();
    }
    delete displayMenu;

}

#include "effectstackview.moc"
