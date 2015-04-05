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


#include "effectslistview.h"
#include "effectslist.h"
#include "effectslistwidget.h"

#include "kdenlivesettings.h"

#include <QDebug>
#include <klocalizedstring.h>

#include <QMenu>
#include <QDir>
#include <QStandardPaths>


EffectsListView::EffectsListView(QWidget *parent) :
        QWidget(parent)
        , m_filterPos(0)
{
    setupUi(this);
    
    m_contextMenu = new QMenu(this);
    m_effectsList = new EffectsListWidget();
    m_effectsList->setStyleSheet(customStyleSheet());
    QVBoxLayout *lyr = new QVBoxLayout(effectlistframe);
    lyr->addWidget(m_effectsList);
    lyr->setContentsMargins(0, 0, 0, 0);
    search_effect->setTreeWidget(m_effectsList);
    search_effect->setToolTip(i18n("Search in the effect list"));
    
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);
    buttonInfo->setIcon(QIcon::fromTheme("help-about"));
    buttonInfo->setToolTip(i18n("Show/Hide the effect description"));
    buttonInfo->setIconSize(iconSize);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(search_effect);
    m_effectsList->setFocusProxy(search_effect);

    if (KdenliveSettings::showeffectinfo())
        buttonInfo->setDown(true);
    else
        infopanel->hide();

    m_removeAction = m_contextMenu->addAction(QIcon::fromTheme("edit-delete"), i18n("Delete effect"), this, SLOT(slotRemoveEffect()));
    
    effectsAll->setIcon(QIcon::fromTheme("kdenlive-show-all-effects"));
    effectsAll->setToolTip(i18n("Show all effects"));
    effectsVideo->setIcon(QIcon::fromTheme("kdenlive-show-video-effects"));
    effectsVideo->setToolTip(i18n("Show video effects"));
    effectsAudio->setIcon(QIcon::fromTheme("kdenlive-show-audio-effects"));
    effectsAudio->setToolTip(i18n("Show audio effects"));
    effectsGPU->setIcon(QIcon::fromTheme("kdenlive-show-gpu-effects"));
    effectsGPU->setToolTip(i18n("Show GPU effects"));
    effectsCustom->setIcon(QIcon::fromTheme("kdenlive-show-custom"));
    effectsCustom->setToolTip(i18n("Show custom effects"));
    m_effectsFavorites = new MyDropButton(this);
    horizontalLayout->addWidget(m_effectsFavorites);
    m_effectsFavorites->setIcon(QIcon::fromTheme("favorites"));
    m_effectsFavorites->setToolTip(i18n("Show favorite effects"));
    connect(m_effectsFavorites, SIGNAL(addEffectToFavorites(QString)), this, SLOT(slotAddFavorite(QString)));
    
    connect(effectsAll, SIGNAL(clicked()), this, SLOT(filterList()));
    connect(effectsVideo, SIGNAL(clicked()), this, SLOT(filterList()));
    connect(effectsAudio, SIGNAL(clicked()), this, SLOT(filterList()));
    connect(effectsGPU, SIGNAL(clicked()), this, SLOT(filterList()));
    connect(effectsCustom, SIGNAL(clicked()), this, SLOT(filterList()));
    connect(m_effectsFavorites, SIGNAL(clicked()), this, SLOT(filterList()));
    connect(buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));
    connect(m_effectsList, &EffectsListWidget::itemSelectionChanged, this, &EffectsListView::slotUpdateInfo);
    connect(m_effectsList, &EffectsListWidget::itemDoubleClicked, this, &EffectsListView::slotEffectSelected);
    connect(m_effectsList, SIGNAL(displayMenu(QTreeWidgetItem *, const QPoint &)), this, SLOT(slotDisplayMenu(QTreeWidgetItem *, const QPoint &)));
    connect(search_effect, SIGNAL(hiddenChanged(QTreeWidgetItem*,bool)), this, SLOT(slotUpdateSearch(QTreeWidgetItem*,bool)));
    connect(m_effectsList, &EffectsListWidget::applyEffect, this, &EffectsListView::addEffect);
    connect(search_effect, SIGNAL(textChanged(QString)), this, SLOT(slotAutoExpand(QString)));
    //m_effectsList->setCurrentRow(0);
}

const QString EffectsListView::customStyleSheet() const
{
    return QString("QTreeView::branch:has-siblings:!adjoins-item{border-image:none;border:0px} \
    QTreeView::branch:has-siblings:adjoins-item {border-image: none;border:0px}      \
    QTreeView::branch:!has-children:!has-siblings:adjoins-item {border-image: none;border:0px} \
    QTreeView::branch:has-children:!has-siblings:closed,QTreeView::branch:closed:has-children:has-siblings {   \
         border-image: none;image: url(:/images/stylesheet-branch-closed.png);}      \
    QTreeView::branch:open:has-children:!has-siblings,QTreeView::branch:open:has-children:has-siblings  {    \
         border-image: none;image: url(:/images/stylesheet-branch-open.png);}");
}

void EffectsListView::slotAddFavorite(QString id)
{
    QStringList favs = KdenliveSettings::favorite_effects();
    if (!favs.contains(id)) {
        favs << id;
        KdenliveSettings::setFavorite_effects(favs);
    }
}

void EffectsListView::filterList()
{
    int pos = 0;
    if (effectsVideo->isChecked()) pos = EffectsListWidget::EFFECT_VIDEO;
    else if (effectsAudio->isChecked()) pos = EffectsListWidget::EFFECT_AUDIO;
    else if (effectsGPU->isChecked()) pos = EffectsListWidget::EFFECT_GPU;
    else if (m_effectsFavorites->isChecked()) pos = EffectsListWidget::EFFECT_FAVORITES;
    else if (effectsCustom->isChecked()) pos = EffectsListWidget::EFFECT_CUSTOM;
    m_filterPos = pos;
    if (m_filterPos == EffectsListWidget::EFFECT_CUSTOM) {
        m_removeAction->setText(i18n("Delete effect"));
    }
    else if (m_filterPos == EffectsListWidget::EFFECT_FAVORITES) {
        m_removeAction->setText(i18n("Remove from favorites"));
    }
    for (int i = 0; i < m_effectsList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *folder = m_effectsList->topLevelItem(i);
        bool hideFolder = true;
        for (int j = 0; j < folder->childCount(); ++j) {
            QTreeWidgetItem *item = folder->child(j);
            if (pos == EffectsListWidget::EFFECT_FAVORITES) {
                QStringList data = item->data(0, Qt::UserRole + 1).toStringList();
                QString id = data.at(1);
                if (id.isEmpty()) id = data.at(0);
                if (KdenliveSettings::favorite_effects().contains(id)) {
                    item->setHidden(false);
                    hideFolder = false;
                }
                else {
                    item->setHidden(true);
                }
            }
            else if (pos == 0 || pos == item->data(0, Qt::UserRole).toInt()) {
                item->setHidden(false);
                hideFolder = false;
            } else {
                item->setHidden(true);
            }
        }
        // do not hide the folder if it's empty but "All" is selected
        if (pos == 0)
            hideFolder = false;
        folder->setHidden(hideFolder);
    }
    // make sure we don't show anything not matching the search expression
    search_effect->updateSearch();


    /*item = m_effectsList->currentItem();
    if (item) {
        if (item->isHidden()) {
            int i;
            for (i = 0; i < m_effectsList->count() && m_effectsList->item(i)->isHidden(); ++i); //do nothing
            m_effectsList->setCurrentRow(i);
        } else m_effectsList->scrollToItem(item);
    }*/
}

void EffectsListView::showInfoPanel()
{
    bool show = !infopanel->isVisible();
    infopanel->setVisible(show);
    buttonInfo->setDown(show);
    KdenliveSettings::setShoweffectinfo(show);
}

void EffectsListView::slotEffectSelected()
{
    QDomElement effect = m_effectsList->currentEffect();
    QTreeWidgetItem* item=m_effectsList->currentItem();
    if (item &&  m_effectsList->indexOfTopLevelItem(item)!=-1){
	item->setExpanded(!item->isExpanded());		
    }
    if (!effect.isNull())
        emit addEffect(effect);
}

void EffectsListView::slotUpdateInfo()
{
    infopanel->setText(m_effectsList->currentInfo());
}

void EffectsListView::reloadEffectList(QMenu *effectsMenu, KActionCategory *effectActions)
{
    m_effectsList->initList(effectsMenu, effectActions);
}

void EffectsListView::slotDisplayMenu(QTreeWidgetItem *item, const QPoint &pos)
{
    if (m_filterPos == EffectsListWidget::EFFECT_FAVORITES) {
        m_contextMenu->popup(pos);
    }
    else if (item->data(0, Qt::UserRole + 1).toInt() == EffectsListWidget::EFFECT_CUSTOM) {
        m_contextMenu->popup(pos);
    }
    
}

void EffectsListView::slotRemoveEffect()
{
    if (m_filterPos == EffectsListWidget::EFFECT_FAVORITES) {
        QDomElement e = m_effectsList->currentEffect();
        QString id = e.attribute("id");
        if (id.isEmpty()) {
            id = e.attribute("tag");
        }
        QStringList favs = KdenliveSettings::favorite_effects();
        favs.removeAll(id);
        KdenliveSettings::setFavorite_effects(favs);
        filterList();
        return;
    }

    QTreeWidgetItem *item = m_effectsList->currentItem();
    QString effectId = item->text(0);
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/effects";

    QDir directory = QDir(path);
    QStringList filter;
    filter << "*.xml";
    const QStringList fileList = directory.entryList(filter, QDir::Files);
    QString itemName;
    foreach(const QString &filename, fileList) {
        itemName = QUrl(path + filename).path();
        QDomDocument doc;
        QFile file(itemName);
        doc.setContent(&file, false);
        file.close();
        QDomNodeList effects = doc.elementsByTagName("effect");
        if (effects.count() != 1) {
            //qDebug() << "More than one effect in file " << itemName << ", NOT SUPPORTED YET";
        } else {
            QDomElement e = effects.item(0).toElement();
            if (e.attribute("id") == effectId) {
                QFile::remove(itemName);
                break;
            }
        }
    }
    emit reloadEffects();
}

void EffectsListView::slotUpdateSearch(QTreeWidgetItem *item, bool hidden)
{
    if (!hidden) {
        if (item->data(0, Qt::UserRole).toInt() == m_filterPos) {
            if (item->parent())
                item->parent()->setHidden(false);
        } else {
            if (m_filterPos != 0)
                item->setHidden(true);
        }
    }
}

void EffectsListView::slotAutoExpand(const QString &text)
{
    QTreeWidgetItem *curr = m_effectsList->currentItem();
    search_effect->updateSearch();
    bool selected = false;
    if (curr && !curr->isHidden()) {
        selected = true;
    }
    for (int i = 0; i < m_effectsList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *folder = m_effectsList->topLevelItem(i);
        bool expandFolder = false;
        /*if (folder->isHidden())
            continue;*/
        if (text.isEmpty()) {
            if (curr && curr->parent() == folder) {
                expandFolder = true;
            }
        }
        else {
            for (int j = 0; j < folder->childCount(); ++j) {
                QTreeWidgetItem *item = folder->child(j);
                if (!item->isHidden()) {
                    expandFolder = true;
		    if (!selected) {
			m_effectsList->setCurrentItem(item);
			selected = true;
		    }
		}
            }
        }
        folder->setExpanded(expandFolder);
    }
    if (!selected) m_effectsList->setCurrentItem(NULL);
}

void EffectsListView::updatePalette()
{
    // We need to reset current stylesheet if we want to change the palette!
    m_effectsList->setStyleSheet("");
    m_effectsList->updatePalette();
    m_effectsList->setStyleSheet(customStyleSheet());
}


