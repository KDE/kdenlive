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

#include <KDebug>
#include <KStandardDirs>
#include <KMessageBox>
#include <KLocale>
#include <KFileDialog>
#include <KIO/NetAccess>


#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "profilesdialog.h"
#include "kdenlivesettings.h"
#include "renderer.h"
#include "clipmanager.h"
#include "addfoldercommand.h"
#include "editfoldercommand.h"

KdenliveDoc::KdenliveDoc(const KUrl &url, MltVideoProfile profile, QUndoGroup *undoGroup, QWidget *parent): QObject(parent), m_render(NULL), m_url(url), m_profile(profile), m_fps((double)profile.frame_rate_num / profile.frame_rate_den), m_width(profile.width), m_height(profile.height), m_commandStack(new KUndoStack(undoGroup)), m_modified(false) {
    m_clipManager = new ClipManager(this);
    if (!url.isEmpty()) {
        QString tmpFile;
        if (KIO::NetAccess::download(url.path(), tmpFile, parent)) {
            QFile file(tmpFile);
            m_document.setContent(&file, false);
            file.close();
            QDomNode infoXmlNode = m_document.elementsByTagName("kdenlive").at(0);
            if (!infoXmlNode.isNull()) {
                QDomElement infoXml = infoXmlNode.toElement();
                QString profilePath = infoXml.attribute("profile");
                if (!profilePath.isEmpty()) setProfilePath(profilePath);
            }
            KIO::NetAccess::removeTempFile(tmpFile);
        } else {
            KMessageBox::error(parent, KIO::NetAccess::lastErrorString());
        }
    } else {
        // Creating new document
        QDomElement westley = m_document.createElement("westley");
        m_document.appendChild(westley);
        QDomElement doc = m_document.createElement("kdenlivedoc");
        doc.setAttribute("version", "0.6");
        westley.appendChild(doc);
        QDomElement props = m_document.createElement("properties");
        doc.setAttribute("width", m_width);
        doc.setAttribute("height", m_height);
        doc.setAttribute("projectfps", m_fps);
        doc.appendChild(props);


        /*QDomElement westley = m_document.createElement("westley");
        m_document.appendChild(westley);*/


        QDomElement tractor = m_document.createElement("tractor");
        tractor.setAttribute("id", "maintractor");
        QDomElement multitrack = m_document.createElement("multitrack");
        QDomElement playlist = m_document.createElement("playlist");
        QDomElement producer = m_document.createElement("producer");
        /*producer.setAttribute("mlt_service", "colour");
        producer.setAttribute("colour", "red");
        playlist.appendChild(producer);*/
        multitrack.appendChild(playlist);
        QDomElement playlist1 = m_document.createElement("playlist");
        playlist1.setAttribute("id", "playlist1");
        playlist1.setAttribute("hide", "video");
        multitrack.appendChild(playlist1);
        QDomElement playlist2 = m_document.createElement("playlist");
        playlist2.setAttribute("id", "playlist2");
        playlist2.setAttribute("hide", "video");
        multitrack.appendChild(playlist2);
        QDomElement playlist3 = m_document.createElement("playlist");
        multitrack.appendChild(playlist3);
        playlist3.setAttribute("id", "playlist3");
        QDomElement playlist4 = m_document.createElement("playlist");
        multitrack.appendChild(playlist4);
        playlist4.setAttribute("id", "playlist4");
        QDomElement playlist5 = m_document.createElement("playlist");
        multitrack.appendChild(playlist5);
        playlist5.setAttribute("id", "playlist5");
        tractor.appendChild(multitrack);

        for (uint i = 2; i < 6 ; i++) {
            QDomElement transition = m_document.createElement("transition");
            transition.setAttribute("in", "0");
            //TODO: Make audio mix last for all project duration
            transition.setAttribute("out", "15000");
            transition.setAttribute("a_track", QString::number(1));
            transition.setAttribute("b_track", QString::number(i));
            transition.setAttribute("mlt_service", "mix");
            transition.setAttribute("combine", "1");
            tractor.appendChild(transition);
        }
        QDomElement playlistmain = m_document.createElement("playlist");
        playlistmain.setAttribute("id", "playlistmain");
        QDomElement playentry = m_document.createElement("entry");
        playentry.setAttribute("producer", "maintractor");
        playentry.setAttribute("in", "0");
        playentry.setAttribute("out", "15000");
        playlistmain.appendChild(playentry);
        doc.appendChild(tractor);
        doc.appendChild(playlistmain);

    }
    m_scenelist = m_document.toString();
    kDebug() << "scenelist" << m_scenelist;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);
}

KdenliveDoc::~KdenliveDoc() {
    delete m_commandStack;
    delete m_clipManager;
}

QDomElement KdenliveDoc::documentInfoXml() {
    //QDomDocument doc;
    QDomElement addedXml = m_document.createElement("kdenlive");
    addedXml.setAttribute("version", "0.7");
    addedXml.setAttribute("profile", profilePath());
    kDebug() << m_document.toString();
    return m_document.documentElement();
}


ClipManager *KdenliveDoc::clipManager() {
    return m_clipManager;
}

QString KdenliveDoc::getDocumentStandard() {
    //WARNING: this way to tell the video standard is a bit hackish...
    if (m_profile.description.contains("pal", Qt::CaseInsensitive) || m_profile.description.contains("25", Qt::CaseInsensitive) || m_profile.description.contains("50", Qt::CaseInsensitive)) return "PAL";
    return "NTSC";
}

QString KdenliveDoc::profilePath() const {
    return m_profile.path;
}

void KdenliveDoc::setProfilePath(QString path) {
    KdenliveSettings::setCurrent_profile(path);
    m_profile = ProfilesDialog::getVideoProfile(path);
    m_fps = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
    m_width = m_profile.width;
    m_height = m_profile.height;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);
}

void KdenliveDoc::setThumbsProgress(KUrl url, int progress) {
    emit thumbsProgress(url, progress);
}

KUndoStack *KdenliveDoc::commandStack() {
    return m_commandStack;
}

void KdenliveDoc::setRenderer(Render *render) {
    m_render = render;
    if (m_render) m_render->setSceneList(m_scenelist);
}

Render *KdenliveDoc::renderer() {
    return m_render;
}

void KdenliveDoc::updateClip(int id) {
    emit updateClipDisplay(id);
}

int KdenliveDoc::getFramePos(QString duration) {
    return m_timecode.getFrameCount(duration, m_fps);
}

QString KdenliveDoc::producerName(int id) {
    QString result = "unnamed";
    QDomNodeList prods = producersList();
    int ct = prods.count();
    for (int i = 0; i <  ct ; i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.attribute("id") != "black" && e.attribute("id").toInt() == id) {
            result = e.attribute("name");
            if (result.isEmpty()) result = KUrl(e.attribute("resource")).fileName();
            break;
        }
    }
    return result;
}

void KdenliveDoc::setProducerDuration(int id, int duration) {
    QDomNodeList prods = producersList();
    int ct = prods.count();
    for (int i = 0; i <  ct ; i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.attribute("id") != "black" && e.attribute("id").toInt() == id) {
            e.setAttribute("duration", QString::number(duration));
            break;
        }
    }
}

int KdenliveDoc::getProducerDuration(int id) {
    int result = 0;
    QDomNodeList prods = producersList();
    int ct = prods.count();
    for (int i = 0; i <  ct ; i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.attribute("id") != "black" && e.attribute("id").toInt() == id) {
            result = e.attribute("duration").toInt();
            break;
        }
    }
    return result;
}


QDomDocument KdenliveDoc::generateSceneList() {
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement prod = doc.createElement("producer");
}

QDomDocument KdenliveDoc::toXml() const {
    return m_document;
}

Timecode KdenliveDoc::timecode() const {
    return m_timecode;
}

QDomNodeList KdenliveDoc::producersList() {
    return m_document.elementsByTagName("producer");
}

void KdenliveDoc::backupMltPlaylist() {
    if (m_render) m_scenelist = m_render->sceneList();
}

double KdenliveDoc::fps() const {
    return m_fps;
}

int KdenliveDoc::width() const {
    return m_width;
}

int KdenliveDoc::height() const {
    return m_height;
}

KUrl KdenliveDoc::url() const {
    return m_url;
}

void KdenliveDoc::setUrl(KUrl url) {
    m_url = url;
}

void KdenliveDoc::setModified(bool mod) {
    if (mod == m_modified) return;
    m_modified = mod;
    emit docModified(m_modified);
}

QString KdenliveDoc::description() const {
    if (m_url.isEmpty())
        return i18n("Untitled") + " / " + m_profile.description;
    else
        return m_url.fileName() + " / " + m_profile.description;
}

void KdenliveDoc::addClip(const QDomElement &elem, const int clipId) {
    DocClipBase *clip = new DocClipBase(m_clipManager, elem, clipId);
    kDebug() << "/////////  DOCUM, CREATING NEW CLIP, ID:" << clipId << ", PAR ID:" << elem.attribute("groupid");
    m_clipManager->addClip(clip);
    emit addProjectClip(clip);
}

void KdenliveDoc::addFolder(const QString foldername, int clipId, bool edit) {
    emit addProjectFolder(foldername, clipId, false, edit);
}

void KdenliveDoc::deleteFolder(const QString foldername, int clipId) {
    emit addProjectFolder(foldername, clipId, true);
}

void KdenliveDoc::deleteProjectClip(QList <int> ids) {
    for (int i = 0; i < ids.size(); ++i) {
        emit deletTimelineClip(ids.at(i));
        m_clipManager->slotDeleteClip(ids.at(i));
    }
    setModified(true);
}

void KdenliveDoc::deleteProjectFolder(QMap <QString, int> map) {
    QMapIterator<QString, int> i(map);
    while (i.hasNext()) {
        i.next();
        slotDeleteFolder(i.key(), i.value());
    }
    setModified(true);
}

void KdenliveDoc::deleteClip(const uint clipId) {
    emit signalDeleteProjectClip(clipId);
    m_clipManager->deleteClip(clipId);
}

void KdenliveDoc::slotAddClipFile(const KUrl url, const QString group, const int groupId) {
    kDebug() << "/////////  DOCUM, ADD CLP: " << url;
    m_clipManager->slotAddClipFile(url, group, groupId);
    setModified(true);
}

void KdenliveDoc::slotAddFolder(const QString folderName) {
    AddFolderCommand *command = new AddFolderCommand(this, folderName, m_clipManager->getFreeClipId(), true);
    commandStack()->push(command);
    setModified(true);
}

void KdenliveDoc::slotDeleteFolder(const QString folderName, const int id) {
    AddFolderCommand *command = new AddFolderCommand(this, folderName, id, false);
    commandStack()->push(command);
    setModified(true);
}

void KdenliveDoc::slotEditFolder(const QString newfolderName, const QString oldfolderName, int clipId) {
    EditFolderCommand *command = new EditFolderCommand(this, newfolderName, oldfolderName, clipId, false);
    commandStack()->push(command);
    setModified(true);
}

int KdenliveDoc::getFreeClipId() {
    return m_clipManager->getFreeClipId();
}

DocClipBase *KdenliveDoc::getBaseClip(int clipId) {
    return m_clipManager->getClipById(clipId);
}

void KdenliveDoc::slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group, const int groupId) {
    m_clipManager->slotAddColorClipFile(name, color, duration, group, groupId);
    setModified(true);
}

#include "kdenlivedoc.moc"

