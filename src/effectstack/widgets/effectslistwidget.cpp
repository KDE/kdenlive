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


#include "effectslistwidget.h"
#include "effectslist.h"
#include "mainwindow.h"

#include <KDebug>
#include <KStandardDirs>
#include <KAction>

#include <QApplication>
#include <QMouseEvent>
#include <QMenu>


static const int EFFECT_VIDEO = 1;
static const int EFFECT_AUDIO = 2;
static const int EFFECT_CUSTOM = 3;
static const int EFFECT_FOLDER = 4;

const int TypeRole = Qt::UserRole;
const int IdRole = TypeRole + 1;


EffectsListWidget::EffectsListWidget(QMenu *contextMenu, QWidget *parent) :
    QTreeWidget(parent),
    m_menu(contextMenu)
{
    setColumnCount(1);
    setDragEnabled(true);
    setAcceptDrops(false);
    setHeaderHidden(true);
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    setRootIsDecorated(true);
    setIndentation(10);
    //setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::DragOnly);
    QPalette p = palette();
    p.setBrush(QPalette::Base, Qt::NoBrush);
    setPalette(p);
    connect(this, SIGNAL(activated(QModelIndex)), this, SLOT(slotExpandItem(QModelIndex)));
}

EffectsListWidget::~EffectsListWidget()
{
}

void EffectsListWidget::slotExpandItem(const QModelIndex & index)
{
    setExpanded(index, !isExpanded(index));
}

void EffectsListWidget::initList(QMenu *effectsMenu, KActionCategory *effectActions)
{
    QString current;
    QString currentFolder;
    bool found = false;
    effectsMenu->clear();
    
    if (currentItem()) {
        current = currentItem()->text(0);
        if (currentItem()->parent())
            currentFolder = currentItem()->parent()->text(0);
        else if (currentItem()->data(0, TypeRole) ==  EFFECT_FOLDER)
            currentFolder = currentItem()->text(0);
    }

    QString effectCategory = KStandardDirs::locate("config", "kdenliveeffectscategory.rc");
    QDomDocument doc;
    QFile file(effectCategory);
    doc.setContent(&file, false);
    file.close();
    QList <QTreeWidgetItem *> folders;
    QStringList folderNames;
    QDomNodeList groups = doc.documentElement().elementsByTagName("group");
    for (int i = 0; i < groups.count(); ++i) {
        folderNames << i18n(groups.at(i).firstChild().firstChild().nodeValue().toUtf8().constData());
    }
    for (int i = 0; i < topLevelItemCount(); ++i) {
        topLevelItem(i)->takeChildren();
        QString currentName = topLevelItem(i)->text(0);
        if (currentName != i18n("Misc") && currentName != i18n("Audio") && currentName != i18nc("Folder Name", "Custom") && !folderNames.contains(currentName)) {
            takeTopLevelItem(i);
            --i;
        }
    }

    for (int i = 0; i < groups.count(); ++i) {
        QTreeWidgetItem *item = findFolder(folderNames.at(i));
        if (item) {
            item->setData(0, IdRole, groups.at(i).toElement().attribute("list"));
        } else {
            item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(folderNames.at(i)));
            item->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
            item->setData(0, IdRole, groups.at(i).toElement().attribute("list"));
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
            insertTopLevelItem(0, item);
        }
        folders.append(item);
    }

    QTreeWidgetItem *misc = findFolder(i18n("Misc"));
    if (misc == NULL) {
        misc = new QTreeWidgetItem((QTreeWidget*)0, QStringList(i18n("Misc")));
        misc->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
        misc->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        insertTopLevelItem(0, misc);
    }

    QTreeWidgetItem *audio = findFolder(i18n("Audio"));
    if (audio == NULL) {
        audio = new QTreeWidgetItem((QTreeWidget*)0, QStringList(i18n("Audio")));
        audio->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
        audio->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        insertTopLevelItem(0, audio);
    }

    QTreeWidgetItem *custom = findFolder(i18nc("Folder Name", "Custom"));
    if (custom == NULL) {
        custom = new QTreeWidgetItem((QTreeWidget*)0, QStringList(i18nc("Folder Name", "Custom")));
        custom->setData(0, TypeRole, QString::number((int) EFFECT_FOLDER));
        custom->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        insertTopLevelItem(0, custom);
    }

    //insertTopLevelItems(0, folders);

    loadEffects(&MainWindow::videoEffects, KIcon("kdenlive-show-video"), misc, &folders, EFFECT_VIDEO, current, &found);
    loadEffects(&MainWindow::audioEffects, KIcon("kdenlive-show-audio"), audio, &folders, EFFECT_AUDIO, current, &found);
    loadEffects(&MainWindow::customEffects, KIcon("kdenlive-custom-effect"), custom, static_cast<QList<QTreeWidgetItem *> *>(0), EFFECT_CUSTOM, current, &found);

    if (!found && !currentFolder.isEmpty()) {
        // previously selected effect was removed, focus on its parent folder
        for (int i = 0; i < topLevelItemCount(); ++i) {
            if (topLevelItem(i)->text(0) == currentFolder) {
                setCurrentItem(topLevelItem(i));
                break;
            }
        }

    }
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    // populate effects menu
    QMenu *sub1 = NULL;
    QMenu *sub2 = NULL;
    QMenu *sub3 = NULL;
    QMenu *sub4 = NULL;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (!topLevelItem(i)->childCount())
            continue;
        QMenu *sub = new QMenu(topLevelItem(i)->text(0), effectsMenu);
        effectsMenu->addMenu(sub);
        int effectsInCategory = topLevelItem(i)->childCount();
        bool hasSubCategories = false;
        if (effectsInCategory > 60) {
            // create subcategories if there are too many effects
            hasSubCategories = true;
            sub1 = new QMenu(i18nc("menu name for effects names between these 2 letters", "0 - F"), sub);
            sub->addMenu(sub1);
            sub2 = new QMenu(i18nc("menu name for effects names between these 2 letters", "G - L"), sub);
            sub->addMenu(sub2);
            sub3 = new QMenu(i18nc("menu name for effects names between these 2 letters", "M - R"), sub);
            sub->addMenu(sub3);
            sub4 = new QMenu(i18nc("menu name for effects names between these 2 letters", "S - Z"), sub);
            sub->addMenu(sub4);
        }
        for (int j = 0; j < effectsInCategory; ++j) {
            QTreeWidgetItem *item = topLevelItem(i)->child(j);
            KAction *a = new KAction(KIcon(item->icon(0)), item->text(0), sub);
            QStringList data = item->data(0, IdRole).toStringList();
            QString id = data.at(1);
            if (id.isEmpty()) id = data.at(0);
            a->setData(data);
            a->setIconVisibleInMenu(false);
            if (hasSubCategories) {
                // put action in sub category
                QRegExp rx("^[s-z].+");
                if (rx.exactMatch(item->text(0).toLower())) {
                    sub4->addAction(a);
                } else {
                    rx.setPattern("^[m-r].+");
                    if (rx.exactMatch(item->text(0).toLower())) {
                        sub3->addAction(a);
                    }
                    else {
                        rx.setPattern("^[g-l].+");
                        if (rx.exactMatch(item->text(0).toLower())) {
                            sub2->addAction(a);
                        }
                        else sub1->addAction(a);
                    }
                }
            }
            else sub->addAction(a);
            effectActions->addAction("video_effect_" + id, a);
        }
    }
}

void EffectsListWidget::loadEffects(const EffectsList *effectlist, KIcon icon, QTreeWidgetItem *defaultFolder, const QList<QTreeWidgetItem *> *folders, int type, const QString &current, bool *found)
{
    QStringList effectInfo, l;
    QTreeWidgetItem *item;
    int ct = effectlist->count();

    
    for (int ix = 0; ix < ct; ++ix) {
        effectInfo = effectlist->effectIdInfo(ix);
        effectInfo.append(QString::number(type));
        QTreeWidgetItem *parentItem = NULL;

        if (folders) {
            for (int i = 0; i < folders->count(); ++i) {
                l = folders->at(i)->data(0, IdRole).toString().split(QLatin1Char(','), QString::SkipEmptyParts);
                if (l.contains(effectInfo.at(2))) {
                    parentItem = folders->at(i);
                    break;
                }
            }
        }
        if (parentItem == NULL)
            parentItem = defaultFolder;

        if (!effectInfo.isEmpty()) {
            item = new QTreeWidgetItem(parentItem, QStringList(effectInfo.takeFirst()));
            if (effectInfo.count() == 4) item->setIcon(0, KIcon("folder"));
            else item->setIcon(0, icon);
            item->setData(0, TypeRole, type);
            item->setData(0, IdRole, effectInfo);
            item->setToolTip(0, effectlist->getInfo(effectInfo.at(0), effectInfo.at(1)));
            if (item->text(0) == current) {
                setCurrentItem(item);
                *found = true;
            }
        }
    }
}

QTreeWidgetItem *EffectsListWidget::findFolder(const QString &name)
{
    QTreeWidgetItem *item = NULL;
    QList<QTreeWidgetItem *> result = findItems(name, Qt::MatchExactly);
    if (!result.isEmpty()) {
        for (int j = 0; j < result.count(); ++j) {
            if (result.at(j)->data(0, TypeRole) ==  EFFECT_FOLDER) {
                item = result.at(j);
                break;
            }
        }
    }
    return item;
}

const QDomElement EffectsListWidget::currentEffect() const
{
    return itemEffect(currentItem());
}

const QDomElement EffectsListWidget::itemEffect(QTreeWidgetItem *item) const
{
    QDomElement effect;
    if (!item || item->data(0, TypeRole).toInt() == (int)EFFECT_FOLDER) return effect;
    QStringList effectInfo = item->data(0, IdRole).toStringList();
    switch (item->data(0, TypeRole).toInt()) {
    case 1:
        effect =  MainWindow::videoEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    case 2:
        effect = MainWindow::audioEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    default:
        effect = MainWindow::customEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    }
    return effect;
}


QString EffectsListWidget::currentInfo() const
{
    QTreeWidgetItem *item = currentItem();
    if (!item || item->data(0, TypeRole).toInt() == (int)EFFECT_FOLDER) return QString();
    QString info;
    QStringList effectInfo = item->data(0, IdRole).toStringList();
    switch (item->data(0, TypeRole).toInt()) {
    case 1:
        info = MainWindow::videoEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    case 2:
        info = MainWindow::audioEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    default:
        info = MainWindow::customEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    }
    return info;
}

//virtual
void EffectsListWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
        emit applyEffect(currentEffect());
        e->accept();
        return;
    }
    QTreeWidget::keyPressEvent(e);
}

//virtual
QMimeData * EffectsListWidget::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QDomDocument doc;
    foreach(QTreeWidgetItem *item, list) {
        if (item->flags() & Qt::ItemIsDragEnabled) {
            const QDomElement e = itemEffect(item);
            if (!e.isNull())
                doc.appendChild(doc.importNode(e, true));
        }
    }
    QMimeData *mime = new QMimeData;
    QByteArray data;
    data.append(doc.toString().toUtf8());
    mime->setData("kdenlive/effectslist", data);
    return mime;
}

//virtual
void EffectsListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}


//virtual
void EffectsListWidget::contextMenuEvent(QContextMenuEvent * event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    if (item && item->data(0, TypeRole).toInt() == EFFECT_CUSTOM)
        m_menu->popup(event->globalPos());
}

#include "effectslistwidget.moc"
