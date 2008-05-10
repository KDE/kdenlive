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
#include "titlewidget.h"

KdenliveDoc::KdenliveDoc(const KUrl &url, const KUrl &projectFolder, MltVideoProfile profile, QUndoGroup *undoGroup, QWidget *parent): QObject(parent), m_render(NULL), m_url(url), m_projectFolder(projectFolder), m_profile(profile), m_fps((double)profile.frame_rate_num / profile.frame_rate_den), m_width(profile.width), m_height(profile.height), m_commandStack(new KUndoStack(undoGroup)), m_modified(false) {
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
                QDomNodeList producers = infoXmlNode.childNodes();
                QDomElement e;
                for (int i = 0; i < producers.count(); i++) {
                    e = producers.item(i).cloneNode().toElement();
                    if (!e.isNull() && e.tagName() == "kdenlive_producer") {
                        e.setTagName("producer");
                        addClip(e, e.attribute("id").toInt());
                    }
                }
                m_document.removeChild(infoXmlNode);
                kDebug() << "Reading file: " << url.path() << ", found clips: " << producers.count();
            } else kWarning() << "  NO KDENLIVE INFO FOUND IN FILE: " << url.path();
            KIO::NetAccess::removeTempFile(tmpFile);
        } else {
            KMessageBox::error(parent, KIO::NetAccess::lastErrorString());
        }
    } else {
        // Creating new document
        QDomElement westley = m_document.createElement("westley");
        m_document.appendChild(westley);

        QDomElement tractor = m_document.createElement("tractor");
        tractor.setAttribute("id", "maintractor");
        QDomElement multitrack = m_document.createElement("multitrack");
        QDomElement playlist = m_document.createElement("playlist");
        playlist.setAttribute("id", "black_track");
        westley.appendChild(playlist);


        // create playlists
        int audiotracks = 2;
        int videotracks = 3;
        int total = audiotracks + videotracks + 1;

        for (int i = 1; i < total; i++) {
            QDomElement playlist = m_document.createElement("playlist");
            playlist.setAttribute("id", "playlist" + QString::number(i));
            westley.appendChild(playlist);
        }

        QDomElement track0 = m_document.createElement("track");
        track0.setAttribute("producer", "black_track");
        tractor.appendChild(track0);

        // create audio tracks
        for (int i = 1; i < audiotracks + 1; i++) {
            QDomElement track = m_document.createElement("track");
            track.setAttribute("producer", "playlist" + QString::number(i));
            track.setAttribute("hide", "video");
            tractor.appendChild(track);
        }

        // create video tracks
        for (int i = audiotracks + 1; i < total; i++) {
            QDomElement track = m_document.createElement("track");
            track.setAttribute("producer", "playlist" + QString::number(i));
            tractor.appendChild(track);
        }

        for (uint i = 2; i < total ; i++) {
            QDomElement transition = m_document.createElement("transition");
            transition.setAttribute("in", "0");
            //TODO: Make audio mix last for all project duration
            transition.setAttribute("out", "15000");
            transition.setAttribute("a_track", QString::number(1));
            transition.setAttribute("b_track", QString::number(i));
            transition.setAttribute("mlt_service", "mix");
            transition.setAttribute("combine", "1");
            transition.setAttribute("internal_added", "237");
            tractor.appendChild(transition);
        }
        westley.appendChild(tractor);
    }
    m_scenelist = m_document.toString();
    // kDebug() << "scenelist" << m_scenelist;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);
}

KdenliveDoc::~KdenliveDoc() {
    delete m_commandStack;
    delete m_clipManager;
}

QDomElement KdenliveDoc::documentInfoXml() {
    QDomDocument doc;
    QDomElement e;
    QDomElement addedXml = doc.createElement("kdenlive");
    addedXml.setAttribute("version", "0.7");
    addedXml.setAttribute("profile", profilePath());
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        e = list.at(i)->toXML();
        e.setTagName("kdenlive_producer");
        addedXml.appendChild(doc.importNode(e, true));
    }

    //kDebug() << m_document.toString();
    return addedXml;
}


ClipManager *KdenliveDoc::clipManager() {
    return m_clipManager;
}

KUrl KdenliveDoc::projectFolder() const {
    if (m_projectFolder.isEmpty()) return KUrl(KStandardDirs::locateLocal("appdata", "/projects/"));
    return m_projectFolder;
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

bool KdenliveDoc::isModified() const {
    return m_modified;
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

void KdenliveDoc::slotAddTextClipFile(const QString path, const QString group, const int groupId) {
    kDebug() << "/////////  DOCUM, ADD TXT CLP: " << path;
    m_clipManager->slotAddTextClipFile(path, group, groupId);
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

void KdenliveDoc::slotAddSlideshowClipFile(const QString name, const QString path, int count, const QString duration, bool loop, const QString group, const int groupId) {
    m_clipManager->slotAddSlideshowClipFile(name, path, count, duration, loop, group, groupId);
    setModified(true);
}

void KdenliveDoc::slotCreateTextClip(QString group, int groupId) {
    QString titlesFolder = projectFolder().path() + "/titles/";
    KStandardDirs::makeDir(titlesFolder);
    TitleWidget *dia_ui = new TitleWidget(KUrl(), titlesFolder, m_render, 0);
    if (dia_ui->exec() == QDialog::Accepted) {
        QString titleName = "title";
        int counter = 0;
        QString path = titlesFolder + titleName + QString::number(counter).rightJustified(3, '0', false);
        while (QFile::exists(path + ".kdenlivetitle")) {
            counter++;
            path = titlesFolder + titleName + QString::number(counter).rightJustified(3, '0', false);
        }
        QPixmap pix = dia_ui->renderedPixmap();
        pix.save(path + ".png");
        dia_ui->saveTitle(path + ".kdenlivetitle");
        slotAddTextClipFile(path, QString(), -1);
    }
    delete dia_ui;
}

void KdenliveDoc::editTextClip(QString path, int id) {
    TitleWidget *dia_ui = new TitleWidget(KUrl(path + ".kdenlivetitle"), path, m_render, 0);
    if (dia_ui->exec() == QDialog::Accepted) {
        QPixmap pix = dia_ui->renderedPixmap();
        pix.save(path + ".png");
        dia_ui->saveTitle(path + ".kdenlivetitle");
        //slotAddClipFile(KUrl("/tmp/kdenlivetitle.png"), QString(), -1);
        emit refreshClipThumbnail(id);
    }
    delete dia_ui;
}


#include "kdenlivedoc.moc"

