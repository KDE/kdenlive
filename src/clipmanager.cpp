/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "clipmanager.h"
#include "addclipcommand.h"
#include "kdenlivesettings.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "abstractclipitem.h"
#include "abstractgroupitem.h"

#include <mlt++/Mlt.h>

#include <KDebug>
#include <KFileDialog>
#include <kio/netaccess.h>

#include <QGraphicsItemGroup>

ClipManager::ClipManager(KdenliveDoc *doc) :
        QObject(),
        m_audioThumbsQueue(),
        m_doc(doc),
        m_audioThumbsEnabled(false),
        m_generatingAudioId()
{
    m_clipIdCounter = 1;
    m_folderIdCounter = 1;
    connect(&m_fileWatcher, SIGNAL(dirty(const QString &)), this, SLOT(slotClipModified(const QString &)));
}

ClipManager::~ClipManager()
{
    kDebug() << "\n\n 2222222222222222222222222  CLOSE CM 22222222222";
    qDeleteAll(m_clipList);
}

void ClipManager::clear()
{
    qDeleteAll(m_clipList);
    m_clipList.clear();
    m_clipIdCounter = 1;
    m_folderIdCounter = 1;
    m_folderList.clear();
    m_audioThumbsQueue.clear();
}

void ClipManager::checkAudioThumbs()
{
    if (m_audioThumbsEnabled == KdenliveSettings::audiothumbnails()) return;
    m_audioThumbsEnabled = KdenliveSettings::audiothumbnails();
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_audioThumbsEnabled) m_audioThumbsQueue.append(m_clipList.at(i)->getId());
        else m_clipList.at(i)->slotClearAudioCache();
    }
    if (m_audioThumbsEnabled) {
        if (m_generatingAudioId.isEmpty()) startAudioThumbsGeneration();
    } else {
        m_audioThumbsQueue.clear();
        m_generatingAudioId.clear();
    }
}

void ClipManager::askForAudioThumb(const QString &id)
{
    DocClipBase *clip = getClipById(id);
    if (clip && KdenliveSettings::audiothumbnails()) {
        m_audioThumbsQueue.append(id);
        if (m_generatingAudioId.isEmpty()) startAudioThumbsGeneration();
    }
}

void ClipManager::startAudioThumbsGeneration()
{
    if (!KdenliveSettings::audiothumbnails()) {
        m_audioThumbsQueue.clear();
        m_generatingAudioId.clear();
        return;
    }
    if (!m_audioThumbsQueue.isEmpty()) {
        m_generatingAudioId = m_audioThumbsQueue.takeFirst();
        DocClipBase *clip = getClipById(m_generatingAudioId);
        if (!clip || !clip->slotGetAudioThumbs())
            endAudioThumbsGeneration(m_generatingAudioId);
    } else {
        m_generatingAudioId.clear();
    }
}

void ClipManager::endAudioThumbsGeneration(const QString &requestedId)
{
    if (!KdenliveSettings::audiothumbnails()) {
        m_audioThumbsQueue.clear();
        m_generatingAudioId.clear();
        return;
    }
    if (!m_audioThumbsQueue.isEmpty()) {
        if (m_generatingAudioId == requestedId) {
            startAudioThumbsGeneration();
        }
    } else {
        m_generatingAudioId.clear();
    }
}

void ClipManager::setThumbsProgress(const QString &message, int progress)
{
    m_doc->setThumbsProgress(message, progress);
}

QList <DocClipBase*> ClipManager::documentClipList() const
{
    return m_clipList;
}

QMap <QString, QString> ClipManager::documentFolderList() const
{
    return m_folderList;
}

void ClipManager::addClip(DocClipBase *clip)
{
    m_clipList.append(clip);
    if (clip->clipType() == IMAGE || clip->clipType() == AUDIO || (clip->clipType() == TEXT && !clip->fileURL().isEmpty())) {
        // listen for file change
        m_fileWatcher.addFile(clip->fileURL().path());
    }
    const QString id = clip->getId();
    if (id.toInt() >= m_clipIdCounter) m_clipIdCounter = id.toInt() + 1;
    const QString gid = clip->getProperty("groupid");
    if (!gid.isEmpty() && gid.toInt() >= m_folderIdCounter) m_folderIdCounter = gid.toInt() + 1;
}

void ClipManager::slotDeleteClips(QStringList ids)
{
    QUndoCommand *delClips = new QUndoCommand();
    delClips->setText(i18np("Delete clip", "Delete clips", ids.size()));

    for (int i = 0; i < ids.size(); i++) {
        DocClipBase *clip = getClipById(ids.at(i));
        if (clip) {
            new AddClipCommand(m_doc, clip->toXML(), ids.at(i), false, delClips);
        }
    }
    m_doc->commandStack()->push(delClips);
}

void ClipManager::deleteClip(const QString &clipId)
{
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            if (m_clipList.at(i)->clipType() == IMAGE || m_clipList.at(i)->clipType() == AUDIO || (m_clipList.at(i)->clipType() == TEXT && !m_clipList.at(i)->fileURL().isEmpty())) {
                // listen for file change
                m_fileWatcher.removeFile(m_clipList.at(i)->fileURL().path());
            }
            delete m_clipList.takeAt(i);
            break;
        }
    }
}

DocClipBase *ClipManager::getClipAt(int pos)
{
    return m_clipList.at(pos);
}

DocClipBase *ClipManager::getClipById(QString clipId)
{
    //kDebug() << "++++  CLIP MAN, LOOKING FOR CLIP ID: " << clipId;
    clipId = clipId.section('_', 0, 0);
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->getId() == clipId) {
            //kDebug() << "++++  CLIP MAN, FOUND FOR CLIP ID: " << clipId;
            return m_clipList.at(i);
        }
    }
    return NULL;
}

const QList <DocClipBase *> ClipManager::getClipByResource(QString resource)
{
    QList <DocClipBase *> list;
    QString clipResource;
    for (int i = 0; i < m_clipList.count(); i++) {
        clipResource = m_clipList.at(i)->getProperty("resource");
        if (clipResource.isEmpty()) clipResource = m_clipList.at(i)->getProperty("colour");
        if (clipResource == resource) {
            list.append(m_clipList.at(i));
        }
    }
    return list;
}

void ClipManager::updatePreviewSettings()
{
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->clipType() == AV || m_clipList.at(i)->clipType() == VIDEO) {
            if (m_clipList.at(i)->producerProperty("meta.media.0.codec.name") && strcmp(m_clipList.at(i)->producerProperty("meta.media.0.codec.name"), "h264") == 0) {
                if (KdenliveSettings::dropbframes()) {
                    m_clipList[i]->setProducerProperty("skip_loop_filter", "all");
                    m_clipList[i]->setProducerProperty("skip_frame", "bidir");
                } else {
                    m_clipList[i]->setProducerProperty("skip_loop_filter", "");
                    m_clipList[i]->setProducerProperty("skip_frame", "");
                }
            }
        }
    }
}

void ClipManager::clearUnusedProducers()
{
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->numReferences() == 0) m_clipList.at(i)->deleteProducers();
    }
}

void ClipManager::resetProducersList(const QList <Mlt::Producer *> prods)
{
    for (int i = 0; i < m_clipList.count(); i++) {
        if (m_clipList.at(i)->numReferences() > 0) {
            m_clipList.at(i)->clearProducers();
        }
    }
    QString id;
    for (int i = 0; i < prods.count(); i++) {
        id = prods.at(i)->get("id");
        kDebug() << "// // // REPLACE CLIP: " << id;
        if (id.contains('_')) id = id.section('_', 0, 0);
        DocClipBase *clip = getClipById(id);
        if (clip) {
            clip->setProducer(prods.at(i));
        }
    }
    emit checkAllClips();
}

void ClipManager::slotAddClipList(const KUrl::List urls, const QString group, const QString &groupId)
{
    QUndoCommand *addClips = new QUndoCommand();
    addClips->setText(i18n("Add clips"));

    foreach(const KUrl &file, urls) {
        if (KIO::NetAccess::exists(file, KIO::NetAccess::SourceSide, NULL)) {
            QDomDocument doc;
            QDomElement prod = doc.createElement("producer");
            doc.appendChild(prod);
            if (!group.isEmpty()) {
                prod.setAttribute("groupname", group);
                prod.setAttribute("groupid", groupId);
            }
            prod.setAttribute("resource", file.path());
            uint id = m_clipIdCounter++;
            prod.setAttribute("id", QString::number(id));
            KMimeType::Ptr type = KMimeType::findByUrl(file);
            if (type->name().startsWith("image/")) {
                prod.setAttribute("type", (int) IMAGE);
                prod.setAttribute("in", 0);
                prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()) - 1);
            } else if (type->name() == "application/x-kdenlivetitle") {
                // opening a title file
                QDomDocument txtdoc("titledocument");
                QFile txtfile(file.path());
                if (txtfile.open(QIODevice::ReadOnly) && txtdoc.setContent(&txtfile)) {
                    txtfile.close();
                    prod.setAttribute("type", (int) TEXT);
                    prod.setAttribute("resource", file.path());
                    prod.setAttribute("xmldata", txtdoc.toString());
                    prod.setAttribute("transparency", 1);
                    prod.setAttribute("in", 0);
                    int out = txtdoc.documentElement().attribute("out").toInt();
                    if (out > 0) prod.setAttribute("out", out);
                } else txtfile.close();
            }
            new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true, addClips);
        }
    }
    m_doc->commandStack()->push(addClips);
}

void ClipManager::slotAddClipFile(const KUrl url, const QString group, const QString &groupId)
{
    kDebug() << "/////  CLIP MANAGER, ADDING CLIP: " << url;
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("resource", url.path());
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    KMimeType::Ptr type = KMimeType::findByUrl(url);
    if (type->name().startsWith("image/")) {
        prod.setAttribute("type", (int) IMAGE);
        prod.setAttribute("in", "0");
        prod.setAttribute("out", m_doc->getFramePos(KdenliveSettings::image_duration()) - 1);
    } else if (type->name() == "application/x-kdenlivetitle") {
        // opening a title file
        QDomDocument txtdoc("titledocument");
        QFile txtfile(url.path());
        if (txtfile.open(QIODevice::ReadOnly) && txtdoc.setContent(&txtfile)) {
            txtfile.close();
            prod.setAttribute("type", (int) TEXT);
            prod.setAttribute("resource", QString());
            prod.setAttribute("xmldata", txtdoc.toString());
            GenTime outPos(txtdoc.documentElement().attribute("out").toDouble() / 1000.0);
            prod.setAttribute("transparency", 1);
            prod.setAttribute("in", 0);
            int out = (int) outPos.frames(m_doc->fps());
            if (out > 0) prod.setAttribute("out", out);
        } else txtfile.close();
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("mlt_service", "colour");
    prod.setAttribute("colour", color);
    prod.setAttribute("type", (int) COLOR);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(duration) - 1);
    prod.setAttribute("name", name);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddSlideshowClipFile(const QString name, const QString path, int count, const QString duration, const bool loop, const bool fade, const QString &luma_duration, const QString &luma_file, const int softness, QString group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("resource", path);
    prod.setAttribute("type", (int) SLIDESHOW);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    prod.setAttribute("in", "0");
    prod.setAttribute("out", m_doc->getFramePos(duration) * count - 1);
    prod.setAttribute("ttl", m_doc->getFramePos(duration));
    prod.setAttribute("luma_duration", m_doc->getFramePos(luma_duration));
    prod.setAttribute("name", name);
    prod.setAttribute("loop", loop);
    prod.setAttribute("fade", fade);
    prod.setAttribute("softness", QString::number(softness));
    prod.setAttribute("luma_file", luma_file);
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}



void ClipManager::slotAddTextClipFile(const QString titleName, int out, const QString xml, const QString group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    //prod.setAttribute("resource", imagePath);
    prod.setAttribute("name", titleName);
    prod.setAttribute("xmldata", xml);
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    prod.setAttribute("type", (int) TEXT);
    prod.setAttribute("transparency", "1");
    prod.setAttribute("in", "0");
    prod.setAttribute("out", out);
    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

void ClipManager::slotAddTextTemplateClip(QString titleName, const KUrl path, const QString group, const QString &groupId)
{
    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    doc.appendChild(prod);
    prod.setAttribute("name", titleName);
    prod.setAttribute("resource", path.path());
    uint id = m_clipIdCounter++;
    prod.setAttribute("id", QString::number(id));
    if (!group.isEmpty()) {
        prod.setAttribute("groupname", group);
        prod.setAttribute("groupid", groupId);
    }
    prod.setAttribute("type", (int) TEXT);
    prod.setAttribute("transparency", "1");
    prod.setAttribute("in", "0");

    int out = 0;
    QDomDocument titledoc;
    QFile txtfile(path.path());
    if (txtfile.open(QIODevice::ReadOnly) && titledoc.setContent(&txtfile)) {
        txtfile.close();
        out = titledoc.documentElement().attribute("out").toInt();
    } else txtfile.close();

    if (out == 0) out = m_doc->getFramePos(KdenliveSettings::image_duration()) - 1;
    prod.setAttribute("out", out);

    AddClipCommand *command = new AddClipCommand(m_doc, doc.documentElement(), QString::number(id), true);
    m_doc->commandStack()->push(command);
}

int ClipManager::getFreeClipId()
{
    return m_clipIdCounter++;
}

int ClipManager::getFreeFolderId()
{
    return m_folderIdCounter++;
}

int ClipManager::lastClipId() const
{
    return m_clipIdCounter - 1;
}

QString ClipManager::projectFolder() const
{
    return m_doc->projectFolder().path();
}

void ClipManager::addFolder(const QString &id, const QString &name)
{
    m_folderList.insert(id, name);
}

void ClipManager::deleteFolder(const QString &id)
{
    m_folderList.remove(id);
}

AbstractGroupItem *ClipManager::createGroup()
{
    AbstractGroupItem *group = new AbstractGroupItem(m_doc->fps());
    m_groupsList.append(group);
    return group;
}

void ClipManager::removeGroup(AbstractGroupItem *group)
{
    m_groupsList.removeAll(group);
}

QDomElement ClipManager::groupsXml() const
{
    QDomDocument doc;
    QDomElement groups = doc.createElement("groups");
    doc.appendChild(groups);
    for (int i = 0; i < m_groupsList.count(); i++) {
        QDomElement group = doc.createElement("group");
        groups.appendChild(group);
        QList <QGraphicsItem *> children = m_groupsList.at(i)->childItems();
        for (int j = 0; j < children.count(); j++) {
            if (children.at(j)->type() == AVWIDGET || children.at(j)->type() == TRANSITIONWIDGET) {
                AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(j));
                ItemInfo info = item->info();
                if (item->type() == AVWIDGET) {
                    QDomElement clip = doc.createElement("clipitem");
                    clip.setAttribute("track", info.track);
                    clip.setAttribute("position", info.startPos.frames(m_doc->fps()));
                    group.appendChild(clip);
                } else if (item->type() == TRANSITIONWIDGET) {
                    QDomElement clip = doc.createElement("transitionitem");
                    clip.setAttribute("track", info.track);
                    clip.setAttribute("position", info.startPos.frames(m_doc->fps()));
                    group.appendChild(clip);
                }
            }
        }
    }
    return doc.documentElement();
}


void ClipManager::slotClipModified(const QString &path)
{
    //kDebug()<<"// CLIP: "<<path<<" WAS MODIFIED";
    const QList <DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip != NULL) emit reloadClip(clip->getId());
    }
}

