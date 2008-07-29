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
#include <kio/netaccess.h>
#include <KMessageBox>

#include <nepomuk/global.h>
#include <nepomuk/resource.h>
#include <nepomuk/tag.h>

#include "projectlist.h"
#include "projectitem.h"
#include "kdenlivesettings.h"
#include "slideshowclip.h"
#include "ui_colorclip_ui.h"


#include "definitions.h"
#include "clipmanager.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "kthumb.h"
#include "projectlistview.h"

ProjectList::ProjectList(QWidget *parent)
        : QWidget(parent), m_render(NULL), m_fps(-1), m_commandStack(NULL), m_selectedItem(NULL) {

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
    if (item && !item->isGroup()) emit clipSelected(item->referencedClip());
    emit showClipProperties(item->referencedClip());
}



void ProjectList::setRenderer(Render *projectRender) {
    m_render = projectRender;
}

void ProjectList::slotClipSelected() {
    ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
    if (item && !item->isGroup()) {
        m_selectedItem = item;
        emit clipSelected(item->referencedClip());
    }
}

void ProjectList::slotUpdateClipProperties(int id, QMap <QString, QString> properties) {
    ProjectItem *item = getItemById(id);
    if (item) {
        slotUpdateClipProperties(item, properties);
        if (properties.contains("colour") || properties.contains("resource")) slotRefreshClipThumbnail(item);
        if (properties.contains("out")) item->changeDuration(properties.value("out").toInt());
    }
}

void ProjectList::slotUpdateClipProperties(ProjectItem *clip, QMap <QString, QString> properties) {
    if (!clip) return;
    clip->setProperties(properties);
    if (properties.contains("description")) {
        CLIPTYPE type = clip->clipType();
        clip->setText(2, properties.value("description"));
        if (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST) {
            // Use Nepomuk system to store clip description
            Nepomuk::Resource f(clip->clipUrl().path());
            if (f.isValid()) f.setDescription(properties.value("description"));
        }
    }
}

void ProjectList::slotItemEdited(QTreeWidgetItem *item, int column) {
    ProjectItem *clip = static_cast <ProjectItem*>(item);
    if (column == 2) {
        QMap <QString, QString> props;
        props["description"] = item->text(2);
        slotUpdateClipProperties(clip, props);
    } else if (column == 1 && clip->clipType() == FOLDER) {
        m_doc->slotEditFolder(item->text(1), clip->groupName(), clip->clipId());
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
    ProjectItem *item = static_cast <ProjectItem *>(listView->currentItem());
    QList <int> ids;
    QMap <QString, int> folderids;
    if (item->clipType() == FOLDER) folderids[item->groupName()] = item->clipId();
    else ids << item->clipId();
    if (item->numReferences() > 0) {
        if (KMessageBox::questionYesNo(this, i18np("Delete clip <b>%2</b> ?<br>This will also remove the clip in timeline", "Delete clip <b>%2</b> ?<br>This will also remove its %1 clips in timeline", item->numReferences(), item->names().at(1)), i18n("Delete Clip")) != KMessageBox::Yes) return;
    } else if (item->clipType() == FOLDER && item->childCount() > 0) {
        int children = item->childCount();
        if (KMessageBox::questionYesNo(this, i18n("Delete folder <b>%2</b> ?<br>This will also remove the %1 clips in that folder", children, item->names().at(1)), i18n("Delete Folder")) != KMessageBox::Yes) return;
        for (int i = 0; i < children; ++i) {
            ProjectItem *child = static_cast <ProjectItem *>(item->child(i));
            ids << child->clipId();
        }
    }
    if (!ids.isEmpty()) m_doc->deleteProjectClip(ids);
    if (!folderids.isEmpty()) m_doc->deleteProjectFolder(folderids);
}

void ProjectList::selectItemById(const int clipId) {
    ProjectItem *item = getItemById(clipId);
    if (item) listView->setCurrentItem(item);
}


void ProjectList::slotDeleteClip(int clipId) {
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

void ProjectList::slotAddFolder(const QString foldername, int clipId, bool remove, bool edit) {
    if (remove) {
        ProjectItem *item;
        QTreeWidgetItemIterator it(listView);
        while (*it) {
            item = static_cast <ProjectItem *>(*it);
            if (item->clipType() == FOLDER && item->clipId() == clipId) {
                delete item;
                break;
            }
            ++it;
        }
    } else {
        if (edit) {
            disconnect(listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotUpdateItemDescription(QTreeWidgetItem *, int)));
            ProjectItem *item;
            QTreeWidgetItemIterator it(listView);
            while (*it) {
                item = static_cast <ProjectItem *>(*it);
                if (item->clipType() == FOLDER && item->clipId() == clipId) {
                    item->setText(1, foldername);
                    break;
                }
                ++it;
            }
            connect(listView, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotUpdateItemDescription(QTreeWidgetItem *, int)));
        } else {
            QStringList text;
            text << QString() << foldername;
            (void) new ProjectItem(listView, text, clipId);
        }
    }
}

void ProjectList::slotAddClip(DocClipBase *clip) {
    const int parent = clip->toXML().attribute("groupid").toInt();
    ProjectItem *item = NULL;
    if (parent != 0) {
        ProjectItem *parentitem = getItemById(parent);
        if (parentitem) item = new ProjectItem(parentitem, clip);
    }
    if (item == NULL) item = new ProjectItem(listView, clip);

    KUrl url = clip->fileURL();
    if (!url.isEmpty()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation;
        if (f.isValid()) {
            annotation = f.description();
            /*
            Nepomuk::Tag tag("test");
            f.addTag(tag);*/
        } else kDebug() << "---  CANNOT CONTACT NEPOMUK";
        if (!annotation.isEmpty()) item->setText(2, annotation);
    }
    emit getFileProperties(clip->toXML(), clip->getId());
}

void ProjectList::slotUpdateClip(int id) {
    ProjectItem *item = getItemById(id);
    item->setData(1, UsageRole, QString::number(item->numReferences()));
}

void ProjectList::slotAddClip(QUrl givenUrl, QString group) {
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!!  NO CMD STK";
    KUrl::List list;
    if (givenUrl.isEmpty()) {
        list = KFileDialog::getOpenUrls(KUrl("kfiledialog:///clipfolder"), "application/vnd.kde.kdenlive application/vnd.westley.scenelist application/flv application/vnd.rn-realmedia video/x-dv video/x-msvideo video/mpeg video/x-ms-wmv audio/mpeg audio/x-mp3 audio/x-wav application/ogg video/mp4 video/quicktime image/gif image/jpeg image/png image/x-bmp image/svg+xml image/tiff image/x-xcf-gimp image/x-vnd.adobe.photoshop image/x-pcx image/x-exr\n*.m2t *.mts|HDV video\n*.dv|DV video");
    } else list.append(givenUrl);
    if (list.isEmpty()) return;

    int groupId = -1;
    if (group.isEmpty()) {
        ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
        if (item && item->clipType() != FOLDER) {
            while (item->parent()) {
                item = static_cast <ProjectItem*>(item->parent());
                if (item->clipType() == FOLDER) break;
            }
        }
        if (item && item->clipType() == FOLDER) {
            group = item->groupName();
            groupId = item->clipId();
        }
    }
    foreach(const KUrl file, list) {
	if (KIO::NetAccess::exists(file, KIO::NetAccess::SourceSide, NULL))
	    m_doc->slotAddClipFile(file, group, groupId);
    }
}

void ProjectList::slotRemoveInvalidClip(int id) {
    ProjectItem *item = getItemById(id);
    if (item) {
        QString path = item->referencedClip()->fileURL().path();
	if (!path.isEmpty()) KMessageBox::sorry(this, i18n("<qt>Clip <b>%1</b><br>is invalid, will be removed from project.", path));
	
    }
    QList <int> ids;
    ids << id;
    m_doc->deleteProjectClip(ids);
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
        int groupId = -1;
        ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
        if (item && item->clipType() != FOLDER) {
            while (item->parent()) {
                item = static_cast <ProjectItem*>(item->parent());
                if (item->clipType() == FOLDER) break;
            }
        }
        if (item && item->clipType() == FOLDER) {
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
        int groupId = -1;
        ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
        if (item && item->clipType() != FOLDER) {
            while (item->parent()) {
                item = static_cast <ProjectItem*>(item->parent());
                if (item->clipType() == FOLDER) break;
            }
        }
        if (item && item->clipType() == FOLDER) {
            group = item->groupName();
            groupId = item->clipId();
        }

        m_doc->slotAddSlideshowClipFile(dia->clipName(), dia->selectedPath(), dia->imageCount(), dia->clipDuration(), dia->loop(), dia->fade(), dia->lumaDuration(), dia->lumaFile(), dia->softness(), group, groupId);
    }
    delete dia;
}

void ProjectList::slotAddTitleClip() {
    QString group = QString();
    int groupId = -1;
    ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
    if (item && item->clipType() != FOLDER) {
        while (item->parent()) {
            item = static_cast <ProjectItem*>(item->parent());
            if (item->clipType() == FOLDER) break;
        }
    }
    if (item && item->clipType() == FOLDER) {
        group = item->groupName();
        groupId = item->clipId();
    }

    m_doc->slotCreateTextClip(group, groupId);
}

void ProjectList::setDocument(KdenliveDoc *doc) {
    listView->clear();
    QList <DocClipBase*> list = doc->clipManager()->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        slotAddClip(list.at(i));
    }

    m_fps = doc->fps();
    m_timecode = doc->timecode();
    m_commandStack = doc->commandStack();
    m_doc = doc;
    QTreeWidgetItem *first = listView->topLevelItem(0);
    if (first) listView->setCurrentItem(first);
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

void ProjectList::slotRefreshClipThumbnail(int clipId) {
    ProjectItem *item = getItemById(clipId);
    if (item) slotRefreshClipThumbnail(item);
}

void ProjectList::slotRefreshClipThumbnail(ProjectItem *item) {
    if (item) {
        int height = 50;
        int width = (int)(height  * m_render->dar());
        QPixmap pix = KThumb::getImage(item->toXml(), item->referencedClip()->getClipThumbFrame(), width, height);
        item->setIcon(0, pix);
    }
}

void ProjectList::slotReplyGetFileProperties(int clipId, Mlt::Producer *producer, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata) {
    ProjectItem *item = getItemById(clipId);
    if (item) {
        item->setProperties(properties, metadata);
        item->referencedClip()->setProducer(producer);
        listView->setCurrentItem(item);
        emit receivedClipDuration(clipId, item->clipMaxDuration());
    } else kDebug() << "////////  COULD NOT FIND CLIP TO UPDATE PRPS...";
}

void ProjectList::slotReplyGetImage(int clipId, int pos, const QPixmap &pix, int w, int h) {
    ProjectItem *item = getItemById(clipId);
    if (item) item->setIcon(0, pix);
}

ProjectItem *ProjectList::getItemById(int id) {
    QTreeWidgetItemIterator it(listView);
    while (*it) {
        if (((ProjectItem *)(*it))->clipId() == id)
            break;
        ++it;
    }
    if (*it) return ((ProjectItem *)(*it));
    return NULL;
}

#include "projectlist.moc"
