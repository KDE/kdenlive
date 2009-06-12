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

#include "projectlist.h"
#include "projectitem.h"
#include "addfoldercommand.h"
#include "kdenlivesettings.h"
#include "slideshowclip.h"
#include "ui_colorclip_ui.h"
#include "titlewidget.h"
#include "definitions.h"
#include "clipmanager.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "kthumb.h"
#include "projectlistview.h"
#include "editclipcommand.h"
#include "editfoldercommand.h"


#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <KInputDialog>
#include <KMessageBox>

#include <nepomuk/global.h>
#include <nepomuk/resourcemanager.h>
//#include <nepomuk/tag.h>

#include <QMouseEvent>
#include <QStylePainter>
#include <QPixmap>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QHeaderView>

ProjectList::ProjectList(QWidget *parent) :
        QWidget(parent),
        m_render(NULL),
        m_fps(-1),
        m_commandStack(NULL),
        m_editAction(NULL),
        m_deleteAction(NULL),
        m_openAction(NULL),
        m_reloadAction(NULL),
        m_selectedItem(NULL),
        m_refreshed(false),
        m_infoQueue(),
        m_thumbnailQueue()
{

    m_listView = new ProjectListView(this);;
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);

    // setup toolbar
    KTreeWidgetSearchLine *searchView = new KTreeWidgetSearchLine(this);
    m_toolbar = new QToolBar("projectToolBar", this);
    m_toolbar->addWidget(searchView);
    searchView->setTreeWidget(m_listView);

    m_addButton = new QToolButton(m_toolbar);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(m_addButton);

    layout->addWidget(m_toolbar);
    layout->addWidget(m_listView);
    setLayout(layout);



    connect(m_listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(focusMonitor()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(pauseMonitor()), this, SLOT(slotPauseMonitor()));
    connect(m_listView, SIGNAL(requestMenu(const QPoint &, QTreeWidgetItem *)), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
    connect(m_listView, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(m_listView, SIGNAL(addClip(const QList <QUrl>, const QString &)), this, SLOT(slotAddClip(const QList <QUrl>, const QString &)));
    connect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));
    connect(m_listView, SIGNAL(showProperties(DocClipBase *)), this, SIGNAL(showClipProperties(DocClipBase *)));

    ItemDelegate *listViewDelegate = new ItemDelegate(m_listView);
    m_listView->setItemDelegate(listViewDelegate);

    if (KdenliveSettings::activate_nepomuk()) {
        Nepomuk::ResourceManager::instance()->init();
        if (!Nepomuk::ResourceManager::instance()->initialized()) {
            kDebug() << "Cannot communicate with Nepomuk, DISABLING it";
            KdenliveSettings::setActivate_nepomuk(false);
        }
    }
}

ProjectList::~ProjectList()
{
    delete m_menu;
    delete m_toolbar;
}

void ProjectList::setupMenu(QMenu *addMenu, QAction *defaultAction)
{
    QList <QAction *> actions = addMenu->actions();
    for (int i = 0; i < actions.count(); i++) {
        if (actions.at(i)->data().toString() == "clip_properties") {
            m_editAction = actions.at(i);
            m_toolbar->addAction(m_editAction);
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "delete_clip") {
            m_deleteAction = actions.at(i);
            m_toolbar->addAction(m_deleteAction);
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "edit_clip") {
            m_openAction = actions.at(i);
            actions.removeAt(i);
            i--;
        } else if (actions.at(i)->data().toString() == "reload_clip") {
            m_reloadAction = actions.at(i);
            actions.removeAt(i);
            i--;
        }
    }

    QMenu *m = new QMenu();
    m->addActions(actions);
    m_addButton->setMenu(m);
    m_addButton->setDefaultAction(defaultAction);
    m_menu = new QMenu();
    m_menu->addActions(addMenu->actions());
}

void ProjectList::setupGeneratorMenu(QMenu *addMenu, QMenu *transcodeMenu)
{
    if (!addMenu) return;
    QMenu *menu = m_addButton->menu();
    menu->addMenu(addMenu);
    m_addButton->setMenu(menu);

    m_menu->addMenu(addMenu);
    if (addMenu->isEmpty()) addMenu->setEnabled(false);
    m_menu->addMenu(transcodeMenu);
    if (transcodeMenu->isEmpty()) transcodeMenu->setEnabled(false);
    m_menu->addAction(m_reloadAction);
    m_menu->addAction(m_editAction);
    m_menu->addAction(m_openAction);
    m_menu->addAction(m_deleteAction);
    m_menu->insertSeparator(m_deleteAction);
}


QByteArray ProjectList::headerInfo() const
{
    return m_listView->header()->saveState();
}

void ProjectList::setHeaderInfo(const QByteArray &state)
{
    m_listView->header()->restoreState(state);
}

void ProjectList::slotEditClip()
{
    ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (!(item->flags() & Qt::ItemIsDragEnabled)) return;
    if (item && !item->isGroup()) {
        emit clipSelected(item->referencedClip());
        emit showClipProperties(item->referencedClip());
    }
}

void ProjectList::slotOpenClip()
{
    ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item && !item->isGroup()) {
        if (item->clipType() == IMAGE) {
            if (KdenliveSettings::defaultimageapp().isEmpty()) KMessageBox::sorry(this, i18n("Please set a default application to open images in the Settings dialog"));
            else QProcess::startDetached(KdenliveSettings::defaultimageapp(), QStringList() << item->clipUrl().path());
        }
        if (item->clipType() == AUDIO) {
            if (KdenliveSettings::defaultaudioapp().isEmpty()) KMessageBox::sorry(this, i18n("Please set a default application to open audio files in the Settings dialog"));
            else QProcess::startDetached(KdenliveSettings::defaultaudioapp(), QStringList() << item->clipUrl().path());
        }
    }
}

void ProjectList::slotReloadClip()
{
    ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item && !item->isGroup()) {
        if (item->clipType() == IMAGE) {
            item->referencedClip()->producer()->set("force_reload", 1);
        }
        emit getFileProperties(item->toXml(), item->clipId(), false);
    }
}

void ProjectList::setRenderer(Render *projectRender)
{
    m_render = projectRender;
    m_listView->setIconSize(QSize(40 * m_render->dar(), 40));
}

void ProjectList::slotClipSelected()
{
    if (m_listView->currentItem()) {
        ProjectItem *clip = static_cast <ProjectItem*>(m_listView->currentItem());
        if (!clip->isGroup()) {
            m_selectedItem = clip;
            emit clipSelected(clip->referencedClip());
        }
        m_editAction->setEnabled(true);
        m_deleteAction->setEnabled(true);
        m_reloadAction->setEnabled(true);
        if (clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else m_openAction->setEnabled(false);
    } else {
        emit clipSelected(NULL);
        m_editAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        m_openAction->setEnabled(false);
        m_reloadAction->setEnabled(false);
    }
}

void ProjectList::slotPauseMonitor()
{
    if (m_render) m_render->pause();
}

void ProjectList::slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        slotUpdateClipProperties(item, properties);
        if (properties.contains("colour") || properties.contains("resource") || properties.contains("xmldata") || properties.contains("force_aspect_ratio")) slotRefreshClipThumbnail(item);
        if (properties.contains("out")) item->changeDuration(properties.value("out").toInt());
    }
}

void ProjectList::slotUpdateClipProperties(ProjectItem *clip, QMap <QString, QString> properties)
{
    if (!clip) return;
    if (!clip->isGroup()) clip->setProperties(properties);
    if (properties.contains("name")) {
        m_listView->blockSignals(true);
        clip->setText(1, properties.value("name"));
        m_listView->blockSignals(false);
        emit clipNameChanged(clip->clipId(), properties.value("name"));
    }
    if (properties.contains("description")) {
        CLIPTYPE type = clip->clipType();
        m_listView->blockSignals(true);
        clip->setText(2, properties.value("description"));
        m_listView->blockSignals(false);
        if (KdenliveSettings::activate_nepomuk() && (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST)) {
            // Use Nepomuk system to store clip description
            Nepomuk::Resource f(clip->clipUrl().path());
            f.setDescription(properties.value("description"));
        }
        emit projectModified();
    }
}

void ProjectList::slotItemEdited(QTreeWidgetItem *item, int column)
{
    ProjectItem *clip = static_cast <ProjectItem*>(item);
    if (column == 2) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["description"] = clip->referencedClip()->getProperty("description");
            newprops["description"] = item->text(2);
            slotUpdateClipProperties(clip, newprops);
            EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
            m_commandStack->push(command);
        }
    } else if (column == 1) {
        if (clip->isGroup()) {
            editFolder(item->text(1), clip->groupName(), clip->clipId());
            clip->setGroupName(item->text(1));
            m_doc->clipManager()->addFolder(clip->clipId(), item->text(1));
            const int children = item->childCount();
            for (int i = 0; i < children; i++) {
                ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                child->setProperty("groupname", item->text(1));
            }
        } else {
            if (clip->referencedClip()) {
                QMap <QString, QString> oldprops;
                QMap <QString, QString> newprops;
                oldprops["name"] = clip->referencedClip()->getProperty("name");
                newprops["name"] = item->text(1);
                slotUpdateClipProperties(clip, newprops);
                emit projectModified();
                EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
                m_commandStack->push(command);
            }
        }
    }
}

void ProjectList::slotContextMenu(const QPoint &pos, QTreeWidgetItem *item)
{
    bool enable = false;
    if (item) {
        enable = true;
    }
    m_editAction->setEnabled(enable);
    m_deleteAction->setEnabled(enable);
    m_reloadAction->setEnabled(enable);
    if (enable) {
        ProjectItem *clip = static_cast <ProjectItem*>(item);
        if (clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else m_openAction->setEnabled(false);
    } else m_openAction->setEnabled(false);
    m_menu->popup(pos);
}

void ProjectList::slotRemoveClip()
{
    if (!m_listView->currentItem()) return;
    QList <QString> ids;
    QMap <QString, QString> folderids;
    QList<QTreeWidgetItem *> selected = m_listView->selectedItems();
    ProjectItem *item;
    for (int i = 0; i < selected.count(); i++) {
        item = static_cast <ProjectItem *>(selected.at(i));
        if (item->isGroup()) folderids[item->groupName()] = item->clipId();
        else ids << item->clipId();
        if (item->numReferences() > 0) {
            if (KMessageBox::questionYesNo(this, i18np("Delete clip <b>%2</b>?<br>This will also remove the clip in timeline", "Delete clip <b>%2</b>?<br>This will also remove its %1 clips in timeline", item->numReferences(), item->names().at(1)), i18n("Delete Clip")) != KMessageBox::Yes) return;
        } else if (item->isGroup() && item->childCount() > 0) {
            int children = item->childCount();
            if (KMessageBox::questionYesNo(this, i18n("Delete folder <b>%2</b>?<br>This will also remove the %1 clips in that folder", children, item->names().at(1)), i18n("Delete Folder")) != KMessageBox::Yes) return;
            for (int i = 0; i < children; ++i) {
                ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                ids << child->clipId();
            }
        }
    }
    if (!ids.isEmpty()) m_doc->deleteProjectClip(ids);
    if (!folderids.isEmpty()) deleteProjectFolder(folderids);
    if (m_listView->topLevelItemCount() == 0) {
        m_editAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        m_openAction->setEnabled(false);
        m_reloadAction->setEnabled(false);
    }
}

void ProjectList::selectItemById(const QString &clipId)
{
    ProjectItem *item = getItemById(clipId);
    if (item) m_listView->setCurrentItem(item);
}


void ProjectList::slotDeleteClip(const QString &clipId)
{
    ProjectItem *item = getItemById(clipId);
    if (!item) {
        kDebug() << "/// Cannot find clip to delete";
        return;
    }
    delete item;
}


void ProjectList::editFolder(const QString folderName, const QString oldfolderName, const QString &clipId)
{
    EditFolderCommand *command = new EditFolderCommand(this, folderName, oldfolderName, clipId, false);
    m_commandStack->push(command);
    m_doc->setModified(true);
}

void ProjectList::slotAddFolder()
{
    AddFolderCommand *command = new AddFolderCommand(this, i18n("Folder"), QString::number(m_doc->clipManager()->getFreeFolderId()), true);
    m_commandStack->push(command);
}

void ProjectList::slotAddFolder(const QString foldername, const QString &clipId, bool remove, bool edit)
{
    if (remove) {
        ProjectItem *item = getFolderItemById(clipId);
        if (item) {
            m_doc->clipManager()->deleteFolder(clipId);
            delete item;
        }
    } else {
        if (edit) {
            ProjectItem *item = getFolderItemById(clipId);
            QTreeWidgetItemIterator it(m_listView);
            if (item) {
                m_listView->blockSignals(true);
                item->setGroupName(foldername);
                m_listView->blockSignals(false);
                m_doc->clipManager()->addFolder(clipId, foldername);
                const int children = item->childCount();
                for (int i = 0; i < children; i++) {
                    ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                    child->setProperty("groupname", foldername);
                }
            }
        } else {
            QStringList text;
            text << QString() << foldername;
            m_listView->blockSignals(true);
            (void) new ProjectItem(m_listView, text, clipId);
            m_doc->clipManager()->addFolder(clipId, foldername);
            m_listView->blockSignals(false);
        }
    }
}



void ProjectList::deleteProjectFolder(QMap <QString, QString> map)
{
    QMapIterator<QString, QString> i(map);
    QUndoCommand *delCommand = new QUndoCommand();
    delCommand->setText(i18n("Delete Folder"));
    while (i.hasNext()) {
        i.next();
        new AddFolderCommand(this, i.key(), i.value(), false, delCommand);
    }
    m_commandStack->push(delCommand);
    m_doc->setModified(true);
}

void ProjectList::slotAddClip(DocClipBase *clip, bool getProperties)
{
    if (getProperties) m_listView->setEnabled(false);
    m_listView->blockSignals(true);
    const QString parent = clip->getProperty("groupid");
    kDebug() << "Adding clip with groupid: " << parent;
    ProjectItem *item = NULL;
    if (!parent.isEmpty()) {
        ProjectItem *parentitem = getFolderItemById(parent);
        if (!parentitem) {
            QStringList text;
            QString groupName = clip->getProperty("groupname");
            //kDebug() << "Adding clip to new group: " << groupName;
            if (groupName.isEmpty()) groupName = i18n("Folder");
            text << QString() << groupName;
            parentitem = new ProjectItem(m_listView, text, parent);
        } else {
            //kDebug() << "Adding clip to existing group: " << parentitem->groupName();
        }
        if (parentitem) item = new ProjectItem(parentitem, clip);
    }
    if (item == NULL) item = new ProjectItem(m_listView, clip);

    KUrl url = clip->fileURL();
    if (!url.isEmpty() && KdenliveSettings::activate_nepomuk()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation = f.description();
        if (!annotation.isEmpty()) item->setText(2, annotation);
        item->setText(3, QString::number(f.rating()));
    }
    m_listView->blockSignals(false);
}

void ProjectList::slotResetProjectList()
{
    m_listView->clear();
    emit clipSelected(NULL);
    m_thumbnailQueue.clear();
    m_infoQueue.clear();
    m_refreshed = false;
}

void ProjectList::requestClipInfo(const QDomElement xml, const QString id)
{
    kDebug() << " PRG LIST REQUEST CLP INFO: " << id;
    m_infoQueue.insert(id, xml);
    m_listView->setEnabled(false);
    if (m_infoQueue.count() == 1) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotProcessNextClipInQueue()
{
    if (m_infoQueue.isEmpty()) {
        m_listView->setEnabled(true);
        return;
    }
    QMap<QString, QDomElement>::const_iterator i = m_infoQueue.constBegin();
    if (i != m_infoQueue.constEnd()) {
        const QDomElement dom = i.value();
        const QString id = i.key();
        m_infoQueue.remove(i.key());
        emit getFileProperties(dom, id, true);
    }
    if (m_infoQueue.isEmpty()) m_listView->setEnabled(true);
}

void ProjectList::slotUpdateClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    m_listView->blockSignals(true);
    if (item) item->setData(1, UsageRole, QString::number(item->numReferences()));
    m_listView->blockSignals(false);
}

void ProjectList::updateAllClips()
{
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        ProjectItem *item = static_cast <ProjectItem *>(*it);
        if (!item->isGroup()) {
            if (item->referencedClip()->producer() == NULL) {
                DocClipBase *clip = item->referencedClip();
                if (clip->clipType() == TEXT && !QFile::exists(clip->fileURL().path())) {
                    // regenerate text clip image if required
                    TitleWidget *dia_ui = new TitleWidget(KUrl(), QString(), m_render, this);
                    QDomDocument doc;
                    doc.setContent(clip->getProperty("xmldata"));
                    dia_ui->setXml(doc);
                    QImage pix = dia_ui->renderedPixmap();
                    pix.save(clip->fileURL().path());
                    delete dia_ui;
                }
                if (clip->isPlaceHolder() == false) requestClipInfo(clip->toXML(), clip->getId());
                else item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            } else {
                QString cachedPixmap = m_doc->projectFolder().path() + "/thumbs/" + item->getClipHash() + ".png";
                if (QFile::exists(cachedPixmap)) {
                    //kDebug()<<"// USING CACHED PIX: "<<cachedPixmap;
                    m_listView->blockSignals(true);
                    item->setIcon(0, QPixmap(cachedPixmap));
                    m_listView->blockSignals(false);
                } else requestClipThumbnail(item->clipId());

                if (item->data(1, DurationRole).toString().isEmpty()) {
                    m_listView->blockSignals(true);
                    item->changeDuration(item->referencedClip()->producer()->get_playtime());
                    m_listView->blockSignals(false);
                }
            }
            m_listView->blockSignals(true);
            item->setData(1, UsageRole, QString::number(item->numReferences()));
            m_listView->blockSignals(false);
            qApp->processEvents();
        }
        ++it;
    }
    QTimer::singleShot(500, this, SLOT(slotCheckForEmptyQueue()));
}

void ProjectList::slotAddClip(const QList <QUrl> givenList, QString group)
{
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    KUrl::List list;
    if (givenList.isEmpty()) {
        // Build list of mime types
        QStringList mimeTypes = QStringList() << "application/x-kdenlive" << "video/x-flv" << "application/vnd.rn-realmedia" << "video/x-dv" << "video/dv" << "video/x-msvideo" << "video/mpeg" << "video/x-ms-wmv" << "audio/mpeg" << "audio/x-mp3" << "audio/x-wav" << "application/ogg" << "video/mp4" << "video/quicktime" << "image/gif" << "image/jpeg" << "image/png" << "image/x-tga" << "image/x-bmp" << "image/svg+xml" << "image/tiff" << "image/x-xcf-gimp" << "image/x-vnd.adobe.photoshop" << "image/x-pcx" << "image/x-exr" << "video/mlt-playlist" << "audio/x-flac" << "audio/mp4" << "video/x-matroska" << "audio/x-matroska" << "video/ogg" << "audio/ogg";

        QString allExtensions;
        foreach(const QString& mimeType, mimeTypes) {
            KMimeType::Ptr mime(KMimeType::mimeType(mimeType));
            if (mime) {
                allExtensions.append(mime->patterns().join(" "));
                allExtensions.append(' ');
            }
        }
        QString dialogFilter = allExtensions + ' ' + QLatin1Char('|') + i18n("All Supported Files");
        dialogFilter.append("\n*" + QLatin1Char('|') + i18n("All Files"));
        list = KFileDialog::getOpenUrls(KUrl("kfiledialog:///clipfolder"), dialogFilter, this);

    } else {
        for (int i = 0; i < givenList.count(); i++)
            list << givenList.at(i);
    }
    if (list.isEmpty()) return;

    QString groupId;
    if (group.isEmpty()) {
        ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
        if (item && !item->isGroup()) {
            while (item->parent()) {
                item = static_cast <ProjectItem*>(item->parent());
                if (item->isGroup()) break;
            }
        }
        if (item && item->isGroup()) {
            group = item->groupName();
            groupId = item->clipId();
        }
    }
    m_doc->slotAddClipList(list, group, groupId);
}

void ProjectList::slotRemoveInvalidClip(const QString &id, bool replace)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        const QString path = item->referencedClip()->fileURL().path();
        if (!path.isEmpty()) {
            if (replace) KMessageBox::sorry(this, i18n("Clip <b>%1</b><br>is invalid, will be removed from project.", path));
            else {
                if (KMessageBox::questionYesNo(this, i18n("Clip <b>%1</b><br>is missing or invalid. Remove it from project?", path), i18n("Invalid clip")) == KMessageBox::Yes) replace = true;
            }
        }
        QList <QString> ids;
        ids << id;
        if (replace) m_doc->deleteProjectClip(ids);
    }
    if (!m_infoQueue.isEmpty()) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
    else m_listView->setEnabled(true);
}

void ProjectList::slotAddColorClip()
{
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    QDialog *dia = new QDialog(this);
    Ui::ColorClip_UI *dia_ui = new Ui::ColorClip_UI();
    dia_ui->setupUi(dia);
    dia_ui->clip_name->setText(i18n("Color Clip"));
    dia_ui->clip_duration->setText(KdenliveSettings::color_duration());
    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui->clip_color->color().name();
        color = color.replace(0, 1, "0x") + "ff";

        QString group;
        QString groupId;
        ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
        if (item && !item->isGroup()) {
            while (item->parent()) {
                item = static_cast <ProjectItem*>(item->parent());
                if (item->isGroup()) break;
            }
        }
        if (item && item->isGroup()) {
            group = item->groupName();
            groupId = item->clipId();
        }

        m_doc->clipManager()->slotAddColorClipFile(dia_ui->clip_name->text(), color, dia_ui->clip_duration->text(), group, groupId);
        m_doc->setModified(true);
    }
    delete dia_ui;
    delete dia;
}


void ProjectList::slotAddSlideshowClip()
{
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    SlideshowClip *dia = new SlideshowClip(m_timecode, this);

    if (dia->exec() == QDialog::Accepted) {

        QString group;
        QString groupId;
        ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
        if (item && !item->isGroup()) {
            while (item->parent()) {
                item = static_cast <ProjectItem*>(item->parent());
                if (item->isGroup()) break;
            }
        }
        if (item && item->isGroup()) {
            group = item->groupName();
            groupId = item->clipId();
        }

        m_doc->clipManager()->slotAddSlideshowClipFile(dia->clipName(), dia->selectedPath(), dia->imageCount(), dia->clipDuration(), dia->loop(), dia->fade(), dia->lumaDuration(), dia->lumaFile(), dia->softness(), group, groupId);
        m_doc->setModified(true);
    }
    delete dia;
}

void ProjectList::slotAddTitleClip()
{
    QString group;
    QString groupId;
    ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item && !item->isGroup()) {
        while (item->parent()) {
            item = static_cast <ProjectItem*>(item->parent());
            if (item->isGroup()) break;
        }
    }
    if (item && item->isGroup()) {
        group = item->groupName();
        groupId = item->clipId();
    }

    m_doc->slotCreateTextClip(group, groupId);
}

void ProjectList::setDocument(KdenliveDoc *doc)
{
    m_listView->blockSignals(true);
    m_listView->clear();
    emit clipSelected(NULL);
    m_thumbnailQueue.clear();
    m_infoQueue.clear();
    m_refreshed = false;
    QMap <QString, QString> flist = doc->clipManager()->documentFolderList();
    QMapIterator<QString, QString> f(flist);
    while (f.hasNext()) {
        f.next();
        (void) new ProjectItem(m_listView, QStringList() << QString() << f.value(), f.key());
    }

    QList <DocClipBase*> list = doc->clipManager()->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        slotAddClip(list.at(i), false);
    }

    m_fps = doc->fps();
    m_timecode = doc->timecode();
    m_commandStack = doc->commandStack();
    m_doc = doc;
    QTreeWidgetItem *first = m_listView->topLevelItem(0);
    if (first) m_listView->setCurrentItem(first);
    m_listView->blockSignals(false);
    m_toolbar->setEnabled(true);
}

QDomElement ProjectList::producersList()
{
    QDomDocument doc;
    QDomElement prods = doc.createElement("producerlist");
    doc.appendChild(prods);
    kDebug() << "////////////  PRO LIST BUILD PRDSLIST ";
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if (!((ProjectItem *)(*it))->isGroup())
            prods.appendChild(doc.importNode(((ProjectItem *)(*it))->toXml(), true));
        ++it;
    }
    return prods;
}

void ProjectList::slotCheckForEmptyQueue()
{
    if (!m_refreshed && m_thumbnailQueue.isEmpty() && m_infoQueue.isEmpty()) {
        m_refreshed = true;
        emit loadingIsOver();
    } else QTimer::singleShot(500, this, SLOT(slotCheckForEmptyQueue()));
}

void ProjectList::reloadClipThumbnails()
{
    m_thumbnailQueue.clear();
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if (!((ProjectItem *)(*it))->isGroup())
            m_thumbnailQueue << ((ProjectItem *)(*it))->clipId();
        ++it;
    }
    QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
}

void ProjectList::requestClipThumbnail(const QString &id)
{
    m_thumbnailQueue.append(id);
    if (m_thumbnailQueue.count() == 1) QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
}

void ProjectList::slotProcessNextThumbnail()
{
    if (m_thumbnailQueue.isEmpty()) {
        return;
    }
    slotRefreshClipThumbnail(m_thumbnailQueue.takeFirst(), false);
}

void ProjectList::slotRefreshClipThumbnail(const QString &clipId, bool update)
{
    ProjectItem *item = getItemById(clipId);
    if (item) slotRefreshClipThumbnail(item, update);
    else slotProcessNextThumbnail();
}

void ProjectList::slotRefreshClipThumbnail(ProjectItem *item, bool update)
{
    if (item) {
        if (!item->referencedClip()) return;
        int height = 50;
        int width = (int)(height  * m_render->dar());
        DocClipBase *clip = item->referencedClip();
        if (!clip) slotProcessNextThumbnail();
        QPixmap pix;
        if (clip->clipType() == AUDIO) pix = KIcon("audio-x-generic").pixmap(QSize(width, height));
        else pix = item->referencedClip()->thumbProducer()->extractImage(item->referencedClip()->getClipThumbFrame(), width, height);
        m_listView->blockSignals(true);
        item->setIcon(0, pix);
        m_listView->blockSignals(false);
        m_doc->cachePixmap(item->getClipHash(), pix);
        if (update) emit projectModified();
        if (!m_thumbnailQueue.isEmpty()) QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
    }
}

void ProjectList::slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata, bool replace)
{
    ProjectItem *item = getItemById(clipId);
    if (item && producer) {
        m_listView->blockSignals(true);
        item->setProperties(properties, metadata);
        Q_ASSERT_X(item->referencedClip(), "void ProjectList::slotReplyGetFileProperties", QString("Item with groupName %1 does not have a clip associated").arg(item->groupName()).toLatin1());
        if (replace) item->referencedClip()->setProducer(producer);
        else {
            // Check if duration changed.
            emit receivedClipDuration(clipId);
            delete producer;
        }
        m_listView->blockSignals(false);
    } else kDebug() << "////////  COULD NOT FIND CLIP TO UPDATE PRPS...";
    if (!m_infoQueue.isEmpty()) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
    else m_listView->setEnabled(true);
}

void ProjectList::slotReplyGetImage(const QString &clipId, const QPixmap &pix)
{
    ProjectItem *item = getItemById(clipId);
    if (item) {
        m_listView->blockSignals(true);
        item->setIcon(0, pix);
        m_doc->cachePixmap(item->getClipHash(), pix);
        m_listView->blockSignals(false);
    }
}

ProjectItem *ProjectList::getItemById(const QString &id)
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == id && item->clipType() != FOLDER)
            return item;
        ++it;
    }
    return NULL;
}

ProjectItem *ProjectList::getFolderItemById(const QString &id)
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == id && item->clipType() == FOLDER)
            return item;
        ++it;
    }
    return NULL;
}

void ProjectList::slotSelectClip(const QString &ix)
{
    ProjectItem *clip = getItemById(ix);
    if (clip) {
        m_listView->setCurrentItem(clip);
        m_listView->scrollToItem(clip);
        m_editAction->setEnabled(true);
        m_deleteAction->setEnabled(true);
        m_reloadAction->setEnabled(true);
        if (clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else m_openAction->setEnabled(false);
    }
}

QString ProjectList::currentClipUrl() const
{
    ProjectItem *item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item == NULL) return QString();
    return item->clipUrl().path();
}

#include "projectlist.moc"
