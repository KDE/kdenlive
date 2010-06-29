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
#include "effectslistwidget.h"
#include "effectslist.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KLocale>
#include <KStandardDirs>

#include <QMenu>
#include <QDir>

EffectsListView::EffectsListView(QWidget *parent) :
        QWidget(parent)
{
    setupUi(this);

    QMenu *menu = new QMenu(this);
    m_effectsList = new EffectsListWidget(menu);
    QVBoxLayout *lyr = new QVBoxLayout(effectlistframe);
    lyr->addWidget(m_effectsList);
    lyr->setContentsMargins(0, 0, 0, 0);
    search_effect->setTreeWidget(m_effectsList);
    search_effect->setToolTip(i18n("Search in the effect list"));
    buttonInfo->setIcon(KIcon("help-about"));
    buttonInfo->setToolTip(i18n("Show/Hide the effect description"));
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(search_effect);
    m_effectsList->setFocusProxy(search_effect);

    if (KdenliveSettings::showeffectinfo())
        buttonInfo->setDown(true);
    else
        infopanel->hide();

    menu->addAction(KIcon("edit-delete"), i18n("Delete effect"), this, SLOT(slotRemoveEffect()));

    connect(type_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterList(int)));
    connect(buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));
    connect(m_effectsList, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateInfo()));
    connect(m_effectsList, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotEffectSelected()));
    connect(search_effect, SIGNAL(hiddenChanged(QTreeWidgetItem *, bool)), this, SLOT(slotUpdateSearch(QTreeWidgetItem *, bool)));
    connect(search_effect, SIGNAL(textChanged(QString)), this, SLOT(slotAutoExpand(QString)));
    //m_effectsList->setCurrentRow(0);
}

void EffectsListView::filterList(int pos)
{
    for (int i = 0; i < m_effectsList->topLevelItemCount(); i++) {
        QTreeWidgetItem *folder = m_effectsList->topLevelItem(i);
        bool hideFolder = true;
        for (int j = 0; j < folder->childCount(); j++) {
            QTreeWidgetItem *item = folder->child(j);
            if (pos == 0 || pos == item->data(0, Qt::UserRole).toInt()) {
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
            for (i = 0; i < m_effectsList->count() && m_effectsList->item(i)->isHidden(); i++); //do nothing
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
    if (!effect.isNull())
        emit addEffect(effect);
}

void EffectsListView::slotUpdateInfo()
{
    infopanel->setText(m_effectsList->currentInfo());
}

void EffectsListView::reloadEffectList()
{
    m_effectsList->initList();
}

void EffectsListView::slotRemoveEffect()
{
    QTreeWidgetItem *item = m_effectsList->currentItem();
    QString effectId = item->text(0);
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);

    QDir directory = QDir(path);
    QStringList filter;
    filter << "*.xml";
    const QStringList fileList = directory.entryList(filter, QDir::Files);
    QString itemName;
    foreach(const QString &filename, fileList) {
        itemName = KUrl(path + filename).path();
        QDomDocument doc;
        QFile file(itemName);
        doc.setContent(&file, false);
        file.close();
        QDomNodeList effects = doc.elementsByTagName("effect");
        if (effects.count() != 1) {
            kDebug() << "More than one effect in file " << itemName << ", NOT SUPPORTED YET";
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
        if (item->data(0, Qt::UserRole).toInt() == type_combo->currentIndex()) {
            if (item->parent())
                item->parent()->setHidden(false);
        } else {
            if (type_combo->currentIndex() != 0)
                item->setHidden(true);
        }
    }
}

void EffectsListView::slotAutoExpand(QString text)
{
    search_effect->updateSearch();

    for (int i = 0; i < m_effectsList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *folder = m_effectsList->topLevelItem(i);
        bool expandFolder = false;
        /*if (folder->isHidden())
            continue;*/
        if (!text.isEmpty()) {
            for (int j = 0; j < folder->childCount(); j++) {
                QTreeWidgetItem *item = folder->child(j);
                if (!item->isHidden())
                    expandFolder = true;
            }
        }
        folder->setExpanded(expandFolder);
    }
}

#include "effectslistview.moc"
