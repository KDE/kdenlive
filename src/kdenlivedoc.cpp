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
#include <KApplication>

#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "profilesdialog.h"
#include "kdenlivesettings.h"
#include "renderer.h"
#include "clipmanager.h"
#include "addfoldercommand.h"
#include "editfoldercommand.h"
#include "titlewidget.h"
#include "mainwindow.h"


KdenliveDoc::KdenliveDoc(const KUrl &url, const KUrl &projectFolder, MltVideoProfile profile, QUndoGroup *undoGroup, MainWindow *parent): QObject(parent), m_render(NULL), m_url(url), m_projectFolder(projectFolder), m_profile(profile), m_fps((double)profile.frame_rate_num / profile.frame_rate_den), m_width(profile.width), m_height(profile.height), m_commandStack(new KUndoStack(undoGroup)), m_modified(false), m_documentLoadingProgress(0), m_documentLoadingStep(0.0), m_startPos(0) {
    kDebug() << "// init profile, ratnum: " << profile.frame_rate_num << ", " << profile.frame_rate_num << ", width: " << profile.width;
    m_clipManager = new ClipManager(this);
    if (!url.isEmpty()) {
        QString tmpFile;
        if (KIO::NetAccess::download(url.path(), tmpFile, parent)) {
            QFile file(tmpFile);
            m_document.setContent(&file, false);
            file.close();
            QDomNode infoXmlNode = m_document.elementsByTagName("kdenlivedoc").at(0);
            QDomNode westley = m_document.elementsByTagName("westley").at(0);
            if (!infoXmlNode.isNull()) {
                QDomElement infoXml = infoXmlNode.toElement();
                QString profilePath = infoXml.attribute("profile");
                m_startPos = infoXml.attribute("position").toInt();
                if (!profilePath.isEmpty()) setProfilePath(profilePath);
                double version = infoXml.attribute("version").toDouble();
                if (version < 0.7) convertDocument(version);
                else {
                    //delete all mlt producers and instead, use Kdenlive saved producers
                    QDomNodeList prods = m_document.elementsByTagName("producer");
                    int maxprod = prods.count();
                    int pos = 0;
                    for (int i = 0; i < maxprod; i++) {
                        QDomNode m = prods.at(pos);
                        if (m.toElement().attribute("id") == "black")
                            pos = 1;
                        else westley.removeChild(m);
                    }
                    prods = m_document.elementsByTagName("kdenlive_producer");
                    maxprod = prods.count();
                    for (int i = 0; i < maxprod; i++) {
                        prods.at(0).toElement().setTagName("producer");
                        westley.insertBefore(prods.at(0), QDomNode());
                    }
                }
                QDomElement e;
                QDomNodeList producers = m_document.elementsByTagName("producer");
                const int max = producers.count();
                if (max > 0) {
                    m_documentLoadingStep = 100.0 / (max + m_document.elementsByTagName("entry").count());
                    parent->slotGotProgressInfo(i18n("Loading project clips"), (int) m_documentLoadingProgress);
                }

                for (int i = 0; i < max; i++) {
                    e = producers.item(i).cloneNode().toElement();
                    if (m_documentLoadingStep > 0) {
                        m_documentLoadingProgress += m_documentLoadingStep;
                        parent->slotGotProgressInfo(QString(), (int) m_documentLoadingProgress);
                        qApp->processEvents();
                    }
                    if (!e.isNull() && e.attribute("id") != "black") {
                        addClip(e, e.attribute("id").toInt());
                    }
                }

                QDomNode markers = m_document.elementsByTagName("markers").at(0);
                if (!markers.isNull()) {
                    QDomNodeList markerslist = markers.childNodes();
                    int maxchild = markerslist.count();
                    for (int k = 0; k < maxchild; k++) {
                        e = markerslist.at(k).toElement();
                        if (e.tagName() == "marker") {
                            int id = e.attribute("id").toInt();
                            m_clipManager->getClipById(id)->addSnapMarker(GenTime(e.attribute("time").toDouble()), e.attribute("comment"));
                        }
                    }
                    m_document.removeChild(markers);
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

            QDomElement property = m_document.createElement("property");
            property.setAttribute("name", "a_track");
            QDomText value = m_document.createTextNode(QString::number(1));
            property.appendChild(value);
            transition.appendChild(property);

            property = m_document.createElement("property");
            property.setAttribute("name", "b_track");
            value = m_document.createTextNode(QString::number(i));
            property.appendChild(value);
            transition.appendChild(property);

            property = m_document.createElement("property");
            property.setAttribute("name", "mlt_service");
            value = m_document.createTextNode("mix");
            property.appendChild(value);
            transition.appendChild(property);

            property = m_document.createElement("property");
            property.setAttribute("name", "combine");
            value = m_document.createTextNode("1");
            property.appendChild(value);
            transition.appendChild(property);

            property = m_document.createElement("property");
            property.setAttribute("name", "internal_added");
            value = m_document.createTextNode("237");
            property.appendChild(value);
            transition.appendChild(property);
            tractor.appendChild(transition);
        }
        westley.appendChild(tractor);
    }
    m_scenelist = m_document.toString();
    kDebug() << "KDEnnlive document, init timecode: " << m_fps;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    QString directory = m_url.directory();
    QString fileName = m_url.fileName();
    m_recoveryUrl.setDirectory(directory);
    m_recoveryUrl.setFileName("~" + fileName);
    connect(m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));
}

KdenliveDoc::~KdenliveDoc() {
    delete m_commandStack;
    delete m_clipManager;
    delete m_autoSaveTimer;
    if (!m_url.isEmpty()) {
        // remove backup file
        if (KIO::NetAccess::exists(m_recoveryUrl, KIO::NetAccess::SourceSide, NULL))
            KIO::NetAccess::del(m_recoveryUrl, NULL);
    }
}

void KdenliveDoc::syncGuides(QList <Guide *> guides) {
    QDomDocument doc;
    QDomElement e;
    m_guidesXml.clear();
    m_guidesXml = doc.createElement("guides");
    for (int i = 0; i < guides.count(); i++) {
        e = doc.createElement("guide");
        e.setAttribute("time", guides.at(i)->position().ms() / 1000);
        e.setAttribute("comment", guides.at(i)->label());
        m_guidesXml.appendChild(e);
    }
}

void KdenliveDoc::slotAutoSave() {
    if (m_render)
        m_render->saveSceneList(m_recoveryUrl.path(), documentInfoXml());

}

void KdenliveDoc::convertDocument(double version) {
    // Opening a old Kdenlive document
    QDomNode westley = m_document.elementsByTagName("westley").at(1);
    QDomNode tractor = m_document.elementsByTagName("tractor").at(0);
    QDomNode kdenlivedoc = m_document.elementsByTagName("kdenlivedoc").at(0);
    QDomNode multitrack = m_document.elementsByTagName("multitrack").at(0);
    QDomNodeList playlists = m_document.elementsByTagName("playlist");

    QDomNode props = m_document.elementsByTagName("properties").at(0).toElement();
    QString profile = props.toElement().attribute("videoprofile");
    if (profile == "dv_wide") profile = "dv_pal_wide";
    if (!profile.isEmpty()) {
        setProfilePath(profile);
    } else setProfilePath("dv_pal");

    // move playlists outside of tractor and add the tracks instead
    int max = playlists.count();
    for (int i = 0; i < max; i++) {
        QDomNode n = playlists.at(i);
        westley.insertBefore(n, QDomNode());
        QDomElement pl = n.toElement();
        QDomElement track = m_document.createElement("track");
        QString trackType = pl.attribute("hide");
        if (!trackType.isEmpty()) track.setAttribute("hide", trackType);
        QString playlist_id =  pl.attribute("id");
        if (playlist_id.isEmpty()) {
            playlist_id = "black_track";
            pl.setAttribute("id", playlist_id);
        }
        track.setAttribute("producer", playlist_id);
        tractor.appendChild(track);
    }
    tractor.removeChild(multitrack);

    // audio track mixing transitions should not be added to track view, so add required attribute
    QDomNodeList transitions = m_document.elementsByTagName("transition");
    max = transitions.count();
    for (int i = 0; i < max; i++) {
        QDomElement tr = transitions.at(i).toElement();
        if (tr.attribute("combine") == "1" && tr.attribute("mlt_service") == "mix") {
            QDomElement property = m_document.createElement("property");
            property.setAttribute("name", "internal_added");
            QDomText value = m_document.createTextNode("237");
            property.appendChild(value);
            tr.appendChild(property);
        } else {
            // convert transition
            QDomNamedNodeMap attrs = tr.attributes();
            for (unsigned int j = 0; j < attrs.count(); j++) {
                QString attrName = attrs.item(j).nodeName();
                if (attrName != "in" && attrName != "out" && attrName != "id") {
                    QDomElement property = m_document.createElement("property");
                    property.setAttribute("name", attrName);
                    QDomText value = m_document.createTextNode(attrs.item(j).nodeValue());
                    property.appendChild(value);
                    tr.appendChild(property);
                }
            }
        }
    }

    // move transitions after tracks
    for (int i = 0; i < max; i++) {
        tractor.insertAfter(transitions.at(0), QDomNode());
    }

    QDomElement markers = m_document.createElement("markers");

    // change producer names
    QDomNodeList producers = m_document.elementsByTagName("producer");
    max = producers.count();
    for (int i = 0; i < max; i++) {
        QDomElement prod = producers.at(0).toElement();
        QDomNode m = prod.firstChild();
        if (!m.isNull() && m.toElement().tagName() == "markers") {
            QDomNodeList prodchilds = m.childNodes();
            int maxchild = prodchilds.count();
            for (int k = 0; k < maxchild; k++) {
                QDomElement mark = prodchilds.at(0).toElement();
                mark.setAttribute("id", prod.attribute("id"));
                markers.insertAfter(mark, QDomNode());
            }
            prod.removeChild(m);
        }
        int duration = prod.attribute("duration").toInt();
        if (duration > 0) prod.setAttribute("out", QString::number(duration));
        westley.insertBefore(prod, QDomNode());
    }

    QDomNode westley0 = m_document.elementsByTagName("westley").at(0);
    if (!markers.firstChild().isNull()) westley0.appendChild(markers);
    westley0.removeChild(kdenlivedoc);

    QDomNodeList elements = westley.childNodes();
    max = elements.count();
    for (int i = 0; i < max; i++) {
        QDomElement prod = elements.at(0).toElement();
        westley0.insertAfter(prod, QDomNode());
    }

    westley0.removeChild(westley);

    //kDebug() << "/////////////////  CONVERTED DOC:";
    //kDebug() << m_document.toString();
    //kDebug() << "/////////////////  END CONVERTED DOC:";
}

QDomElement KdenliveDoc::documentInfoXml() {
    QDomDocument doc;
    QDomElement e;
    QDomElement addedXml = doc.createElement("kdenlivedoc");
    QDomElement markers = doc.createElement("markers");
    addedXml.setAttribute("version", "0.7");
    addedXml.setAttribute("profile", profilePath());
    addedXml.setAttribute("position", m_render->seekPosition().frames(m_fps));
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        e = list.at(i)->toXML();
        e.setTagName("kdenlive_producer");
        addedXml.appendChild(doc.importNode(e, true));
        QList < CommentedTime > marks = list.at(i)->commentedSnapMarkers();
        for (int j = 0; j < marks.count(); j++) {
            QDomElement marker = doc.createElement("marker");
            marker.setAttribute("time", marks.at(j).time().ms() / 1000);
            marker.setAttribute("comment", marks.at(j).comment());
            marker.setAttribute("id", e.attribute("id"));
            markers.appendChild(marker);
        }
    }
    addedXml.appendChild(markers);
    if (!m_guidesXml.isNull()) addedXml.appendChild(doc.importNode(m_guidesXml, true));
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
    kDebug() << "KDEnnlive document, init timecode from path: " << path << ",  " << m_fps;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);
}

void KdenliveDoc::setThumbsProgress(const QString &message, int progress) {
    emit progressInfo(message, progress);
}

void KdenliveDoc::loadingProgressed() {
    m_documentLoadingProgress += m_documentLoadingStep;
    emit progressInfo(QString(), (int) m_documentLoadingProgress);
}

KUndoStack *KdenliveDoc::commandStack() {
    return m_commandStack;
}

void KdenliveDoc::setRenderer(Render *render) {
    m_render = render;
    emit progressInfo(i18n("Loading playlist..."), 0);
    qApp->processEvents();
    if (m_render) m_render->setSceneList(m_scenelist, m_startPos);
    emit progressInfo(QString(), -1);
}

Render *KdenliveDoc::renderer() {
    return m_render;
}

void KdenliveDoc::updateClip(int id) {
    emit updateClipDisplay(id);
}

void KdenliveDoc::updateAllProjectClips() {
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    for (int i = 0; i < list.count(); i++)
        emit updateClipDisplay(list.at(i)->getId());
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
    QString directory = m_url.directory();
    QString fileName = m_url.fileName();
    m_recoveryUrl.setDirectory(directory);
    m_recoveryUrl.setFileName("~" + fileName);
}

void KdenliveDoc::setModified(bool mod) {
    if (!m_url.isEmpty() && mod && KdenliveSettings::crashrecovery()) {
        m_autoSaveTimer->start(3000);
    }
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
    TitleWidget *dia_ui = new TitleWidget(KUrl(), titlesFolder, m_render, kapp->activeWindow());
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
    TitleWidget *dia_ui = new TitleWidget(KUrl(path + ".kdenlivetitle"), path, m_render, kapp->activeWindow());
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

