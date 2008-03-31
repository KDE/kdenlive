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
#include "ui_colorclip_ui.h"

#include "definitions.h"
#include "titlewidget.h"
#include "clipmanager.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "projectlistview.h"
#include <QtGui>

ProjectList::ProjectList(QWidget *parent)
        : QWidget(parent), m_render(NULL), m_fps(-1), m_commandStack(NULL) {

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

    QAction *addClipButton = addMenu->addAction(KIcon("document-new"), i18n("Add Clip"));
    connect(addClipButton, SIGNAL(triggered()), this, SLOT(slotAddClip()));

    QAction *addColorClip = addMenu->addAction(KIcon("document-new"), i18n("Add Color Clip"));
    connect(addColorClip, SIGNAL(triggered()), this, SLOT(slotAddColorClip()));

    QAction *addTitleClip = addMenu->addAction(KIcon("document-new"), i18n("Add Title Clip"));
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
    m_menu->addAction(addTitleClip);
    m_menu->addAction(m_editAction);
    m_menu->addAction(m_deleteAction);
    m_menu->addAction(addFolderButton);
    m_menu->insertSeparator(m_deleteAction);

    connect(listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
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



void ProjectList::setRenderer(Render *projectRender) {
    m_render = projectRender;
}

void ProjectList::slotClipSelected() {
    ProjectItem *item = static_cast <ProjectItem*>(listView->currentItem());
    if (item && !item->isGroup()) emit clipSelected(item->toXml());
}

void ProjectList::slotUpdateClipProperties(int id, QMap <QString, QString> properties) {
    ProjectItem *item = getItemById(id);
    if (item) slotUpdateClipProperties(item, properties);
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

void ProjectList::addClip(const QStringList &name, const QDomElement &elem, const int clipId, const KUrl &url, const QString &group, int parentId) {
    kDebug() << "/////////  ADDING VCLIP=: " << name;
    ProjectItem *item;
    ProjectItem *groupItem = NULL;
    QString groupName;
    if (group.isEmpty()) groupName = elem.attribute("groupname", QString::null);
    else groupName = group;
    if (elem.isNull() && url.isEmpty()) {
        // this is a folder
        groupName = name.at(1);
        QList<QTreeWidgetItem *> groupList = listView->findItems(groupName, Qt::MatchExactly, 1);
        if (groupList.isEmpty())  {
            (void) new ProjectItem(listView, name, m_doc->getFreeClipId());
        }
        return;
    }

    if (parentId != -1) {
        groupItem = getItemById(parentId);
    } else if (!groupName.isEmpty()) {
        // Clip is in a group
        QList<QTreeWidgetItem *> groupList = listView->findItems(groupName, Qt::MatchExactly, 1);

        if (groupList.isEmpty())  {
            QStringList itemName;
            itemName << QString::null << groupName;
            kDebug() << "-------  CREATING NEW GRP: " << itemName;
            groupItem = new ProjectItem(listView, itemName, m_doc->getFreeClipId());
        } else groupItem = (ProjectItem *) groupList.first();
    }
    if (groupItem) item = new ProjectItem(groupItem, name, elem, clipId);
    else item = new ProjectItem(listView, name, elem, clipId);
    if (!url.isEmpty()) {
        // if file has Nepomuk comment, use it
        Nepomuk::Resource f(url.path());
        QString annotation;
        if (f.isValid()) annotation = f.description();

        if (!annotation.isEmpty()) item->setText(2, annotation);
        QString resource = url.path();
        if (resource.endsWith("westley") || resource.endsWith("kdenlive")) {
            QString tmpfile;
            QDomDocument doc;
            if (KIO::NetAccess::download(url, tmpfile, 0)) {
                QFile file(tmpfile);
                if (file.open(QIODevice::ReadOnly)) {
                    doc.setContent(&file, false);
                    file.close();
                }
                KIO::NetAccess::removeTempFile(tmpfile);

                QDomNodeList subProds = doc.elementsByTagName("producer");
                int ct = subProds.count();
                for (int i = 0; i <  ct ; i++) {
                    QDomElement e = subProds.item(i).toElement();
                    if (!e.isNull()) {
                        addProducer(e, clipId);
                    }
                }
            }
        }

    }

    if (elem.isNull()) {
        QDomDocument doc;
        QDomElement element = doc.createElement("producer");
        element.setAttribute("resource", url.path());
        emit getFileProperties(element, clipId);
    } else emit getFileProperties(elem, clipId);
    selectItemById(clipId);
}

void ProjectList::slotDeleteClip(int clipId) {
    ProjectItem *item = getItemById(clipId);
    QTreeWidgetItem *p = item->parent();
    if (p) {
        kDebug() << "///////  DELETEED CLIP HAS A PARENT... " << p->indexOfChild(item);
        QTreeWidgetItem *clone = p->takeChild(p->indexOfChild(item));
    } else if (item) delete item;
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
    if (givenUrl.isEmpty())
        list = KFileDialog::getOpenUrls(KUrl(), "application/vnd.kde.kdenlive application/vnd.westley.scenelist application/flv application/vnd.rn-realmedia video/x-dv video/x-msvideo video/mpeg video/x-ms-wmv audio/mpeg audio/x-mp3 audio/x-wav application/ogg *.m2t *.dv video/mp4 video/quicktime image/gif image/jpeg image/png image/x-bmp image/svg+xml image/tiff image/x-xcf-gimp image/x-vnd.adobe.photoshop image/x-pcx image/x-exr");
    else list.append(givenUrl);
    if (list.isEmpty()) return;
    KUrl::List::Iterator it;
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
    for (it = list.begin(); it != list.end(); it++) {
        m_doc->slotAddClipFile(*it, group, groupId);
    }
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

void ProjectList::slotAddTitleClip() {

    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!!  NO CMD STK";
    //QDialog *dia = new QDialog;

    TitleWidget *dia_ui = new TitleWidget(m_render, this);
    //dia_ui->setupUi(dia);
    //dia_ui->clip_name->setText(i18n("Title Clip"));
    //dia_ui->clip_duration->setText(KdenliveSettings::color_duration());
    if (dia_ui->exec() == QDialog::Accepted) {
        //QString color = dia_ui->clip_color->color().name();
        //color = color.replace(0, 1, "0x") + "ff";
        //m_doc->slotAddColorClipFile(dia_ui->clip_name->text(), color, dia_ui->clip_duration->text(), QString::null);
    }
    delete dia_ui;
    //delete dia;
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
    /*    QDomNodeList prods = doc->producersList();
        int ct = prods.count();
        kDebug() << "////////////  SETTING DOC, FOUND CLIPS: " << prods.count();
        listView->clear();
        for (int i = 0; i <  ct ; i++) {
            QDomElement e = prods.item(i).toElement();
            kDebug() << "// IMPORT: " << i << ", :" << e.attribute("id", "non") << ", NAME: " << e.attribute("name", "non");
            if (!e.isNull()) addProducer(e);
        }*/
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


void ProjectList::slotReplyGetFileProperties(int clipId, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata) {
    ProjectItem *item = getItemById(clipId);
    if (item) {
        item->setProperties(properties, metadata);
        listView->setCurrentItem(item);
        emit receivedClipDuration(clipId, item->clipMaxDuration());
    }
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


void ProjectList::addProducer(QDomElement producer, int parentId) {
    if (!m_commandStack) kDebug() << "!!!!!!!!!!!!!!!!  NO CMD STK";
    CLIPTYPE type = (CLIPTYPE) producer.attribute("type").toInt();

    /*QDomDocument doc;
    QDomElement prods = doc.createElement("list");
    doc.appendChild(prods);
    prods.appendChild(doc.importNode(producer, true));*/


    //kDebug()<<"//////  ADDING PRODUCER:\n "<<doc.toString()<<"\n+++++++++++++++++";
    int id = producer.attribute("id").toInt();
    QString groupName = producer.attribute("groupname");
    if (id >= m_clipIdCounter) m_clipIdCounter = id + 1;
    else if (id == 0) id = m_clipIdCounter++;

    if (parentId != -1) {
        // item is a westley playlist, adjust subproducers ids
        id = (parentId + 1) * 10000 + id;
    }
    if (type == AUDIO || type == VIDEO || type == AV || type == IMAGE  || type == PLAYLIST) {
        KUrl resource = KUrl(producer.attribute("resource"));
        if (!resource.isEmpty()) {
            QStringList itemEntry;
            itemEntry.append(QString::null);
            itemEntry.append(resource.fileName());
            addClip(itemEntry, producer, id, resource, groupName, parentId);
        }
    } else if (type == COLOR) {
        QString colour = producer.attribute("colour");
        QPixmap pix(60, 40);
        colour = colour.replace(0, 2, "#");
        pix.fill(QColor(colour.left(7)));
        QStringList itemEntry;
        itemEntry.append(QString::null);
        itemEntry.append(producer.attribute("name", i18n("Color clip")));
        addClip(itemEntry, producer, id, KUrl(), groupName, parentId);
    }

}

#include "projectlist.moc"
