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
#include "editclipcutcommand.h"
#include "editfoldercommand.h"
#include "addclipcutcommand.h"

#include "ui_templateclip_ui.h"

#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <KInputDialog>
#include <KMessageBox>
#include <KIO/NetAccess>
#include <KFileItem>
#ifdef NEPOMUK
#include <nepomuk/global.h>
#include <nepomuk/resourcemanager.h>
//#include <nepomuk/tag.h>
#endif

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
        m_transcodeAction(NULL),
        m_doc(NULL),
        m_refreshed(false),
        m_infoQueue(),
        m_thumbnailQueue()
{

    m_listView = new ProjectListView(this);;
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // setup toolbar
    KTreeWidgetSearchLine *searchView = new KTreeWidgetSearchLine(this);
    m_toolbar = new QToolBar("projectToolBar", this);
    m_toolbar->addWidget(searchView);
    int s = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolbar->setIconSize(QSize(s, s));
    searchView->setTreeWidget(m_listView);

    m_addButton = new QToolButton(m_toolbar);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(m_addButton);

    layout->addWidget(m_toolbar);
    layout->addWidget(m_listView);
    setLayout(layout);

    m_queueTimer.setInterval(100);
    connect(&m_queueTimer, SIGNAL(timeout()), this, SLOT(slotProcessNextClipInQueue()));
    m_queueTimer.setSingleShot(true);


    connect(m_listView, SIGNAL(projectModified()), this, SIGNAL(projectModified()));
    connect(m_listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(focusMonitor()), this, SLOT(slotClipSelected()));
    connect(m_listView, SIGNAL(pauseMonitor()), this, SLOT(slotPauseMonitor()));
    connect(m_listView, SIGNAL(requestMenu(const QPoint &, QTreeWidgetItem *)), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
    connect(m_listView, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(m_listView, SIGNAL(addClip(const QList <QUrl>, const QString &, const QString &)), this, SLOT(slotAddClip(const QList <QUrl>, const QString &, const QString &)));
    connect(m_listView, SIGNAL(addClipCut(const QString &, int, int)), this, SLOT(slotAddClipCut(const QString &, int, int)));
    connect(m_listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));
    connect(m_listView, SIGNAL(showProperties(DocClipBase *)), this, SIGNAL(showClipProperties(DocClipBase *)));

    m_listViewDelegate = new ItemDelegate(m_listView);
    m_listView->setItemDelegate(m_listViewDelegate);
#ifdef NEPOMUK
    if (KdenliveSettings::activate_nepomuk()) {
        Nepomuk::ResourceManager::instance()->init();
        if (!Nepomuk::ResourceManager::instance()->initialized()) {
            kDebug() << "Cannot communicate with Nepomuk, DISABLING it";
            KdenliveSettings::setActivate_nepomuk(false);
        }
    }
#endif
}

ProjectList::~ProjectList()
{
    delete m_menu;
    delete m_toolbar;
    m_listView->blockSignals(true);
    m_listView->clear();
    delete m_listViewDelegate;
}

void ProjectList::focusTree() const
{
    m_listView->setFocus();
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

void ProjectList::setupGeneratorMenu(QMenu *addMenu, QMenu *transcodeMenu, QMenu *inTimelineMenu)
{
    if (!addMenu) return;
    QMenu *menu = m_addButton->menu();
    menu->addMenu(addMenu);
    m_addButton->setMenu(menu);

    m_menu->addMenu(addMenu);
    if (addMenu->isEmpty()) addMenu->setEnabled(false);
    m_menu->addMenu(transcodeMenu);
    if (transcodeMenu->isEmpty()) transcodeMenu->setEnabled(false);
    m_transcodeAction = transcodeMenu;
    m_menu->addAction(m_reloadAction);
    m_menu->addMenu(inTimelineMenu);
    inTimelineMenu->setEnabled(false);
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

void ProjectList::updateProjectFormat(Timecode t)
{
    m_timecode = t;
}

void ProjectList::slotEditClip()
{
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    if (list.count() > 1) {
        editClipSelection(list);
        return;
    }
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE) return;
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    } else item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (!(item->flags() & Qt::ItemIsDragEnabled)) return;
    if (item) {
        emit clipSelected(item->referencedClip());
        emit showClipProperties(item->referencedClip());
    }
}

void ProjectList::editClipSelection(QList<QTreeWidgetItem *> list)
{
    // Gather all common properties
    QMap <QString, QString> commonproperties;
    QList <DocClipBase *> clipList;
    commonproperties.insert("force_aspect_ratio", "-");
    commonproperties.insert("force_fps", "-");
    commonproperties.insert("force_progressive", "-");
    commonproperties.insert("threads", "-");
    commonproperties.insert("video_index", "-");
    commonproperties.insert("audio_index", "-");

    bool allowDurationChange = true;
    int commonDuration = -1;
    ProjectItem *item;
    for (int i = 0; i < list.count(); i++) {
        item = NULL;
        if (list.at(i)->type() == PROJECTFOLDERTYPE) continue;
        if (list.at(i)->type() == PROJECTSUBCLIPTYPE) {
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        } else item = static_cast <ProjectItem*>(list.at(i));
        if (!(item->flags() & Qt::ItemIsDragEnabled)) continue;
        if (item) {
            // check properties
            DocClipBase *clip = item->referencedClip();
            if (clipList.contains(clip)) continue;
            if (clip->clipType() != COLOR && clip->clipType() != IMAGE && clip->clipType() != TEXT) {
                allowDurationChange = false;
            }
            if (allowDurationChange && commonDuration != 0) {
                if (commonDuration == -1) {
                    commonDuration = clip->duration().frames(m_fps);
                } else if (commonDuration != clip->duration().frames(m_fps)) {
                    commonDuration = 0;
                }
            }
            clipList.append(clip);
            QMap <QString, QString> clipprops = clip->properties();
            QMapIterator<QString, QString> p(commonproperties);
            while (p.hasNext()) {
                p.next();
                if (p.value().isEmpty()) continue;
                if (clipprops.contains(p.key())) {
                    if (p.value() == "-") commonproperties.insert(p.key(), clipprops.value(p.key()));
                    else if (p.value() != clipprops.value(p.key())) commonproperties.insert(p.key(), QString());
                } else commonproperties.insert(p.key(), QString());
            }
        }
    }
    if (allowDurationChange) commonproperties.insert("out", QString::number(commonDuration));
    QMapIterator<QString, QString> p(commonproperties);
    while (p.hasNext()) {
        p.next();
        kDebug() << "Result: " << p.key() << " = " << p.value();
    }
    emit showClipProperties(clipList, commonproperties);
}

void ProjectList::slotOpenClip()
{
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE) return;
    if (m_listView->currentItem()->type() == QTreeWidgetItem::UserType + 1) {
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    } else item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item) {
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

void ProjectList::cleanup()
{
    m_listView->clearSelection();
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            it++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it);
        if (item->numReferences() == 0) item->setSelected(true);
        it++;
    }
    slotRemoveClip();
}

void ProjectList::trashUnusedClips()
{
    QTreeWidgetItemIterator it(m_listView);
    ProjectItem *item;
    QStringList ids;
    QStringList urls;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            it++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it);
        if (item->numReferences() == 0) {
            ids << item->clipId();
            KUrl url = item->clipUrl();
            if (!url.isEmpty() && !urls.contains(url.path())) urls << url.path();
        }
        it++;
    }

    // Check that we don't use the URL in another clip
    QTreeWidgetItemIterator it2(m_listView);
    while (*it2) {
        if ((*it2)->type() != PROJECTCLIPTYPE) {
            it2++;
            continue;
        }
        item = static_cast <ProjectItem *>(*it2);
        if (item->numReferences() > 0) {
            KUrl url = item->clipUrl();
            if (!url.isEmpty() && urls.contains(url.path())) urls.removeAll(url.path());
        }
        it2++;
    }

    emit deleteProjectClips(ids, QMap <QString, QString>());
    for (int i = 0; i < urls.count(); i++) {
        KIO::NetAccess::del(KUrl(urls.at(i)), this);
    }
}

void ProjectList::slotReloadClip(const QString &id)
{
    QList<QTreeWidgetItem *> selected;
    if (id.isEmpty()) selected = m_listView->selectedItems();
    else selected.append(getItemById(id));
    ProjectItem *item;
    for (int i = 0; i < selected.count(); i++) {
        if (selected.at(i)->type() != PROJECTCLIPTYPE) {
            if (selected.at(i)->type() == PROJECTFOLDERTYPE) {
                for (int j = 0; j < selected.at(i)->childCount(); j++)
                    selected.append(selected.at(i)->child(j));
            }
            continue;
        }
        item = static_cast <ProjectItem *>(selected.at(i));
        if (item) {
            if (item->clipType() == TEXT) {
                if (!item->referencedClip()->getProperty("xmltemplate").isEmpty()) regenerateTemplate(item);
            } else if (item->clipType() != COLOR && item->clipType() != SLIDESHOW && item->referencedClip() &&  item->referencedClip()->checkHash() == false) {
                item->referencedClip()->setPlaceHolder(true);
                item->setProperty("file_hash", QString());
            } else if (item->clipType() == IMAGE) {
                item->referencedClip()->producer()->set("force_reload", 1);
            }
            //requestClipInfo(item->toXml(), item->clipId(), true);
            // Clear the file_hash value, which will cause a complete reload of the clip
            emit getFileProperties(item->toXml(), item->clipId(), m_listView->iconSize().height(), true);
        }
    }
}

void ProjectList::slotMissingClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item) {
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        if (item->referencedClip()) {
            item->referencedClip()->setPlaceHolder(true);
            if (m_render == NULL) kDebug() << "*********  ERROR, NULL RENDR";
            item->referencedClip()->setProducer(m_render->invalidProducer(id), true);
            item->slotSetToolTip();
            emit clipNeedsReload(id, true);
        }
    }
    update();
    emit displayMessage(i18n("Check missing clips"), -2);
    emit updateRenderStatus();
}

void ProjectList::slotAvailableClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    if (item == NULL) return;
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    if (item->referencedClip()) { // && item->referencedClip()->checkHash() == false) {
        item->setProperty("file_hash", QString());
        slotReloadClip(id);
    }
    /*else {
    item->referencedClip()->setValid();
    item->slotSetToolTip();
    }
    update();*/
    emit updateRenderStatus();
}

bool ProjectList::hasMissingClips()
{
    bool missing = false;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() == PROJECTCLIPTYPE && !((*it)->flags() & Qt::ItemIsDragEnabled)) {
            missing = true;
            break;
        }
        it++;
    }
    return missing;
}

void ProjectList::setRenderer(Render *projectRender)
{
    m_render = projectRender;
    m_listView->setIconSize(QSize((ProjectItem::itemDefaultHeight() - 2) * m_render->dar(), ProjectItem::itemDefaultHeight() - 2));
}

void ProjectList::slotClipSelected()
{
    if (!m_listView->isEnabled()) return;
    if (m_listView->currentItem()) {
        if (m_listView->currentItem()->type() == PROJECTFOLDERTYPE) {
            emit clipSelected(NULL);
            m_editAction->setEnabled(false);
            m_deleteAction->setEnabled(true);
            m_openAction->setEnabled(false);
            m_reloadAction->setEnabled(false);
            m_transcodeAction->setEnabled(false);
        } else {
            ProjectItem *clip;
            if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
                // this is a sub item, use base clip
                m_deleteAction->setEnabled(true);
                clip = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
                if (clip == NULL) kDebug() << "-----------ERROR";
                SubProjectItem *sub = static_cast <SubProjectItem*>(m_listView->currentItem());
                emit clipSelected(clip->referencedClip(), sub->zone());
                m_transcodeAction->setEnabled(false);
                return;
            }
            clip = static_cast <ProjectItem*>(m_listView->currentItem());
            if (clip) emit clipSelected(clip->referencedClip());
            m_editAction->setEnabled(true);
            m_deleteAction->setEnabled(true);
            m_reloadAction->setEnabled(true);
            m_transcodeAction->setEnabled(true);
            if (clip && clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
                m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
                m_openAction->setEnabled(true);
            } else if (clip && clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
                m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
                m_openAction->setEnabled(true);
            } else m_openAction->setEnabled(false);
            // Display relevant transcoding actions only
            adjustTranscodeActions(clip);
            // Display uses in timeline
            emit findInTimeline(clip->clipId());
        }
    } else {
        emit clipSelected(NULL);
        m_editAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
        m_openAction->setEnabled(false);
        m_reloadAction->setEnabled(false);
        m_transcodeAction->setEnabled(false);
    }
}

void ProjectList::adjustTranscodeActions(ProjectItem *clip) const
{
    if (clip == NULL || clip->type() != PROJECTCLIPTYPE || clip->clipType() == COLOR || clip->clipType() == TEXT || clip->clipType() == PLAYLIST || clip->clipType() == SLIDESHOW || clip->clipType() == IMAGE) {
        m_transcodeAction->setEnabled(false);
        return;
    }
    m_transcodeAction->setEnabled(true);
    QList<QAction *> transcodeActions = m_transcodeAction->actions();
    QStringList data;
    QString condition;
    for (int i = 0; i < transcodeActions.count(); i++) {
        data = transcodeActions.at(i)->data().toStringList();
        if (data.count() > 2) {
            condition = data.at(2);
            if (condition.startsWith("vcodec")) transcodeActions.at(i)->setEnabled(clip->referencedClip()->hasVideoCodec(condition.section("=", 1, 1)));
            else if (condition.startsWith("acodec")) transcodeActions.at(i)->setEnabled(clip->referencedClip()->hasVideoCodec(condition.section("=", 1, 1)));
        }
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
        if (properties.contains("out") || properties.contains("force_fps")) {
            slotReloadClip(id);
        } else if (properties.contains("colour") || properties.contains("resource") || properties.contains("xmldata") || properties.contains("force_aspect_ratio") || properties.contains("templatetext")) {
            slotRefreshClipThumbnail(item);
            emit refreshClip();
        }
    }
}

void ProjectList::slotUpdateClipProperties(ProjectItem *clip, QMap <QString, QString> properties)
{
    if (!clip) return;
    clip->setProperties(properties);
    if (properties.contains("name")) {
        m_listView->blockSignals(true);
        clip->setText(0, properties.value("name"));
        m_listView->blockSignals(false);
        emit clipNameChanged(clip->clipId(), properties.value("name"));
    }
    if (properties.contains("description")) {
        CLIPTYPE type = clip->clipType();
        m_listView->blockSignals(true);
        clip->setText(1, properties.value("description"));
        m_listView->blockSignals(false);
#ifdef NEPOMUK
        if (KdenliveSettings::activate_nepomuk() && (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST)) {
            // Use Nepomuk system to store clip description
            Nepomuk::Resource f(clip->clipUrl().path());
            f.setDescription(properties.value("description"));
        }
#endif
        emit projectModified();
    }
}

void ProjectList::slotItemEdited(QTreeWidgetItem *item, int column)
{
    if (item->type() == PROJECTSUBCLIPTYPE) {
        // this is a sub-item
        if (column == 1) {
            // user edited description
            SubProjectItem *sub = static_cast <SubProjectItem*>(item);
            ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
            EditClipCutCommand *command = new EditClipCutCommand(this, item->clipId(), sub->zone(), sub->zone(), sub->description(), sub->text(1), true);
            m_commandStack->push(command);
            //slotUpdateCutClipProperties(sub->clipId(), sub->zone(), sub->text(1), sub->text(1));
        }
        return;
    }
    if (item->type() == PROJECTFOLDERTYPE) {
        if (column != 0) return;
        FolderProjectItem *folder = static_cast <FolderProjectItem*>(item);
        editFolder(item->text(0), folder->groupName(), folder->clipId());
        folder->setGroupName(item->text(0));
        m_doc->clipManager()->addFolder(folder->clipId(), item->text(0));
        const int children = item->childCount();
        for (int i = 0; i < children; i++) {
            ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
            child->setProperty("groupname", item->text(0));
        }
        return;
    }

    ProjectItem *clip = static_cast <ProjectItem*>(item);
    if (column == 1) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["description"] = clip->referencedClip()->getProperty("description");
            newprops["description"] = item->text(1);

            if (clip->clipType() == TEXT) {
                // This is a text template clip, update the image
                /*oldprops.insert("xmldata", clip->referencedClip()->getProperty("xmldata"));
                newprops.insert("xmldata", generateTemplateXml(clip->referencedClip()->getProperty("xmltemplate"), item->text(2)).toString());*/
                oldprops.insert("templatetext", clip->referencedClip()->getProperty("templatetext"));
                newprops.insert("templatetext", item->text(1));
            }
            slotUpdateClipProperties(clip->clipId(), newprops);
            EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
            m_commandStack->push(command);
        }
    } else if (column == 0) {
        if (clip->referencedClip()) {
            QMap <QString, QString> oldprops;
            QMap <QString, QString> newprops;
            oldprops["name"] = clip->referencedClip()->getProperty("name");
            newprops["name"] = item->text(0);
            slotUpdateClipProperties(clip, newprops);
            emit projectModified();
            EditClipCommand *command = new EditClipCommand(this, clip->clipId(), oldprops, newprops, false);
            m_commandStack->push(command);
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
    m_transcodeAction->setEnabled(enable);
    if (enable) {
        ProjectItem *clip = NULL;
        if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
            clip = static_cast <ProjectItem*>(item->parent());
            m_transcodeAction->setEnabled(false);
        } else if (m_listView->currentItem()->type() == PROJECTCLIPTYPE) {
            clip = static_cast <ProjectItem*>(item);
            // Display relevant transcoding actions only
            adjustTranscodeActions(clip);
            // Display uses in timeline
            emit findInTimeline(clip->clipId());
        } else m_transcodeAction->setEnabled(false);
        if (clip && clip->clipType() == IMAGE && !KdenliveSettings::defaultimageapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultimageapp()));
            m_openAction->setEnabled(true);
        } else if (clip && clip->clipType() == AUDIO && !KdenliveSettings::defaultaudioapp().isEmpty()) {
            m_openAction->setIcon(KIcon(KdenliveSettings::defaultaudioapp()));
            m_openAction->setEnabled(true);
        } else m_openAction->setEnabled(false);

    } else m_openAction->setEnabled(false);
    m_menu->popup(pos);
}

void ProjectList::slotRemoveClip()
{
    if (!m_listView->currentItem()) return;
    QStringList ids;
    QMap <QString, QString> folderids;
    QList<QTreeWidgetItem *> selected = m_listView->selectedItems();

    QUndoCommand *delCommand = new QUndoCommand();
    delCommand->setText(i18n("Delete Clip Zone"));

    for (int i = 0; i < selected.count(); i++) {
        if (selected.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            SubProjectItem *sub = static_cast <SubProjectItem *>(selected.at(i));
            ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
            new AddClipCutCommand(this, item->clipId(), sub->zone().x(), sub->zone().y(), sub->description(), false, true, delCommand);
        } else if (selected.at(i)->type() == PROJECTFOLDERTYPE) {
            // folder
            FolderProjectItem *folder = static_cast <FolderProjectItem *>(selected.at(i));
            folderids[folder->groupName()] = folder->clipId();
            int children = folder->childCount();

            if (children > 0 && KMessageBox::questionYesNo(this, i18np("Delete folder <b>%2</b>?<br />This will also remove the clip in that folder", "Delete folder <b>%2</b>?<br />This will also remove the %1 clips in that folder",  children, folder->text(1)), i18n("Delete Folder")) != KMessageBox::Yes) return;
            for (int i = 0; i < children; ++i) {
                ProjectItem *child = static_cast <ProjectItem *>(folder->child(i));
                ids << child->clipId();
            }
        } else {
            ProjectItem *item = static_cast <ProjectItem *>(selected.at(i));
            ids << item->clipId();
            if (item->numReferences() > 0) {
                if (KMessageBox::questionYesNo(this, i18np("Delete clip <b>%2</b>?<br />This will also remove the clip in timeline", "Delete clip <b>%2</b>?<br />This will also remove its %1 clips in timeline", item->numReferences(), item->names().at(1)), i18n("Delete Clip")) != KMessageBox::Yes) return;
            }
        }
    }

    if (delCommand->childCount() == 0) delete delCommand;
    else m_commandStack->push(delCommand);
    emit deleteProjectClips(ids, folderids);
}

void ProjectList::updateButtons() const
{
    if (m_listView->topLevelItemCount() == 0) {
        m_deleteAction->setEnabled(false);
    } else {
        m_deleteAction->setEnabled(true);
        if (!m_listView->currentItem()) m_listView->setCurrentItem(m_listView->topLevelItem(0));
        QTreeWidgetItem *item = m_listView->currentItem();
        if (item && item->type() == PROJECTCLIPTYPE) {
            m_editAction->setEnabled(true);
            m_openAction->setEnabled(true);
            m_reloadAction->setEnabled(true);
            m_transcodeAction->setEnabled(true);
            return;
        }
    }

    m_editAction->setEnabled(false);
    m_openAction->setEnabled(false);
    m_reloadAction->setEnabled(false);
    m_transcodeAction->setEnabled(false);
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
    m_listView->blockSignals(true);
    QTreeWidgetItem *newSelectedItem = m_listView->itemAbove(item);
    if (!newSelectedItem) newSelectedItem = m_listView->itemBelow(item);
    delete item;
    m_doc->clipManager()->deleteClip(clipId);
    m_listView->blockSignals(false);
    if (newSelectedItem) {
        m_listView->setCurrentItem(newSelectedItem);
    } else {
        updateButtons();
        emit clipSelected(NULL);
    }
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
        FolderProjectItem *item = getFolderItemById(clipId);
        if (item) {
            m_doc->clipManager()->deleteFolder(clipId);
            QTreeWidgetItem *newSelectedItem = m_listView->itemAbove(item);
            if (!newSelectedItem) newSelectedItem = m_listView->itemBelow(item);
            delete item;
            if (newSelectedItem) m_listView->setCurrentItem(newSelectedItem);
            else updateButtons();
        }
    } else {
        if (edit) {
            FolderProjectItem *item = getFolderItemById(clipId);
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
            m_listView->blockSignals(true);
            m_listView->setCurrentItem(new FolderProjectItem(m_listView, QStringList() << foldername, clipId));
            m_doc->clipManager()->addFolder(clipId, foldername);
            m_listView->blockSignals(false);
            m_listView->editItem(m_listView->currentItem(), 0);
        }
        updateButtons();
    }
    m_doc->setModified(true);
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
}

void ProjectList::slotAddClip(DocClipBase *clip, bool getProperties)
{
    m_listView->setEnabled(false);
    if (getProperties) {
        m_listView->blockSignals(true);
        m_refreshed = false;
        // remove file_hash so that we load all properties for the clip
        QDomElement e = clip->toXML().cloneNode().toElement();
        e.removeAttribute("file_hash");
        m_infoQueue.insert(clip->getId(), e);
        clip->askForAudioThumbs();
        //m_render->getFileProperties(clip->toXML(), clip->getId(), true);
    }
    const QString parent = clip->getProperty("groupid");
    ProjectItem *item = NULL;
    if (!parent.isEmpty()) {
        FolderProjectItem *parentitem = getFolderItemById(parent);
        if (!parentitem) {
            QStringList text;
            QString groupName = clip->getProperty("groupname");
            //kDebug() << "Adding clip to new group: " << groupName;
            if (groupName.isEmpty()) groupName = i18n("Folder");
            text << groupName;
            parentitem = new FolderProjectItem(m_listView, text, parent);
        } else {
            //kDebug() << "Adding clip to existing group: " << parentitem->groupName();
        }
        if (parentitem) item = new ProjectItem(parentitem, clip);
    }
    if (item == NULL) item = new ProjectItem(m_listView, clip);
    KUrl url = clip->fileURL();

    if (getProperties == false && !clip->getClipHash().isEmpty()) {
        QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + ".png";
        if (QFile::exists(cachedPixmap)) {
            QPixmap pix(cachedPixmap);
            if (pix.isNull()) KIO::NetAccess::del(KUrl(cachedPixmap), this);
            item->setData(0, Qt::DecorationRole, pix);
        }
    }
#ifdef NEPOMUK
    if (!url.isEmpty() && KdenliveSettings::activate_nepomuk()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation = f.description();
        if (!annotation.isEmpty()) item->setText(1, annotation);
        item->setText(2, QString::number(f.rating()));
    }
#endif
    // Add cut zones
    QList <CutZoneInfo> cuts = clip->cutZones();
    if (!cuts.isEmpty()) {
        for (int i = 0; i < cuts.count(); i++) {
            SubProjectItem *sub = new SubProjectItem(item, cuts.at(i).zone.x(), cuts.at(i).zone.y(), cuts.at(i).description);
            if (!clip->getClipHash().isEmpty()) {
                QString cachedPixmap = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "thumbs/" + clip->getClipHash() + '#' + QString::number(cuts.at(i).zone.x()) + ".png";
                if (QFile::exists(cachedPixmap)) {
                    QPixmap pix(cachedPixmap);
                    if (pix.isNull()) KIO::NetAccess::del(KUrl(cachedPixmap), this);
                    sub->setData(0, Qt::DecorationRole, pix);
                }
            }
        }
    }
    if (m_listView->isEnabled()) {
        updateButtons();
        if (getProperties) m_listView->blockSignals(false);
    }
    if (getProperties && !m_queueTimer.isActive()) m_queueTimer.start();
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
    m_refreshed = false;
    m_infoQueue.insert(id, xml);
    //if (m_infoQueue.count() == 1 || ) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotProcessNextClipInQueue()
{
    if (m_infoQueue.isEmpty()) {
        slotProcessNextThumbnail();
        return;
    }

    QMap<QString, QDomElement>::const_iterator j = m_infoQueue.constBegin();
    if (j != m_infoQueue.constEnd()) {
        const QDomElement dom = j.value();
        const QString id = j.key();
        m_infoQueue.remove(j.key());
        emit getFileProperties(dom, id, m_listView->iconSize().height(), false);
    }
}

void ProjectList::slotUpdateClip(const QString &id)
{
    ProjectItem *item = getItemById(id);
    m_listView->blockSignals(true);
    if (item) item->setData(0, UsageRole, QString::number(item->numReferences()));
    m_listView->blockSignals(false);
}

void ProjectList::updateAllClips()
{
    m_listView->setSortingEnabled(false);
    kDebug() << "// UPDATE ALL CLPY";

    QTreeWidgetItemIterator it(m_listView);
    DocClipBase *clip;
    ProjectItem *item;
    m_listView->blockSignals(true);
    while (*it) {
        if ((*it)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            SubProjectItem *sub = static_cast <SubProjectItem *>(*it);
            if (sub->data(0, Qt::DecorationRole).isNull()) {
                item = static_cast <ProjectItem *>((*it)->parent());
                requestClipThumbnail(item->clipId() + '#' + QString::number(sub->zone().x()));
            }
            ++it;
            continue;
        } else if ((*it)->type() == PROJECTFOLDERTYPE) {
            // folder
            ++it;
            continue;
        } else {
            item = static_cast <ProjectItem *>(*it);
            clip = item->referencedClip();
            if (item->referencedClip()->producer() == NULL) {
                if (clip->isPlaceHolder() == false) {
                    requestClipInfo(clip->toXML(), clip->getId());
                } else if (!clip->isPlaceHolder()) item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            } else {
                if (item->data(0, Qt::DecorationRole).isNull()) {
                    requestClipThumbnail(clip->getId());
                }
                if (item->data(0, DurationRole).toString().isEmpty()) {
                    item->changeDuration(item->referencedClip()->producer()->get_playtime());
                }
            }
            item->setData(0, UsageRole, QString::number(item->numReferences()));
        }
        //qApp->processEvents();
        ++it;
    }
    if (!m_queueTimer.isActive()) m_queueTimer.start();
    if (m_listView->isEnabled()) m_listView->blockSignals(false);
    m_listView->setSortingEnabled(true);
    if (m_infoQueue.isEmpty()) slotProcessNextThumbnail();
}

void ProjectList::slotAddClip(const QList <QUrl> givenList, const QString &groupName, const QString &groupId)
{
    if (!m_commandStack) {
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    }
    KUrl::List list;
    if (givenList.isEmpty()) {
        // Build list of mime types
        QStringList mimeTypes = QStringList() << "application/x-kdenlive" << "application/x-kdenlivetitle" << "video/x-flv" << "application/vnd.rn-realmedia" << "video/x-dv" << "video/dv" << "video/x-msvideo" << "video/x-matroska" << "video/mlt-playlist" << "video/mpeg" << "video/ogg" << "video/x-ms-wmv" << "audio/x-flac" << "audio/x-matroska" << "audio/mp4" << "audio/mpeg" << "audio/x-mp3" << "audio/ogg" << "audio/x-wav" << "application/ogg" << "video/mp4" << "video/quicktime" << "image/gif" << "image/jpeg" << "image/png" << "image/x-tga" << "image/x-bmp" << "image/svg+xml" << "image/tiff" << "image/x-xcf" << "image/x-xcf-gimp" << "image/x-vnd.adobe.photoshop" << "image/x-pcx" << "image/x-exr";

        QString allExtensions;
        foreach(const QString& mimeType, mimeTypes) {
            KMimeType::Ptr mime(KMimeType::mimeType(mimeType));
            if (mime) {
                allExtensions.append(mime->patterns().join(" "));
                allExtensions.append(' ');
            }
        }
        const QString dialogFilter = allExtensions.simplified() + ' ' + QLatin1Char('|') + i18n("All Supported Files") + "\n* " + QLatin1Char('|') + i18n("All Files");
        list = KFileDialog::getOpenUrls(KUrl("kfiledialog:///clipfolder"), dialogFilter, this);

    } else {
        for (int i = 0; i < givenList.count(); i++)
            list << givenList.at(i);
    }

    foreach(const KUrl &file, list) {
        // Check there is no folder here
        KMimeType::Ptr type = KMimeType::findByUrl(file);
        if (type->is("inode/directory")) {
            // user dropped a folder
            list.removeAll(file);
        }
    }

    if (list.isEmpty()) return;

    if (givenList.isEmpty()) {
        QStringList groupInfo = getGroup();
        m_doc->slotAddClipList(list, groupInfo.at(0), groupInfo.at(1));
    } else m_doc->slotAddClipList(list, groupName, groupId);
}

void ProjectList::slotRemoveInvalidClip(const QString &id, bool replace)
{
    ProjectItem *item = getItemById(id);
    QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
    if (item) {
        const QString path = item->referencedClip()->fileURL().path();
        if (item->referencedClip()->isPlaceHolder()) replace = false;
        if (!path.isEmpty()) {
            if (replace) KMessageBox::sorry(this, i18n("Clip <b>%1</b><br />is invalid, will be removed from project.", path));
            else {
                if (KMessageBox::questionYesNo(this, i18n("Clip <b>%1</b><br />is missing or invalid. Remove it from project?", path), i18n("Invalid clip")) == KMessageBox::Yes) replace = true;
            }
        }
        QStringList ids;
        ids << id;
        if (replace) emit deleteProjectClips(ids, QMap <QString, QString>());
    }
}

void ProjectList::slotAddColorClip()
{
    if (!m_commandStack) {
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    }
    QDialog *dia = new QDialog(this);
    Ui::ColorClip_UI dia_ui;
    dia_ui.setupUi(dia);
    dia->setWindowTitle(i18n("Color Clip"));
    dia_ui.clip_name->setText(i18n("Color Clip"));
    dia_ui.clip_duration->setInputMask("");
    dia_ui.clip_duration->setValidator(m_timecode.validator());
    dia_ui.clip_duration->setText(m_timecode.reformatSeparators(KdenliveSettings::color_duration()));
    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui.clip_color->color().name();
        color = color.replace(0, 1, "0x") + "ff";
        QStringList groupInfo = getGroup();
        m_doc->slotCreateColorClip(dia_ui.clip_name->text(), color, dia_ui.clip_duration->text(), groupInfo.at(0), groupInfo.at(1));
    }
    delete dia;
}


void ProjectList::slotAddSlideshowClip()
{
    if (!m_commandStack) {
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    }
    SlideshowClip *dia = new SlideshowClip(m_timecode, this);

    if (dia->exec() == QDialog::Accepted) {
        QStringList groupInfo = getGroup();
        m_doc->slotCreateSlideshowClipFile(dia->clipName(), dia->selectedPath(), dia->imageCount(), dia->clipDuration(), dia->loop(), dia->fade(), dia->lumaDuration(), dia->lumaFile(), dia->softness(), groupInfo.at(0), groupInfo.at(1));
    }
    delete dia;
}

void ProjectList::slotAddTitleClip()
{
    QStringList groupInfo = getGroup();
    m_doc->slotCreateTextClip(groupInfo.at(0), groupInfo.at(1));
}

void ProjectList::slotAddTitleTemplateClip()
{
    QStringList groupInfo = getGroup();
    if (!m_commandStack) {
        kDebug() << "!!!!!!!!!!!!!!!! NO CMD STK";
    }

    // Get the list of existing templates
    QStringList filter;
    filter << "*.kdenlivetitle";
    const QString path = m_doc->projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
    QStringList templateFiles = QDir(path).entryList(filter, QDir::Files);

    QDialog *dia = new QDialog(this);
    Ui::TemplateClip_UI dia_ui;
    dia_ui.setupUi(dia);
    for (int i = 0; i < templateFiles.size(); ++i) {
        dia_ui.template_list->comboBox()->addItem(templateFiles.at(i), path + templateFiles.at(i));
    }
    if (!templateFiles.isEmpty())
        dia_ui.buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    dia_ui.template_list->fileDialog()->setFilter("*.kdenlivetitle");
    //warning: setting base directory doesn't work??
    KUrl startDir(path);
    dia_ui.template_list->fileDialog()->setUrl(startDir);
    dia_ui.text_box->setHidden(true);
    if (dia->exec() == QDialog::Accepted) {
        QString textTemplate = dia_ui.template_list->comboBox()->itemData(dia_ui.template_list->comboBox()->currentIndex()).toString();
        if (textTemplate.isEmpty()) textTemplate = dia_ui.template_list->comboBox()->currentText();
        // Create a cloned template clip
        m_doc->slotCreateTextTemplateClip(groupInfo.at(0), groupInfo.at(1), KUrl(textTemplate));
    }
    delete dia;
}

QStringList ProjectList::getGroup() const
{
    QStringList result;
    QTreeWidgetItem *item = m_listView->currentItem();
    while (item && item->type() != PROJECTFOLDERTYPE) {
        item = item->parent();
    }

    if (item) {
        FolderProjectItem *folder = static_cast <FolderProjectItem *>(item);
        result << folder->groupName();
        result << folder->clipId();
    } else result << QString() << QString();
    return result;
}

void ProjectList::setDocument(KdenliveDoc *doc)
{
    m_listView->blockSignals(true);
    m_listView->clear();
    m_listView->setSortingEnabled(false);
    emit clipSelected(NULL);
    m_thumbnailQueue.clear();
    m_infoQueue.clear();
    m_refreshed = false;
    m_fps = doc->fps();
    m_timecode = doc->timecode();
    m_commandStack = doc->commandStack();
    m_doc = doc;

    QMap <QString, QString> flist = doc->clipManager()->documentFolderList();
    QMapIterator<QString, QString> f(flist);
    while (f.hasNext()) {
        f.next();
        (void) new FolderProjectItem(m_listView, QStringList() << f.value(), f.key());
    }

    QList <DocClipBase*> list = doc->clipManager()->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        slotAddClip(list.at(i), false);
    }

    m_listView->blockSignals(false);
    m_toolbar->setEnabled(true);
    connect(m_doc->clipManager(), SIGNAL(reloadClip(const QString &)), this, SLOT(slotReloadClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(missingClip(const QString &)), this, SLOT(slotMissingClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(availableClip(const QString &)), this, SLOT(slotAvailableClip(const QString &)));
    connect(m_doc->clipManager(), SIGNAL(checkAllClips()), this, SLOT(updateAllClips()));
}

QList <DocClipBase*> ProjectList::documentClipList() const
{
    if (m_doc == NULL) return QList <DocClipBase*> ();
    return m_doc->clipManager()->documentClipList();
}

QDomElement ProjectList::producersList()
{
    QDomDocument doc;
    QDomElement prods = doc.createElement("producerlist");
    doc.appendChild(prods);
    kDebug() << "////////////  PRO LIST BUILD PRDSLIST ";
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
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
        emit displayMessage(QString(), -1);
        m_listView->blockSignals(false);
        m_listView->setEnabled(true);
        updateButtons();
    } else if (!m_refreshed) QTimer::singleShot(300, this, SLOT(slotCheckForEmptyQueue()));
}

void ProjectList::reloadClipThumbnails()
{
    kDebug() << "//////////////  RELOAD CLIPS THUMBNAILS!!!";
    m_thumbnailQueue.clear();
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        m_thumbnailQueue << ((ProjectItem *)(*it))->clipId();
        ++it;
    }
    QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
}

void ProjectList::requestClipThumbnail(const QString id)
{
    if (!m_thumbnailQueue.contains(id)) m_thumbnailQueue.append(id);
}

void ProjectList::slotProcessNextThumbnail()
{
    if (m_thumbnailQueue.isEmpty() && m_infoQueue.isEmpty()) {
        slotCheckForEmptyQueue();
        return;
    }
    if (!m_infoQueue.isEmpty()) {
        //QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
        return;
    }
    if (m_thumbnailQueue.count() > 1) {
        int max = m_doc->clipManager()->clipsCount();
        emit displayMessage(i18n("Loading thumbnails"), (int)(100 *(max - m_thumbnailQueue.count()) / max));
    }
    slotRefreshClipThumbnail(m_thumbnailQueue.takeFirst(), false);
}

void ProjectList::slotRefreshClipThumbnail(const QString &clipId, bool update)
{
    QTreeWidgetItem *item = getAnyItemById(clipId);
    if (item) slotRefreshClipThumbnail(item, update);
    else slotProcessNextThumbnail();
}

void ProjectList::slotRefreshClipThumbnail(QTreeWidgetItem *it, bool update)
{
    if (it == NULL) return;
    ProjectItem *item = NULL;
    bool isSubItem = false;
    int frame;
    if (it->type() == PROJECTFOLDERTYPE) return;
    if (it->type() == PROJECTSUBCLIPTYPE) {
        item = static_cast <ProjectItem *>(it->parent());
        frame = static_cast <SubProjectItem *>(it)->zone().x();
        isSubItem = true;
    } else {
        item = static_cast <ProjectItem *>(it);
        frame = item->referencedClip()->getClipThumbFrame();
    }

    if (item) {
        DocClipBase *clip = item->referencedClip();
        if (!clip) {
            slotProcessNextThumbnail();
            return;
        }
        QPixmap pix;
        int height = m_listView->iconSize().height();
        int width = (int)(height  * m_render->dar());
        if (clip->clipType() == AUDIO) pix = KIcon("audio-x-generic").pixmap(QSize(width, height));
        else if (clip->clipType() == IMAGE) pix = QPixmap::fromImage(KThumb::getFrame(item->referencedClip()->producer(), 0, width, height));
        else pix = item->referencedClip()->thumbProducer()->extractImage(frame, width, height);

        if (!pix.isNull()) {
            m_listView->blockSignals(true);
            it->setData(0, Qt::DecorationRole, pix);
            if (m_listView->isEnabled()) m_listView->blockSignals(false);
            if (!isSubItem) m_doc->cachePixmap(item->getClipHash(), pix);
            else m_doc->cachePixmap(item->getClipHash() + '#' + QString::number(frame), pix);
        }
        if (update) emit projectModified();
        QTimer::singleShot(30, this, SLOT(slotProcessNextThumbnail()));
    }
}

void ProjectList::slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata, bool replace)
{
    QString toReload;
    ProjectItem *item = getItemById(clipId);
    if (item && producer) {
        m_listView->blockSignals(true);
        item->setProperties(properties, metadata);
        if (item->referencedClip()->isPlaceHolder() && producer->is_valid()) {
            item->referencedClip()->setValid();
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
            toReload = clipId;
        }
        //Q_ASSERT_X(item->referencedClip(), "void ProjectList::slotReplyGetFileProperties", QString("Item with groupName %1 does not have a clip associated").arg(item->groupName()).toLatin1());
        item->referencedClip()->setProducer(producer, replace);
        item->referencedClip()->askForAudioThumbs();
        if (!replace && item->data(0, Qt::DecorationRole).isNull()) {
            requestClipThumbnail(clipId);
        }
        if (!toReload.isEmpty()) {
            item->slotSetToolTip();

        }
        //emit receivedClipDuration(clipId);
        if (m_listView->isEnabled() && replace) {
            // update clip in clip monitor
            emit clipSelected(NULL);
            emit clipSelected(item->referencedClip());
            //TODO: Make sure the line below has no side effect
            toReload = clipId;
        }
        /*else {
            // Check if duration changed.
            emit receivedClipDuration(clipId);
            delete producer;
        }*/
        if (m_listView->isEnabled()) m_listView->blockSignals(false);
        /*if (item->icon(0).isNull()) {
            requestClipThumbnail(clipId);
        }*/
    } else kDebug() << "////////  COULD NOT FIND CLIP TO UPDATE PRPS...";
    int max = m_doc->clipManager()->clipsCount();
    if (item && m_infoQueue.isEmpty() && m_thumbnailQueue.isEmpty()) {
        m_listView->setCurrentItem(item);
        emit clipSelected(item->referencedClip());
    } else emit displayMessage(i18n("Loading clips"), (int)(100 *(max - m_infoQueue.count()) / max));
    if (!toReload.isEmpty()) emit clipNeedsReload(toReload, true);
    // small delay so that the app can display the progress info
    QTimer::singleShot(30, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotReplyGetImage(const QString &clipId, const QPixmap &pix)
{
    ProjectItem *item = getItemById(clipId);
    if (item && !pix.isNull()) {
        m_listView->blockSignals(true);
        item->setData(0, Qt::DecorationRole, pix);
        m_doc->cachePixmap(item->getClipHash(), pix);
        if (m_listView->isEnabled()) m_listView->blockSignals(false);
    }
}

QTreeWidgetItem *ProjectList::getAnyItemById(const QString &id)
{
    QTreeWidgetItemIterator it(m_listView);
    QString lookId = id;
    if (id.contains('#')) {
        lookId = id.section('#', 0, 0);
    }

    ProjectItem *result = NULL;
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        ProjectItem *item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == lookId) {
            result = item;
            break;
        }
        ++it;
    }
    if (result == NULL || !id.contains('#')) return result;
    else for (int i = 0; i < result->childCount(); i++) {
            SubProjectItem *sub = static_cast <SubProjectItem *>(result->child(i));
            if (sub && sub->zone().x() == id.section('#', 1, 1).toInt()) {
                return sub;
            }
        }

    return NULL;
}


ProjectItem *ProjectList::getItemById(const QString &id)
{
    ProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() != PROJECTCLIPTYPE) {
            // subitem
            ++it;
            continue;
        }
        item = static_cast<ProjectItem *>(*it);
        if (item->clipId() == id)
            return item;
        ++it;
    }
    return NULL;
}

FolderProjectItem *ProjectList::getFolderItemById(const QString &id)
{
    FolderProjectItem *item;
    QTreeWidgetItemIterator it(m_listView);
    while (*it) {
        if ((*it)->type() == PROJECTFOLDERTYPE) {
            item = static_cast<FolderProjectItem *>(*it);
            if (item->clipId() == id) return item;
        }
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
        m_transcodeAction->setEnabled(true);
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
    ProjectItem *item;
    if (!m_listView->currentItem() || m_listView->currentItem()->type() == PROJECTFOLDERTYPE) return QString();
    if (m_listView->currentItem()->type() == PROJECTSUBCLIPTYPE) {
        // subitem
        item = static_cast <ProjectItem*>(m_listView->currentItem()->parent());
    } else item = static_cast <ProjectItem*>(m_listView->currentItem());
    if (item == NULL) return QString();
    return item->clipUrl().path();
}

KUrl::List ProjectList::getConditionalUrls(const QString &condition) const
{
    KUrl::List result;
    ProjectItem *item;
    QList<QTreeWidgetItem *> list = m_listView->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == PROJECTFOLDERTYPE) continue;
        if (list.at(i)->type() == PROJECTSUBCLIPTYPE) {
            // subitem
            item = static_cast <ProjectItem*>(list.at(i)->parent());
        } else item = static_cast <ProjectItem*>(list.at(i));
        if (item == NULL) continue;
        if (item->type() == COLOR || item->type() == SLIDESHOW || item->type() == TEXT) continue;
        DocClipBase *clip = item->referencedClip();
        if (!condition.isEmpty()) {
            if (condition.startsWith("vcodec") && !clip->hasVideoCodec(condition.section("=", 1, 1))) continue;
            else if (condition.startsWith("acodec") && !clip->hasAudioCodec(condition.section("=", 1, 1))) continue;
        }
        result.append(item->clipUrl());
    }
    return result;
}

void ProjectList::regenerateTemplate(const QString &id)
{
    ProjectItem *clip = getItemById(id);
    if (clip) regenerateTemplate(clip);
}

void ProjectList::regenerateTemplate(ProjectItem *clip)
{
    //TODO: remove this unused method, only force_reload is necessary
    clip->referencedClip()->producer()->set("force_reload", 1);
}

QDomDocument ProjectList::generateTemplateXml(QString path, const QString &replaceString)
{
    QDomDocument doc;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        return doc;
    }
    if (!doc.setContent(&file)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        file.close();
        return doc;
    }
    file.close();
    QDomNodeList texts = doc.elementsByTagName("content");
    for (int i = 0; i < texts.count(); i++) {
        QString data = texts.item(i).firstChild().nodeValue();
        data.replace("%s", replaceString);
        texts.item(i).firstChild().setNodeValue(data);
    }
    return doc;
}


void ProjectList::slotAddClipCut(const QString &id, int in, int out)
{
    ProjectItem *clip = getItemById(id);
    if (clip == NULL) return;
    if (clip->referencedClip()->hasCutZone(QPoint(in, out))) return;
    AddClipCutCommand *command = new AddClipCutCommand(this, id, in, out, QString(), true, false);
    m_commandStack->push(command);
}

void ProjectList::addClipCut(const QString &id, int in, int out, const QString desc, bool newItem)
{
    ProjectItem *clip = getItemById(id);
    if (clip) {
        DocClipBase *base = clip->referencedClip();
        base->addCutZone(in, out);
        m_listView->blockSignals(true);
        SubProjectItem *sub = new SubProjectItem(clip, in, out, desc);
        if (newItem && desc.isEmpty() && !m_listView->isColumnHidden(1)) {
            if (!clip->isExpanded()) clip->setExpanded(true);
            m_listView->scrollToItem(sub);
            m_listView->editItem(sub, 1);
        }
        QPixmap p = clip->referencedClip()->thumbProducer()->extractImage(in, (int)(sub->sizeHint(0).height()  * m_render->dar()), sub->sizeHint(0).height() - 2);
        sub->setData(0, Qt::DecorationRole, p);
        m_doc->cachePixmap(clip->getClipHash() + '#' + QString::number(in), p);
        m_listView->blockSignals(false);
    }
    emit projectModified();
}

void ProjectList::removeClipCut(const QString &id, int in, int out)
{
    ProjectItem *clip = getItemById(id);
    if (clip) {
        DocClipBase *base = clip->referencedClip();
        base->removeCutZone(in, out);
        SubProjectItem *sub = getSubItem(clip, QPoint(in, out));
        if (sub) {
            m_listView->blockSignals(true);
            delete sub;
            m_listView->blockSignals(false);
        }
    }
    emit projectModified();
}

SubProjectItem *ProjectList::getSubItem(ProjectItem *clip, QPoint zone)
{
    SubProjectItem *sub = NULL;
    if (clip) {
        for (int i = 0; i < clip->childCount(); i++) {
            QTreeWidgetItem *it = clip->child(i);
            if (it->type() == PROJECTSUBCLIPTYPE) {
                sub = static_cast <SubProjectItem*>(it);
                if (sub->zone() == zone) break;
                else sub = NULL;
            }
        }
    }
    return sub;
}

void ProjectList::slotUpdateClipCut(QPoint p)
{
    if (!m_listView->currentItem() || m_listView->currentItem()->type() != PROJECTSUBCLIPTYPE) return;
    SubProjectItem *sub = static_cast <SubProjectItem*>(m_listView->currentItem());
    ProjectItem *item = static_cast <ProjectItem *>(sub->parent());
    EditClipCutCommand *command = new EditClipCutCommand(this, item->clipId(), sub->zone(), p, sub->text(1), sub->text(1), true);
    m_commandStack->push(command);
}

void ProjectList::doUpdateClipCut(const QString &id, const QPoint oldzone, const QPoint zone, const QString &comment)
{
    ProjectItem *clip = getItemById(id);
    SubProjectItem *sub = getSubItem(clip, oldzone);
    if (sub == NULL || clip == NULL) return;
    DocClipBase *base = clip->referencedClip();
    base->updateCutZone(oldzone.x(), oldzone.y(), zone.x(), zone.y(), comment);
    m_listView->blockSignals(true);
    sub->setZone(zone);
    sub->setDescription(comment);
    m_listView->blockSignals(false);
    emit projectModified();
}

#include "projectlist.moc"
