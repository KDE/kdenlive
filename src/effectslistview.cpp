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
    search_effect->setListWidget(m_effectsList);
    buttonInfo->setIcon(KIcon("help-about"));
    setFocusPolicy(Qt::StrongFocus);

    if (KdenliveSettings::showeffectinfo()) {
        buttonInfo->setDown(true);
    } else infopanel->hide();
    menu->addAction(KIcon("edit-delete"), i18n("Delete effect"), this, SLOT(slotRemoveEffect()));

    connect(type_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(filterList(int)));
    connect(buttonInfo, SIGNAL(clicked()), this, SLOT(showInfoPanel()));
    connect(m_effectsList, SIGNAL(itemSelectionChanged()), this, SLOT(slotUpdateInfo()));
    connect(m_effectsList, SIGNAL(doubleClicked(QListWidgetItem *, const QPoint &)), this, SLOT(slotEffectSelected()));

    m_effectsList->setCurrentRow(0);
}


void EffectsListView::focusInEvent(QFocusEvent * event)
{
    kDebug() << "// got foc";
    search_effect->setFocus();
}

void EffectsListView::filterList(int pos)
{
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
            for (i = 0; i < m_effectsList->count() && m_effectsList->item(i)->isHidden(); i++) /*do nothing*/;
            m_effectsList->setCurrentRow(i);
        } else m_effectsList->scrollToItem(item);
    }
}

void EffectsListView::showInfoPanel()
{
    if (infopanel->isVisible()) {
        infopanel->setVisible(false);
        buttonInfo->setDown(false);
        KdenliveSettings::setShoweffectinfo(false);
    } else {
        infopanel->setVisible(true);
        buttonInfo->setDown(true);
        KdenliveSettings::setShoweffectinfo(true);
    }
}

void EffectsListView::slotEffectSelected()
{
    QDomElement effect = m_effectsList->currentEffect();
    if (!effect.isNull()) emit addEffect(effect);
}

void EffectsListView::slotUpdateInfo()
{
    infopanel->setText(m_effectsList->currentInfo());
}

KListWidget *EffectsListView::listView()
{
    return m_effectsList;
}

void EffectsListView::reloadEffectList()
{
    m_effectsList->initList();
}

void EffectsListView::slotRemoveEffect()
{
    QListWidgetItem *item = m_effectsList->currentItem();
    QString effectId = item->text();
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

#include "effectslistview.moc"
