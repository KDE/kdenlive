/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include <KDebug>
#include <KLocale>

#include "effectslistview.h"
#include "effectslistwidget.h"
#include "effectslist.h"

EffectsListView::EffectsListView(QWidget *parent)
        : QWidget(parent) {
    m_effectsList = new EffectsListWidget();

    ui.setupUi(this);
    QVBoxLayout *lyr = new QVBoxLayout(ui.effectlistframe);
    lyr->addWidget(m_effectsList);
    ui.search_effect->setListWidget(m_effectsList);
    ui.buttonInfo->setIcon(KIcon("help-about"));
    ui.infopanel->hide();

    connect(ui.type_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterList(int)));
    connect(ui.buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));
    connect(m_effectsList, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateInfo()));
    connect(m_effectsList, SIGNAL(doubleClicked(QListWidgetItem *, const QPoint &)), this, SLOT(slotEffectSelected()));

    m_effectsList->setCurrentRow(0);
}


void EffectsListView::filterList(int pos) {
    QListWidgetItem *item;
    for (int i = 0; i < m_effectsList->count(); i++) {
        item = m_effectsList->item(i);
        if (pos == 0) item->setHidden(false);
        else if (item->data(Qt::UserRole).toInt() == pos) item->setHidden(false);
        else item->setHidden(true);
    }
    item = m_effectsList->currentItem();
    if (item) {
        if (item->isHidden()) {
            int i;
            for (i = 0; i < m_effectsList->count() && m_effectsList->item(i)->isHidden(); i++);
            m_effectsList->setCurrentRow(i);
        } else m_effectsList->scrollToItem(item);
    }
}

void EffectsListView::showInfoPanel() {
    if (ui.infopanel->isVisible()) {
        ui.infopanel->setVisible(false);
        ui.buttonInfo->setDown(false);
    } else {
        ui.infopanel->setVisible(true);
        ui.buttonInfo->setDown(true);
    }
}

void EffectsListView::slotEffectSelected() {
    QDomElement effect = m_effectsList->currentEffect();
    if (!effect.isNull()) emit addEffect(effect);
}

void EffectsListView::slotUpdateInfo() {
    QString info = m_effectsList->currentInfo();
    if (!info.isEmpty()) ui.infopanel->setText(info);
}

KListWidget *EffectsListView::listView() {
    return m_effectsList;
}

#include "effectslistview.moc"
