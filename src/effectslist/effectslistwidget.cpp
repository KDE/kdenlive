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
#include "effectslist/effectslist.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"

#include "klocalizedstring.h"
#include <QAction>
#include <QMenu>
#include <QMimeData>
#include <QStandardPaths>

EffectsListWidget::EffectsListWidget(QWidget *parent) :
    QTreeWidget(parent)
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
    updatePalette();
    connect(this, &EffectsListWidget::activated, this, &EffectsListWidget::slotExpandItem);
}

EffectsListWidget::~EffectsListWidget()
{
}

void EffectsListWidget::updatePalette()
{
    QPalette p = qApp->palette();
    p.setBrush(QPalette::Base, QBrush(Qt::transparent));
    setPalette(p);
}

void EffectsListWidget::slotExpandItem(const QModelIndex &index)
{
    setExpanded(index, !isExpanded(index));
}

void EffectsListWidget::initList(QMenu *effectsMenu, KActionCategory *effectActions, const QString &categoryFile, bool transitionMode)
{
    QString current;
    QString currentFolder;
    bool found = false;
    effectsMenu->clear();

    if (currentItem()) {
        current = currentItem()->text(0);
        if (currentItem()->parent()) {
            currentFolder = currentItem()->parent()->text(0);
        } else if (currentItem()->data(0, TypeRole) == EffectsList::EFFECT_FOLDER) {
            currentFolder = currentItem()->text(0);
        }
    }

    QTreeWidgetItem *misc = nullptr;
    QTreeWidgetItem *audio = nullptr;
    QTreeWidgetItem *custom = nullptr;
    QList<QTreeWidgetItem *> folders;

    if (!categoryFile.isEmpty()) {
        QDomDocument doc;
        QFile file(categoryFile);
        doc.setContent(&file, false);
        file.close();
        QStringList folderNames;
        QDomNodeList groups = doc.documentElement().elementsByTagName(QStringLiteral("group"));
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
                item->setData(0, IdRole, groups.at(i).toElement().attribute(QStringLiteral("list")));
            } else {
                item = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(folderNames.at(i)));
                item->setData(0, TypeRole, QString::number((int) EffectsList::EFFECT_FOLDER));
                item->setData(0, IdRole, groups.at(i).toElement().attribute(QStringLiteral("list")));
                item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
                insertTopLevelItem(0, item);
            }
            folders.append(item);
        }

        misc = findFolder(i18n("Misc"));
        if (misc == nullptr) {
            misc = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(i18n("Misc")));
            misc->setData(0, TypeRole, QString::number((int) EffectsList::EFFECT_FOLDER));
            misc->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            insertTopLevelItem(0, misc);
        }

        audio = findFolder(i18n("Audio"));
        if (audio == nullptr) {
            audio = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(i18n("Audio")));
            audio->setData(0, TypeRole, QString::number((int) EffectsList::EFFECT_FOLDER));
            audio->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            insertTopLevelItem(0, audio);
        }

        custom = findFolder(i18nc("Folder Name", "Custom"));
        if (custom == nullptr) {
            custom = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(i18nc("Folder Name", "Custom")));
            custom->setData(0, TypeRole, QString::number((int) EffectsList::EFFECT_FOLDER));
            custom->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            insertTopLevelItem(0, custom);
        }
    }

    //insertTopLevelItems(0, folders);
    if (transitionMode) {
        loadEffects(&MainWindow::transitions, misc, &folders, EffectsList::TRANSITION_TYPE, current, &found);
    } else {
        loadEffects(&MainWindow::videoEffects, misc, &folders, EffectsList::EFFECT_VIDEO, current, &found);
        loadEffects(&MainWindow::audioEffects, audio, &folders, EffectsList::EFFECT_AUDIO, current, &found);
        loadEffects(&MainWindow::customEffects, custom, static_cast<QList<QTreeWidgetItem *> *>(nullptr), EffectsList::EFFECT_CUSTOM, current, &found);
        if (!found && !currentFolder.isEmpty()) {
            // previously selected effect was removed, focus on its parent folder
            for (int i = 0; i < topLevelItemCount(); ++i) {
                if (topLevelItem(i)->text(0) == currentFolder) {
                    setCurrentItem(topLevelItem(i));
                    break;
                }
            }
        }
    }
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    // populate effects menu
    QMenu *sub1 = nullptr;
    QMenu *sub2 = nullptr;
    QMenu *sub3 = nullptr;
    QMenu *sub4 = nullptr;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (topLevelItem(i)->data(0, TypeRole) == EffectsList::TRANSITION_TYPE) {
            QTreeWidgetItem *item = topLevelItem(i);
            QAction *a = new QAction(item->icon(0), item->text(0), effectsMenu);
            QStringList data = item->data(0, IdRole).toStringList();
            QString id = data.at(1);
            if (id.isEmpty()) {
                id = data.at(0);
            }
            a->setData(data);
            a->setIconVisibleInMenu(false);
            effectsMenu->addAction(a);
            effectActions->addAction("transition_" + id, a);
            continue;
        }
        if (!topLevelItem(i)->childCount()) {
            continue;
        }
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
            QAction *a = new QAction(item->icon(0), item->text(0), sub);
            QStringList data = item->data(0, IdRole).toStringList();
            QString id = data.at(1);
            if (id.isEmpty()) {
                id = data.at(0);
            }
            a->setData(data);
            a->setIconVisibleInMenu(false);
            if (hasSubCategories) {
                // put action in sub category
                QRegExp rx("^[s-z].+");
                if (rx.exactMatch(item->text(0).toLower())) {
                    sub4->addAction(a);
                } else {
                    rx.setPattern(QStringLiteral("^[m-r].+"));
                    if (rx.exactMatch(item->text(0).toLower())) {
                        sub3->addAction(a);
                    } else {
                        rx.setPattern(QStringLiteral("^[g-l].+"));
                        if (rx.exactMatch(item->text(0).toLower())) {
                            sub2->addAction(a);
                        } else {
                            sub1->addAction(a);
                        }
                    }
                }
            } else {
                sub->addAction(a);
            }
            effectActions->addAction("video_effect_" + id, a);
        }
    }
}

void EffectsListWidget::loadEffects(const EffectsList *effectlist, QTreeWidgetItem *defaultFolder, const QList<QTreeWidgetItem *> *folders, int type, const QString &current, bool *found)
{
    QStringList effectInfo, l;
    QTreeWidgetItem *item;
    int ct = effectlist->count();
    QFontMetrics f(font());
    int fontSize = f.height();

    for (int ix = 0; ix < ct; ++ix) {
        const QDomElement effect = effectlist->at(ix);
        effectInfo = effectlist->effectInfo(effect);
        if (effectInfo.isEmpty()) {
            continue;
        }
        QTreeWidgetItem *parentItem = nullptr;

        if (folders) {
            for (int i = 0; i < folders->count(); ++i) {
                l = folders->at(i)->data(0, IdRole).toString().split(QLatin1Char(','), QString::SkipEmptyParts);
                if (l.contains(effectInfo.at(2))) {
                    parentItem = folders->at(i);
                    break;
                }
            }
        }
        if (parentItem == nullptr) {
            parentItem = defaultFolder;
        }

        QIcon icon2 = generateIcon(fontSize, effectInfo.at(0), effect);
        item = new QTreeWidgetItem(parentItem, QStringList(effectInfo.takeFirst()));
        QString tag = effectInfo.at(0);
        if (type != EffectsList::EFFECT_CUSTOM && tag.startsWith(QLatin1String("movit."))) {
            // GPU effect
            effectInfo.append(QString::number(EffectsList::EFFECT_GPU));
            item->setData(0, TypeRole, EffectsList::EFFECT_GPU);
        } else {
            effectInfo.append(QString::number(type));
            item->setData(0, TypeRole, type);
        }
        if (effectInfo.count() == 4) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("folder")));
        } else {
            item->setIcon(0, icon2);
        }
        item->setData(0, IdRole, effectInfo);
        item->setToolTip(0, effectlist->getEffectInfo(effect));
        if (parentItem == nullptr) {
            addTopLevelItem(item);
        }
        if (item->text(0) == current) {
            setCurrentItem(item);
            *found = true;
        }
    }
}

QTreeWidgetItem *EffectsListWidget::findFolder(const QString &name)
{
    QTreeWidgetItem *item = nullptr;
    QList<QTreeWidgetItem *> result = findItems(name, Qt::MatchExactly);
    if (!result.isEmpty()) {
        for (int j = 0; j < result.count(); ++j) {
            if (result.at(j)->data(0, TypeRole) == EffectsList::EFFECT_FOLDER) {
                item = result.at(j);
                break;
            }
        }
    }
    return item;
}

const QDomElement EffectsListWidget::currentEffect() const
{
    QTreeWidgetItem *item = currentItem();
    if (!item) {
        return QDomElement();
    }
    int type = item->data(0, TypeRole).toInt();
    QStringList info = item->data(0, IdRole).toStringList();
    return itemEffect(type, info);
}

QIcon EffectsListWidget::generateIcon(int size, const QString &name, QDomElement info)
{
    QPixmap pix(size, size);
    if (name.isEmpty()) {
        pix.fill(Qt::red);
        return QIcon(pix);
    }
    QFont ft = font();
    ft.setBold(true);
    uint hex = qHash(name);
    QString t = "#" + QString::number(hex, 16).toUpper().left(6);
    QColor col(t);
    info.setAttribute(QStringLiteral("effectcolor"), col.name());
    bool isAudio = info.attribute(QStringLiteral("type")) == QLatin1String("audio");
    if (isAudio) {
        pix.fill(Qt::transparent);
    } else {
        pix.fill(col);
    }
    QPainter p(&pix);
    if (isAudio) {
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(pix.rect());
        p.setPen(QPen());
    }
    p.setFont(ft);
    p.drawText(pix.rect(), Qt::AlignCenter, name.at(0));
    p.end();
    return QIcon(pix);
}

const QDomElement EffectsListWidget::itemEffect(int type, const QStringList &effectInfo)
{
    QDomElement effect;
    if (type == (int)EffectsList::EFFECT_FOLDER) {
        return effect;
    }
    switch (type) {
    case EffectsList::EFFECT_VIDEO:
    case EffectsList::EFFECT_GPU:
        effect =  MainWindow::videoEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    case EffectsList::EFFECT_AUDIO:
        effect = MainWindow::audioEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    case EffectsList::EFFECT_CUSTOM:
        effect = MainWindow::customEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    case EffectsList::TRANSITION_TYPE:
        effect = MainWindow::transitions.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    default:
        effect =  MainWindow::videoEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        if (!effect.isNull()) {
            break;
        }
        effect = MainWindow::audioEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        if (!effect.isNull()) {
            break;
        }
        effect = MainWindow::customEffects.getEffectByTag(effectInfo.at(0), effectInfo.at(1)).cloneNode().toElement();
        break;
    }
    return effect;
}

QString EffectsListWidget::currentInfo() const
{
    QTreeWidgetItem *item = currentItem();
    if (!item || item->data(0, TypeRole).toInt() == (int)EffectsList::EFFECT_FOLDER) {
        return QString();
    }
    QString info;
    QStringList effectInfo = item->data(0, IdRole).toStringList();
    switch (item->data(0, TypeRole).toInt()) {
    case EffectsList::EFFECT_VIDEO:
    case EffectsList::EFFECT_GPU:
        info = MainWindow::videoEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    case EffectsList::EFFECT_AUDIO:
        info = MainWindow::audioEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    case EffectsList::EFFECT_CUSTOM:
        info = MainWindow::customEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    case EffectsList::TRANSITION_TYPE:
        info = MainWindow::transitions.getInfo(effectInfo.at(0), effectInfo.at(1));
        break;
    default:
        info = MainWindow::videoEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        if (!info.isEmpty()) {
            break;
        }
        info = MainWindow::audioEffects.getInfo(effectInfo.at(0), effectInfo.at(1));
        if (!info.isEmpty()) {
            break;
        }
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
QMimeData *EffectsListWidget::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QDomDocument doc;
    bool transitionMode = false;
    for (QTreeWidgetItem *item : list) {
        if (item->flags() & Qt::ItemIsDragEnabled) {
            int type = item->data(0, TypeRole).toInt();
            if (type == EffectsList::TRANSITION_TYPE) {
                transitionMode = true;
            }
            QStringList info = item->data(0, IdRole).toStringList();
            const QDomElement e = itemEffect(type, info);
            if (!e.isNull()) {
                doc.appendChild(doc.importNode(e, true));
            }
        }
    }
    QMimeData *mime = new QMimeData;
    QByteArray data;
    data.append(doc.toString().toUtf8());
    mime->setData(transitionMode ? "kdenlive/transitionslist" : "kdenlive/effectslist", data);
    return mime;
}

//virtual
void EffectsListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

//virtual
void EffectsListWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    if (item && item->data(0, TypeRole) !=  EffectsList::EFFECT_FOLDER) {
        emit displayMenu(item, event->globalPos());
    }
}

void EffectsListWidget::setRootOnCustomFolder()
{
    // special case, display only items in "custom" folder"
    QTreeWidgetItem *item = findFolder(i18nc("Folder Name", "Custom"));
    if (!item) {
        // No custom effect, show empty list
        for (int i = 0; i < topLevelItemCount(); ++i) {
            QTreeWidgetItem *folder = topLevelItem(i);
            folder->setHidden(true);
        }
        return;
    }
    setRootIndex(indexFromItem(item));
    for (int j = 0; j < item->childCount(); ++j) {
        QTreeWidgetItem *child = item->child(j);
        child->setHidden(false);
    }
}

void EffectsListWidget::resetRoot()
{
    setRootIndex(indexFromItem(invisibleRootItem()));
}
void EffectsListWidget::createTopLevelItems(const QList<QTreeWidgetItem *> &list, int effectType)
{
    // Favorites is a pseudo-folder used to store items, not visible to end user, so don't i18n its name
    QTreeWidgetItem *misc = findFolder(QStringLiteral("TemporaryFolder"));
    if (misc == nullptr) {
        misc = new QTreeWidgetItem(this, QStringList(QStringLiteral("TemporaryFolder")));
        misc->setData(0, TypeRole, QString::number((int) EffectsList::EFFECT_FOLDER));
        misc->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    } else {
        qDeleteAll(misc->takeChildren());
    }
    setIndentation(0);
    misc->addChildren(list);
    for (int j = 0; j < misc->childCount(); ++j) {
        QTreeWidgetItem *child = misc->child(j);
        child->setHidden(false);
        child->setData(0, Qt::UserRole, effectType);
    }
    setRootIndex(indexFromItem(misc));
}

void EffectsListWidget::resetFavorites()
{
    QTreeWidgetItem *misc = findFolder(QStringLiteral("TemporaryFolder"));
    if (misc) {
        setRootIndex(indexFromItem(invisibleRootItem()));
        delete misc;
    }
}

