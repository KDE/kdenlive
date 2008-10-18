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


#include <QMouseEvent>
#include <QStylePainter>
#include <QPixmap>
#include <QIcon>
#include <QDialog>
#include <QtGui>

#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <KInputDialog>
#include <KMessageBox>

#include <nepomuk/global.h>
#include <nepomuk/resource.h>
#include <nepomuk/tag.h>

#include "projectlist.h"
#include "projectitem.h"
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

ProjectList::ProjectList(QWidget *parent)
        : QWidget(parent), m_render(NULL), m_fps(-1), m_commandStack(NULL), m_selectedItem(NULL), m_infoQueue(QMap <QString, QDomElement> ()), m_thumbnailQueue(QList <QString> ()), m_refreshed(false) {

    QWidget *vbox = new QWidget;
    listView = new ProjectListView(this);;
    QVBoxLayout *layout = new QVBoxLayout;
    m_clipIdCounter = 0;
    // setup toolbar
    searchView = new KTreeWidgetSearchLine(this);
    m_toolbar = new QToolBar("projectToolBar", this);
    m_toolbar->addWidget(searchView);

    QToolButton *addButton = new QToolButton(m_toolbar);
    QMenu *addMenu = new QMenu(this);
    addButton->setMenu(addMenu);
    addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(addButton);

    QAction *addClipButton = addMenu->addAction(KIcon("kdenlive-add-clip"), i18n("Add Clip"));
    connect(addClipButton, SIGNAL(triggered()), this, SLOT(slotAddClip()));

    QAction *addColorClip = addMenu->addAction(KIcon("kdenlive-add-color-clip"), i18n("Add Color Clip"));
    connect(addColorClip, SIGNAL(triggered()), this, SLOT(slotAddColorClip()));

    QAction *addSlideClip = addMenu->addAction(KIcon("kdenlive-add-slide-clip"), i18n("Add Slideshow Clip"));
    connect(addSlideClip, SIGNAL(triggered()), this, SLOT(slotAddSlideshowClip()));

    QAction *addTitleClip = addMenu->addAction(KIcon("kdenlive-add-text-clip"), i18n("Add Title Clip"));
    connect(addTitleClip, SIGNAL(triggered()), this, SLOT(slotAddTitleClip()));

    m_deleteAction = m_toolbar->addAction(KIcon("edit-delete"), i18n("Delete Clip"));
    connect(m_deleteAction, SIGNAL(triggered()), this, SLOT(slotRemoveClip()));

    m_editAction = m_toolbar->addAction(KIcon("document-properties"), i18n("Edit Clip"));
    connect(m_editAction, SIGNAL(triggered()), this, SLOT(slotEditClip()));

    QAction *addFolderButton = addMenu->addAction(KIcon("folder-new"), i18n("Create Folder"));
    connect(addFolderButton, SIGNAL(triggered()), this, SLOT(slotAddFolder()));

    addButton->setDefaultAction(addClipButton);

    layout->addWidget(m_toolbar);
    layout->addWidget(listView);
    setLayout(layout);
    //m_toolbar->setEnabled(false);

    searchView->setTreeWidget(listView);

    m_menu = new QMenu();
    m_menu->addAction(addClipButton);
    m_menu->addAction(addColorClip);
    m_menu->addAction(addSlideClip);
    m_menu->addAction(addTitleClip);
    m_menu->addAction(m_editAction);
    m_menu->addAction(m_deleteAction);
    m_menu->addAction(addFolderButton);
    m_menu->insertSeparator(m_deleteAction);

    connect(listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
    connect(listView, SIGNAL(focusMonitor()), this, SLOT(slotClipSelected()));
    connect(listView, SIGNAL(requestMenu(const QPoint &, QTreeWidgetItem *)), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
    connect(listView, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(listView, SIGNAL(addClip(QUrl, const QString &)), this, SLOT(slotAddClip(QUrl, const QString &)));
    connect(listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotItemEdited(QTreeWidgetItem *, int)));
    connect(listView, SIGNAL(showProperties(DocClipBase *)), this, SIGNAL(showClipProperties(DocClipBase *)));

    m_listViewDelegate = new ItemDelegate(listView);
    listView->setItemDelegate(m_listViewDelegate);
}

ProjectList::~ProjectList() {
    delete m_menu;
    delete m_toolbar;
}

void ProjectList::slotEditClip() {
    ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
    if (item && !item->isGroup()) {
        emit clipSelected(item->referencedClip());
        emit showClipProperties(item->referencedClip());
    }
}



void ProjectList::setRenderer(Render *projectRender) {
    m_render = projectRender;
}

void ProjectList::slotClipSelected() {
    if (listView->currentItem()) {
        ProjectItem *clip = static_cast <ProjectItem*>(listView->currentItem());
        if (!clip->isGroup()) {
            m_selectedItem = clip;
            emit clipSelected(clip->referencedClip());
        }
        m_editAction->setEnabled(true);
        m_deleteAction->setEnabled(true);
    } else {
        m_editAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
    }
}

void ProjectList::slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties) {
    ProjectItem *item = getItemById(id);
    if (item) {
        slotUpdateClipProperties(item, properties);
        if (properties.contains("colour") || properties.contains("resource") || properties.contains("xmldata")) slotRefreshClipThumbnail(item);
        if (properties.contains("out")) item->changeDuration(properties.value("out").toInt());
    }
}

void ProjectList::slotUpdateClipProperties(ProjectItem *clip, QMap <QString, QString> properties) {
    if (!clip) return;
    if (!clip->isGroup()) clip->setProperties(properties);
    if (properties.contains("description")) {
        CLIPTYPE type = clip->clipType();
        clip->setText(2, properties.value("description"));
        if (KdenliveSettings::activate_nepomuk() && (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST)) {
            // Use Nepomuk system to store clip description
            Nepomuk::Resource f(clip->clipUrl().path());
            if (f.isValid()) {
                f.setDescription(properties.value("description"));
            } else {
                KMessageBox::sorry(this, i18n("Cannot access Desktop Search info for %1.\nDisabling Desktop Search integration.", clip->clipUrl().path()));
                KdenliveSettings::setActivate_nepomuk(false);
            }
        }
        emit projectModified();
    }
}

void ProjectList::slotItemEdited(QTreeWidgetItem *item, int column) {
    ProjectItem *clip = static_cast <ProjectItem*>(item);
    if (column == 2) {
        QMap <QString, QString> props;
        props["description"] = item->text(2);
        slotUpdateClipProperties(clip, props);
    } else if (column == 1 && clip->isGroup()) {
        m_doc->slotEditFolder(item->text(1), clip->groupName(), clip->clipId());
        clip->setGroupName(item->text(1));
        const int children = item->childCount();
        for (int i = 0; i < children; i++) {
            ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
            child->setProperty("groupname", item->text(1));
        }
    }
}

void ProjectList::slotContextMenu(const QPoint &pos, QTreeWidgetItem *item) {
    bool enable = false;
    if (item) {
        enable = true;
    }
    m_editAction->setEnabled(enable);
    m_deleteAction->setEnabled(enable);
    m_menu->popup(pos);
}

void ProjectList::slotRemoveClip() {
    if (!listView->currentItem()) return;
    QList <QString> ids;
    QMap <QString, QString> folderids;
    QList<QTreeWidgetItem *> selected = listView->selectedItems();
    ProjectItem *item;
    for (int i = 0; i < selected.count(); i++) {
        item = static_cast <ProjectItem *>(selected.at(i));
        if (item->isGroup()) folderids[item->groupName()] = item->clipId();
        else ids << item->clipId();
        if (item->numReferences() > 0) {
            if (KMessageBox::questionYesNo(this, i18np("Delete clip <b>%2</b> ?<br>This will also remove the clip in timeline", "Delete clip <b>%2</b> ?<br>This will also remove its %1 clips in timeline", item->numReferences(), item->names().at(1)), i18n("Delete Clip")) != KMessageBox::Yes) return;
        } else if (item->isGroup() && item->childCount() > 0) {
            int children = item->childCount();
            if (KMessageBox::questionYesNo(this, i18n("Delete folder <b>%2</b> ?<br>This will also remove the %1 clips in that folder", children, item->names().at(1)), i18n("Delete Folder")) != KMessageBox::Yes) return;
            for (int i = 0; i < children; ++i) {
                ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                ids << child->clipId();
            }
        }
    }
    if (!ids.isEmpty()) m_doc->deleteProjectClip(ids);
    if (!folderids.isEmpty()) m_doc->deleteProjectFolder(folderids);
    if (listView->topLevelItemCount() == 0) {
        m_editAction->setEnabled(false);
        m_deleteAction->setEnabled(false);
    }
}

void ProjectList::selectItemById(const QString &clipId) {
    ProjectItem *item = getItemById(clipId);
    if (item) listView->setCurrentItem(item);
}


void ProjectList::slotDeleteClip(const QString &clipId) {
    ProjectItem *item = getItemById(clipId);
    QTreeWidgetItem *p = item->parent();
    if (p) {
        kDebug() << "///////  DELETEED CLIP HAS A PARENT... " << p->indexOfChild(item);
        QTreeWidgetItem *clone = p->takeChild(p->indexOfChild(item));
    } else if (item) {
        delete item;
    }
}

void ProjectList::slotAddFolder() {

    // QString folderName = KInputDialog::getText(i18n("New Folder"), i18n("Enter new folder name: "));
    // if (folderName.isEmpty()) return;
    m_doc->slotAddFolder(i18n("Folder")); //folderName);
}

void ProjectList::slotAddFolder(const QString foldername, const QString &clipId, bool remove, bool edit) {
    if (remove) {
        ProjectItem *item;
        QTreeWidgetItemIterator it(listView);
        while (*it) {
            item = static_cast <ProjectItem *>(*it);
            if (item->isGroup() && item->clipId() == clipId) {
                delete item;
                break;
            }
            ++it;
        }
    } else {
        if (edit) {
            listView->blockSignals(true);
            ProjectItem *item;
            QTreeWidgetItemIterator it(listView);
            while (*it) {
                item = static_cast <ProjectItem *>(*it);
                if (item->isGroup() && item->clipId() == clipId) {
                    item->setGroupName(foldername);
                    const int children = item->childCount();
                    for (int i = 0; i < children; i++) {
                        ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
                        child->setProperty("groupname", foldername);
                    }
                    break;
                }
                ++it;
            }
            listView->blockSignals(false);
        } else {
            QStringList text;
            text << QString() << foldername;
            (void) new ProjectItem(listView, text, clipId);
        }
    }
}

void ProjectList::slotAddClip(DocClipBase *clip, bool getProperties) {
    const QString parent = clip->getProperty("groupid");
    ProjectItem *item = NULL;
    if (!parent.isEmpty()) {
        ProjectItem *parentitem = getItemById(parent);
        if (!parentitem) {
            QStringList text;
            QString groupName = clip->getProperty("groupname");
            if (groupName.isEmpty()) groupName = i18n("Folder");
            text << QString() << groupName;
            listView->blockSignals(true);
            parentitem = new ProjectItem(listView, text, parent);
            listView->blockSignals(false);
        }
        if (parentitem) item = new ProjectItem(parentitem, clip);
    }
    if (item == NULL) item = new ProjectItem(listView, clip);

    KUrl url = clip->fileURL();
    if (!url.isEmpty() && KdenliveSettings::activate_nepomuk()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation;
        if (f.isValid()) {
            annotation = f.description();
            /*
            Nepomuk::Tag tag("test");
            f.addTag(tag);*/
        } else {
            KMessageBox::sorry(this, i18n("Cannot access Desktop Search info for %1.\nDisabling Desktop Search integration.", url.path()));
            KdenliveSettings::setActivate_nepomuk(false);
        }
        if (!annotation.isEmpty()) item->setText(2, annotation);
    }
    if (getProperties) requestClipInfo(clip->toXML(), clip->getId());
}

void ProjectList::requestClipInfo(const QDomElement xml, const QString id) {
    m_infoQueue.insert(id, xml);
    listView->setEnabled(false);
    if (m_infoQueue.count() == 1) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotProcessNextClipInQueue() {
    if (m_infoQueue.isEmpty()) {
        listView->setEnabled(true);
        return;
    }
    QMap<QString, QDomElement>::const_iterator i = m_infoQueue.constBegin();
    if (i != m_infoQueue.constEnd()) {
        const QDomElement dom = i.value();
        const QString id = i.key();
        m_infoQueue.remove(i.key());
        emit getFileProperties(dom, id);
    }
    if (m_infoQueue.isEmpty()) listView->setEnabled(true);
}

void ProjectList::slotUpdateClip(const QString &id) {
    ProjectItem *item = getItemById(id);
    item->setData(1, UsageRole, QString::number(item->numReferences()));
}

void ProjectList::updateAllClips() {
    QTreeWidgetItemIterator it(listView);
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
                    QPixmap pix = dia_ui->renderedPixmap();
                    pix.save(clip->fileURL().path());
                    delete dia_ui;
                }
                requestClipInfo(clip->toXML(), clip->getId());
            } else {
                requestClipThumbnail(item->clipId());
                item->changeDuration(item->referencedClip()->producer()->get_playtime());
            }
            item->setData(1, UsageRole, QString::number(item->numReferences()));
            qApp->processEvents();
        }
        ++it;
    }
    QTimer::singleShot(500, this, SLOT(slotCheckForEmptyQueue()));
}

void ProjectList::slotAddClip(QUrl givenUrl, QString group) {
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!!  NO CMD STK";
    KUrl::List list;
    if (givenUrl.isEmpty()) {
        list = KFileDialog::getOpenUrls(KUrl("kfiledialog:///clipfolder"), "application/x-kdenlive application/x-flash-video application/vnd.rn-realmedia video/x-dv video/dv video/x-msvideo video/mpeg video/x-ms-wmv audio/mpeg audio/x-mp3 audio/x-wav application/ogg video/mp4 video/quicktime image/gif image/jpeg image/png image/x-bmp image/svg+xml image/tiff image/x-xcf-gimp image/x-vnd.adobe.photoshop image/x-pcx image/x-exr video/mlt-playlist audio/x-flac audio/mp4", this);
    } else list.append(givenUrl);
    if (list.isEmpty()) return;

    QString groupId = QString();
    if (group.isEmpty()) {
        ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
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

void ProjectList::slotRemoveInvalidClip(const QString &id) {
    ProjectItem *item = getItemById(id);
    if (item) {
        QString path = item->referencedClip()->fileURL().path();
        if (!path.isEmpty()) KMessageBox::sorry(this, i18n("<qt>Clip <b>%1</b><br>is invalid, will be removed from project.", path));

    }
    QList <QString> ids;
    ids << id;
    m_doc->deleteProjectClip(ids);
    if (!m_infoQueue.isEmpty()) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotAddColorClip() {
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!!  NO CMD STK";
    QDialog *dia = new QDialog(this);
    Ui::ColorClip_UI *dia_ui = new Ui::ColorClip_UI();
    dia_ui->setupUi(dia);
    dia_ui->clip_name->setText(i18n("Color Clip"));
    dia_ui->clip_duration->setText(KdenliveSettings::color_duration());
    if (dia->exec() == QDialog::Accepted) {
        QString color = dia_ui->clip_color->color().name();
        color = color.replace(0, 1, "0x") + "ff";

        QString group = QString();
        QString groupId = QString();
        ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
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

        m_doc->slotAddColorClipFile(dia_ui->clip_name->text(), color, dia_ui->clip_duration->text(), group, groupId);
    }
    delete dia_ui;
    delete dia;
}


void ProjectList::slotAddSlideshowClip() {
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!!  NO CMD STK";
    SlideshowClip *dia = new SlideshowClip(this);

    if (dia->exec() == QDialog::Accepted) {

        QString group = QString();
        QString groupId = QString();
        ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
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

        m_doc->slotAddSlideshowClipFile(dia->clipName(), dia->selectedPath(), dia->imageCount(), dia->clipDuration(), dia->loop(), dia->fade(), dia->lumaDuration(), dia->lumaFile(), dia->softness(), group, groupId);
    }
    delete dia;
}

void ProjectList::slotAddTitleClip() {
    QString group = QString();
    QString groupId = QString();
    ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
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

void ProjectList::setDocument(KdenliveDoc *doc) {
    listView->clear();
    m_thumbnailQueue.clear();
    m_infoQueue.clear();
    m_refreshed = false;
    QList <DocClipBase*> list = doc->clipManager()->documentClipList();
    listView->blockSignals(true);
    for (int i = 0; i < list.count(); i++) {
        slotAddClip(list.at(i), false);
    }

    m_fps = doc->fps();
    m_timecode = doc->timecode();
    m_commandStack = doc->commandStack();
    m_doc = doc;
    QTreeWidgetItem *first = listView->topLevelItem(0);
    if (first) listView->setCurrentItem(first);
    listView->blockSignals(false);
    m_toolbar->setEnabled(true);
}

QDomElement ProjectList::producersList() {
    QDomDocument doc;
    QDomElement prods = doc.createElement("producerlist");
    doc.appendChild(prods);
    kDebug() << "////////////  PRO LIST BUILD PRDSLIST ";
    QTreeWidgetItemIterator it(listView);
    while (*it) {
        if (!((ProjectItem *)(*it))->isGroup())
            prods.appendChild(doc.importNode(((ProjectItem *)(*it))->toXml(), true));
        ++it;
    }
    return prods;
}

void ProjectList::slotCheckForEmptyQueue() {
    if (!m_refreshed && m_thumbnailQueue.isEmpty() && m_infoQueue.isEmpty()) {
        m_refreshed = true;
        emit loadingIsOver();
    } else QTimer::singleShot(500, this, SLOT(slotCheckForEmptyQueue()));
}

void ProjectList::requestClipThumbnail(const QString &id) {
    m_thumbnailQueue.append(id);
    if (m_thumbnailQueue.count() == 1) QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
}

void ProjectList::slotProcessNextThumbnail() {
    if (m_thumbnailQueue.isEmpty()) {
        return;
    }
    slotRefreshClipThumbnail(m_thumbnailQueue.takeFirst(), false);
}

void ProjectList::slotRefreshClipThumbnail(const QString &clipId, bool update) {
    ProjectItem *item = getItemById(clipId);
    if (item) slotRefreshClipThumbnail(item, update);
    else slotProcessNextThumbnail();
}

void ProjectList::slotRefreshClipThumbnail(ProjectItem *item, bool update) {
    if (item) {
        if (!item->referencedClip()) return;
        int height = 50;
        int width = (int)(height  * m_render->dar());
        QPixmap pix = item->referencedClip()->thumbProducer()->extractImage(item->referencedClip()->getClipThumbFrame(), width, height);
        item->setIcon(0, pix);
        if (update) emit projectModified();
        if (!m_thumbnailQueue.isEmpty()) QTimer::singleShot(300, this, SLOT(slotProcessNextThumbnail()));
    }
}

void ProjectList::slotReplyGetFileProperties(const QString &clipId, Mlt::Producer *producer, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata) {
    ProjectItem *item = getItemById(clipId);
    if (item && producer) {
        item->setProperties(properties, metadata);
        item->referencedClip()->setProducer(producer);
        emit receivedClipDuration(clipId, item->clipMaxDuration());
    } else kDebug() << "////////  COULD NOT FIND CLIP TO UPDATE PRPS...";
    if (!m_infoQueue.isEmpty()) QTimer::singleShot(300, this, SLOT(slotProcessNextClipInQueue()));
}

void ProjectList::slotReplyGetImage(const QString &clipId, int pos, const QPixmap &pix, int w, int h) {
    ProjectItem *item = getItemById(clipId);
    if (item) item->setIcon(0, pix);
}

ProjectItem *ProjectList::getItemById(const QString &id) {
    QTreeWidgetItemIterator it(listView);
    while (*it) {
        if (((ProjectItem *)(*it))->clipId() == id)
            break;
        ++it;
    }
    if (*it) return ((ProjectItem *)(*it));
    return NULL;
}

void ProjectList::slotSelectClip(const QString &ix) {
    ProjectItem *p = getItemById(ix);
    if (p) {
        listView->setCurrentItem(p);
        listView->scrollToItem(p);
        m_editAction->setEnabled(true);
        m_deleteAction->setEnabled(true);
    }
}

#include "projectlist.moc"
