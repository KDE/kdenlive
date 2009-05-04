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


#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "profilesdialog.h"
#include "kdenlivesettings.h"
#include "renderer.h"
#include "clipmanager.h"
#include "titlewidget.h"
#include "mainwindow.h"
#include "documentchecker.h"
#include "kdenlive-config.h"

#include <KDebug>
#include <KStandardDirs>
#include <KMessageBox>
#include <KLocale>
#include <KFileDialog>
#include <KIO/NetAccess>
#include <KIO/CopyJob>
#include <KApplication>

#include <QCryptographicHash>
#include <QFile>

#include <mlt++/Mlt.h>

KdenliveDoc::KdenliveDoc(const KUrl &url, const KUrl &projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QPoint tracks, Render *render, MainWindow *parent) :
        QObject(parent),
        m_autosave(NULL),
        m_url(url),
        m_zoom(7),
        m_startPos(0),
        m_render(render),
        m_commandStack(new QUndoStack(undoGroup)),
        m_modified(false),
        m_projectFolder(projectFolder),
        m_documentLoadingStep(0.0),
        m_documentLoadingProgress(0),
        m_abortLoading(false),
        m_zoneStart(0),
        m_zoneEnd(100)
{
    m_clipManager = new ClipManager(this);
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    if (!url.isEmpty()) {
        QString tmpFile;
        bool success = KIO::NetAccess::download(url.path(), tmpFile, parent);
        if (success) {
            QFile file(tmpFile);
            QString errorMsg;
            success = m_document.setContent(&file, false, &errorMsg);
            file.close();
            if (success == false) {
                // File is corrupted, warn user
                KMessageBox::error(parent, errorMsg);
            }
        } else KMessageBox::error(parent, KIO::NetAccess::lastErrorString());

        if (success) {
            QDomNode infoXmlNode = m_document.elementsByTagName("kdenlivedoc").at(0);
            QDomNode westley = m_document.elementsByTagName("westley").at(0);
            if (!infoXmlNode.isNull()) {
                QDomElement infoXml = infoXmlNode.toElement();
                double version = infoXml.attribute("version").toDouble();

                // Upgrade old Kdenlive documents to current version
                if (!convertDocument(version)) {
                    m_url.clear();
                    m_document = createEmptyDocument(tracks.x(), tracks.y());
                    setProfilePath(profileName);
                } else {
                    /*
                     * read again <kdenlivedoc> and <westley> to get all the new
                     * stuff (convertDocument() can now do anything without breaking
                     * document loading)
                     */
                    infoXmlNode = m_document.elementsByTagName("kdenlivedoc").at(0);
                    infoXml = infoXmlNode.toElement();
                    version = infoXml.attribute("version").toDouble();
                    westley = m_document.elementsByTagName("westley").at(0);

                    QString profilePath = infoXml.attribute("profile");
                    QString projectFolderPath = infoXml.attribute("projectfolder");
                    if (!projectFolderPath.isEmpty()) m_projectFolder = KUrl(projectFolderPath);

                    if (m_projectFolder.isEmpty() || !KIO::NetAccess::exists(m_projectFolder.path(), KIO::NetAccess::DestinationSide, parent)) {
                        // Make sure the project folder is usable
                        KMessageBox::information(parent, i18n("Document project folder is invalid, setting it to the default one: %1", KdenliveSettings::defaultprojectfolder()));
                        m_projectFolder = KUrl(KdenliveSettings::defaultprojectfolder());
                    }
                    m_startPos = infoXml.attribute("position").toInt();
                    m_zoom = infoXml.attribute("zoom", "7").toInt();
                    m_zoneStart = infoXml.attribute("zonein", "0").toInt();
                    m_zoneEnd = infoXml.attribute("zoneout", "100").toInt();
                    setProfilePath(profilePath);

                    // Build tracks
                    QDomElement e;
                    QDomNode tracksinfo = m_document.elementsByTagName("tracksinfo").at(0);
                    TrackInfo projectTrack;
                    if (!tracksinfo.isNull()) {
                        QDomNodeList trackslist = tracksinfo.childNodes();
                        int maxchild = trackslist.count();
                        for (int k = 0; k < maxchild; k++) {
                            e = trackslist.at(k).toElement();
                            if (e.tagName() == "trackinfo") {
                                if (e.attribute("type") == "audio") projectTrack.type = AUDIOTRACK;
                                else projectTrack.type = VIDEOTRACK;
                                projectTrack.isMute = e.attribute("mute").toInt();
                                projectTrack.isBlind = e.attribute("blind").toInt();
                                projectTrack.isLocked = e.attribute("locked").toInt();
                                m_tracksList.append(projectTrack);
                            }
                        }
                        westley.removeChild(tracksinfo);
                    }
                    checkDocumentClips();
                    QDomNodeList producers = m_document.elementsByTagName("producer");
                    QDomNodeList infoproducers = m_document.elementsByTagName("kdenlive_producer");
                    const int max = producers.count();
                    const int infomax = infoproducers.count();

                    QDomNodeList folders = m_document.elementsByTagName("folder");
                    for (int i = 0; i < folders.count(); i++) {
                        e = folders.item(i).cloneNode().toElement();
                        m_clipManager->addFolder(e.attribute("id"), e.attribute("name"));
                    }

                    if (max > 0) {
                        m_documentLoadingStep = 100.0 / (max + infomax + m_document.elementsByTagName("entry").count());
                        parent->slotGotProgressInfo(i18n("Loading project clips"), (int) m_documentLoadingProgress);
                    }


                    for (int i = 0; i < infomax && !m_abortLoading; i++) {
                        e = infoproducers.item(i).cloneNode().toElement();
                        if (m_documentLoadingStep > 0) {
                            m_documentLoadingProgress += m_documentLoadingStep;
                            parent->slotGotProgressInfo(QString(), (int) m_documentLoadingProgress);
                            //qApp->processEvents();
                        }
                        QString prodId = e.attribute("id");
                        if (!e.isNull() && prodId != "black" && !prodId.startsWith("slowmotion") && !m_abortLoading) {
                            e.setTagName("producer");
                            // Get MLT's original producer properties
                            QDomElement orig;
                            for (int j = 0; j < max; j++) {
                                QDomElement o = producers.item(j).cloneNode().toElement();
                                QString origId = o.attribute("id").section('_', 0, 0);
                                if (origId == prodId) {
                                    orig = o;
                                    break;
                                }
                            }
                            addClipInfo(e, orig, prodId);
                            kDebug() << "// NLIVE PROD: " << prodId;
                        }
                    }
                    if (m_abortLoading) {
                        //parent->slotGotProgressInfo(i18n("File %1 is not a Kdenlive project file."), 100);
                        emit resetProjectList();
                        m_startPos = 0;
                        m_url = KUrl();
                        m_tracksList.clear();
                        kWarning() << "Aborted loading of: " << url.path();
                        m_document = createEmptyDocument(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
                        setProfilePath(KdenliveSettings::default_profile());
                        m_clipManager->clear();
                    } else {
                        QDomNode markers = m_document.elementsByTagName("markers").at(0);
                        if (!markers.isNull()) {
                            QDomNodeList markerslist = markers.childNodes();
                            int maxchild = markerslist.count();
                            for (int k = 0; k < maxchild; k++) {
                                e = markerslist.at(k).toElement();
                                if (e.tagName() == "marker") {
                                    m_clipManager->getClipById(e.attribute("id"))->addSnapMarker(GenTime(e.attribute("time").toDouble()), e.attribute("comment"));
                                }
                            }
                            westley.removeChild(markers);
                        }
                        m_document.removeChild(infoXmlNode);
                        kDebug() << "Reading file: " << url.path() << ", found clips: " << producers.count();
                    }
                }
            } else {
                parent->slotGotProgressInfo(i18n("File %1 is not a Kdenlive project file."), 100);
                kWarning() << "  NO KDENLIVE INFO FOUND IN FILE: " << url.path();
                m_document = createEmptyDocument(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
                m_url = KUrl();
                setProfilePath(KdenliveSettings::default_profile());
            }
            KIO::NetAccess::removeTempFile(tmpFile);
        } else {
            parent->slotGotProgressInfo(i18n("File %1 is not a Kdenlive project file."), 100);
            m_url = KUrl();
            m_document = createEmptyDocument(KdenliveSettings::videotracks(), KdenliveSettings::audiotracks());
            setProfilePath(KdenliveSettings::default_profile());
        }
    } else {
        m_document = createEmptyDocument(tracks.x(), tracks.y());
        setProfilePath(profileName);
    }
    if (m_projectFolder.isEmpty()) m_projectFolder = KUrl(KdenliveSettings::defaultprojectfolder());

    // make sure that the necessary folders exist
    KStandardDirs::makeDir(m_projectFolder.path() + "/titles/");
    KStandardDirs::makeDir(m_projectFolder.path() + "/thumbs/");
    KStandardDirs::makeDir(m_projectFolder.path() + "/ladspa/");

    kDebug() << "KDEnlive document, init timecode: " << m_fps;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);

    //kDebug() << "// SETTING SCENE LIST:\n\n" << m_document.toString();
    connect(m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));

}

KdenliveDoc::~KdenliveDoc()
{
    delete m_commandStack;
    delete m_clipManager;
    delete m_autoSaveTimer;
    if (m_autosave) {
        m_autosave->remove();
        delete m_autosave;
    }
}

void KdenliveDoc::setSceneList()
{
    m_render->setSceneList(m_document.toString(), m_startPos);
    // m_document xml is now useless, clear it
    m_document.clear();
    checkProjectClips();
}

QDomDocument KdenliveDoc::createEmptyDocument(const int videotracks, const int audiotracks)
{
    // Creating new document
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);


    TrackInfo videoTrack;
    videoTrack.type = VIDEOTRACK;
    videoTrack.isMute = false;
    videoTrack.isBlind = false;
    videoTrack.isLocked = false;

    TrackInfo audioTrack;
    audioTrack.type = AUDIOTRACK;
    audioTrack.isMute = false;
    audioTrack.isBlind = true;
    audioTrack.isLocked = false;

    QDomElement tractor = doc.createElement("tractor");
    tractor.setAttribute("id", "maintractor");
    QDomElement multitrack = doc.createElement("multitrack");
    QDomElement playlist = doc.createElement("playlist");
    playlist.setAttribute("id", "black_track");
    westley.appendChild(playlist);


    // create playlists
    int total = audiotracks + videotracks + 1;

    for (int i = 1; i < total; i++) {
        QDomElement playlist = doc.createElement("playlist");
        playlist.setAttribute("id", "playlist" + QString::number(i));
        westley.appendChild(playlist);
    }

    QDomElement track0 = doc.createElement("track");
    track0.setAttribute("producer", "black_track");
    tractor.appendChild(track0);

    // create audio tracks
    for (int i = 1; i < audiotracks + 1; i++) {
        QDomElement track = doc.createElement("track");
        track.setAttribute("producer", "playlist" + QString::number(i));
        track.setAttribute("hide", "video");
        tractor.appendChild(track);
        m_tracksList.append(audioTrack);
    }

    // create video tracks
    for (int i = audiotracks + 1; i < total; i++) {
        QDomElement track = doc.createElement("track");
        track.setAttribute("producer", "playlist" + QString::number(i));
        tractor.appendChild(track);
        m_tracksList.append(videoTrack);
    }

    for (int i = 2; i < total ; i++) {
        QDomElement transition = doc.createElement("transition");
        transition.setAttribute("always_active", "1");

        QDomElement property = doc.createElement("property");
        property.setAttribute("name", "a_track");
        QDomText value = doc.createTextNode(QString::number(1));
        property.appendChild(value);
        transition.appendChild(property);

        property = doc.createElement("property");
        property.setAttribute("name", "b_track");
        value = doc.createTextNode(QString::number(i));
        property.appendChild(value);
        transition.appendChild(property);

        property = doc.createElement("property");
        property.setAttribute("name", "mlt_service");
        value = doc.createTextNode("mix");
        property.appendChild(value);
        transition.appendChild(property);

        property = doc.createElement("property");
        property.setAttribute("name", "combine");
        value = doc.createTextNode("1");
        property.appendChild(value);
        transition.appendChild(property);

        property = doc.createElement("property");
        property.setAttribute("name", "internal_added");
        value = doc.createTextNode("237");
        property.appendChild(value);
        transition.appendChild(property);
        tractor.appendChild(transition);
    }
    westley.appendChild(tractor);
    return doc;
}


void KdenliveDoc::syncGuides(QList <Guide *> guides)
{
    m_guidesXml.clear();
    QDomElement guideNode = m_guidesXml.createElement("guides");
    m_guidesXml.appendChild(guideNode);
    QDomElement e;

    for (int i = 0; i < guides.count(); i++) {
        e = m_guidesXml.createElement("guide");
        e.setAttribute("time", guides.at(i)->position().ms() / 1000);
        e.setAttribute("comment", guides.at(i)->label());
        guideNode.appendChild(e);
    }
    emit guidesUpdated();
}

QDomElement KdenliveDoc::guidesXml() const
{
    return m_guidesXml.documentElement();
}

void KdenliveDoc::slotAutoSave()
{
    if (m_render && m_autosave) {
        if (!m_autosave->isOpen() && !m_autosave->open(QIODevice::ReadWrite)) {
            // show error: could not open the autosave file
            kDebug() << "ERROR; CANNOT CREATE AUTOSAVE FILE";
        }
        kDebug() << "// AUTOSAVE FILE: " << m_autosave->fileName();
        QString doc;
        if (KdenliveSettings::dropbframes()) {
            KdenliveSettings::setDropbframes(false);
            m_clipManager->updatePreviewSettings();
            doc = m_render->sceneList();
            KdenliveSettings::setDropbframes(true);
            m_clipManager->updatePreviewSettings();
        } else doc = m_render->sceneList();
        saveSceneList(m_autosave->fileName(), doc);
    }
}

void KdenliveDoc::setZoom(int factor)
{
    m_zoom = factor;
}

int KdenliveDoc::zoom() const
{
    return m_zoom;
}

bool KdenliveDoc::convertDocument(double version)
{
    kDebug() << "Opening a document with version " << version;
    const double current_version = 0.82;

    if (version == current_version) return true;

    if (version > current_version) {
        kDebug() << "Unable to open document with version " << version;
        KMessageBox::sorry(kapp->activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.\nPlease consider upgrading you Kdenlive version.", version), i18n("Unable to open project"));
        return false;
    }

    // Opening a old Kdenlive document
    if (version == 0.5 || version == 0.7) {
        KMessageBox::sorry(kapp->activeWindow(), i18n("This project type is unsupported (version %1) and can't be loaded.", version), i18n("Unable to open project"));
        kDebug() << "Unable to open document with version " << version;
        // TODO: convert 0.7 (0.5?) files to the new document format.
        return false;
    }

    setModified(true);

    if (version == 0.81) {
        // Add correct tracks info
        QDomNode kdenlivedoc = m_document.elementsByTagName("kdenlivedoc").at(0);
        QDomElement infoXml = kdenlivedoc.toElement();
        infoXml.setAttribute("version", current_version);
        QString currentTrackOrder = infoXml.attribute("tracks");
        QDomElement tracksinfo = m_document.createElement("tracksinfo");
        for (int i = 0; i < currentTrackOrder.size(); i++) {
            QDomElement trackinfo = m_document.createElement("trackinfo");
            if (currentTrackOrder.data()[i] == 'a') {
                trackinfo.setAttribute("type", "audio");
                trackinfo.setAttribute("blind", true);
            } else trackinfo.setAttribute("blind", false);
            trackinfo.setAttribute("mute", false);
            trackinfo.setAttribute("locked", false);
            tracksinfo.appendChild(trackinfo);
        }
        infoXml.appendChild(tracksinfo);
        return true;
    }

    if (version == 0.8) {
        // Add the tracks information
        QDomNodeList tracks = m_document.elementsByTagName("track");
        int max = tracks.count();

        QDomNode kdenlivedoc = m_document.elementsByTagName("kdenlivedoc").at(0);
        QDomElement infoXml = kdenlivedoc.toElement();
        infoXml.setAttribute("version", current_version);
        QDomElement tracksinfo = m_document.createElement("tracksinfo");

        for (int i = 0; i < max; i++) {
            QDomElement trackinfo = m_document.createElement("trackinfo");
            QDomElement t = tracks.at(i).toElement();
            if (t.attribute("hide") == "video") {
                trackinfo.setAttribute("type", "audio");
                trackinfo.setAttribute("blind", true);
            } else trackinfo.setAttribute("blind", false);
            trackinfo.setAttribute("mute", false);
            trackinfo.setAttribute("locked", false);
            if (t.attribute("producer") != "black_track") tracksinfo.appendChild(trackinfo);
        }
        infoXml.appendChild(tracksinfo);
        return true;
    }

    QDomNode westley = m_document.elementsByTagName("westley").at(1);
    QDomNode tractor = m_document.elementsByTagName("tractor").at(0);
    QDomNode kdenlivedoc = m_document.elementsByTagName("kdenlivedoc").at(0);
    QDomElement kdenlivedoc_old = kdenlivedoc.cloneNode(true).toElement(); // Needed for folders
    QDomElement infoXml = kdenlivedoc.toElement();
    infoXml.setAttribute("version", current_version);
    QDomNode multitrack = m_document.elementsByTagName("multitrack").at(0);
    QDomNodeList playlists = m_document.elementsByTagName("playlist");

    QDomNode props = m_document.elementsByTagName("properties").at(0).toElement();
    QString profile = props.toElement().attribute("videoprofile");
    m_startPos = props.toElement().attribute("timeline_position").toInt();
    if (profile == "dv_wide") profile = "dv_pal_wide";

    // move playlists outside of tractor and add the tracks instead
    int max = playlists.count();
    if (westley.isNull()) {
        westley = m_document.createElement("westley");
        m_document.documentElement().appendChild(westley);
    }
    if (tractor.isNull()) {
        kDebug() << "// NO WESTLEY PLAYLIST, building empty one";
        QDomElement blank_tractor = m_document.createElement("tractor");
        westley.appendChild(blank_tractor);
        QDomElement blank_playlist = m_document.createElement("playlist");
        blank_playlist.setAttribute("id", "black_track");
        westley.insertBefore(blank_playlist, QDomNode());
        QDomElement blank_track = m_document.createElement("track");
        blank_track.setAttribute("producer", "black_track");
        blank_tractor.appendChild(blank_track);

        QDomNodeList kdenlivetracks = m_document.elementsByTagName("kdenlivetrack");
        for (int i = 0; i < kdenlivetracks.count(); i++) {
            blank_playlist = m_document.createElement("playlist");
            blank_playlist.setAttribute("id", "playlist" + QString::number(i));
            westley.insertBefore(blank_playlist, QDomNode());
            blank_track = m_document.createElement("track");
            blank_track.setAttribute("producer", "playlist" + QString::number(i));
            blank_tractor.appendChild(blank_track);
            if (kdenlivetracks.at(i).toElement().attribute("cliptype") == "Sound") {
                blank_playlist.setAttribute("hide", "video");
                blank_track.setAttribute("hide", "video");
            }
        }
    } else for (int i = 0; i < max; i++) {
            QDomNode n = playlists.at(i);
            westley.insertBefore(n, QDomNode());
            QDomElement pl = n.toElement();
            QDomElement track = m_document.createElement("track");
            QString trackType = pl.attribute("hide");
            if (!trackType.isEmpty())
                track.setAttribute("hide", trackType);
            QString playlist_id =  pl.attribute("id");
            if (playlist_id.isEmpty()) {
                playlist_id = "black_track";
                pl.setAttribute("id", playlist_id);
            }
            track.setAttribute("producer", playlist_id);
            //tractor.appendChild(track);
#define KEEP_TRACK_ORDER 1
#ifdef KEEP_TRACK_ORDER
            tractor.insertAfter(track, QDomNode());
#else
            // Insert the new track in an order that hopefully matches the 3 video, then 2 audio tracks of Kdenlive 0.7.0
            // insertion sort - O( tracks*tracks )
            // Note, this breaks _all_ transitions - but you can move them up and down afterwards.
            QDomElement tractor_elem = tractor.toElement();
            if (! tractor_elem.isNull()) {
                QDomNodeList tracks = tractor_elem.elementsByTagName("track");
                int size = tracks.size();
                if (size == 0) {
                    tractor.insertAfter(track, QDomNode());
                } else {
                    bool inserted = false;
                    for (int i = 0; i < size; ++i) {
                        QDomElement track_elem = tracks.at(i).toElement();
                        if (track_elem.isNull()) {
                            tractor.insertAfter(track, QDomNode());
                            inserted = true;
                            break;
                        } else {
                            kDebug() << "playlist_id: " << playlist_id << " producer:" << track_elem.attribute("producer");
                            if (playlist_id < track_elem.attribute("producer")) {
                                tractor.insertBefore(track, track_elem);
                                inserted = true;
                                break;
                            }
                        }
                    }
                    // Reach here, no insertion, insert last
                    if (!inserted) {
                        tractor.insertAfter(track, QDomNode());
                    }
                }
            } else {
                kWarning() << "tractor was not a QDomElement";
                tractor.insertAfter(track, QDomNode());
            }
#endif
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
            property = m_document.createElement("property");
            property.setAttribute("name", "mlt_service");
            value = m_document.createTextNode("mix");
            property.appendChild(value);
            tr.appendChild(property);
        } else {
            // convert transition
            QDomNamedNodeMap attrs = tr.attributes();
            for (int j = 0; j < attrs.count(); j++) {
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

    // Fix filters format
    QDomNodeList entries = m_document.elementsByTagName("entry");
    max = entries.count();
    for (int i = 0; i < max; i++) {
        QString last_id;
        int effectix = 0;
        QDomNode m = entries.at(i).firstChild();
        while (!m.isNull()) {
            if (m.toElement().tagName() == "filter") {
                QDomElement filt = m.toElement();
                QDomNamedNodeMap attrs = filt.attributes();
                QString current_id = filt.attribute("kdenlive_id");
                if (current_id != last_id) {
                    effectix++;
                    last_id = current_id;
                }
                QDomElement e = m_document.createElement("property");
                e.setAttribute("name", "kdenlive_ix");
                QDomText value = m_document.createTextNode(QString::number(effectix));
                e.appendChild(value);
                filt.appendChild(e);
                for (int j = 0; j < attrs.count(); j++) {
                    QDomAttr a = attrs.item(j).toAttr();
                    if (!a.isNull()) {
                        kDebug() << " FILTER; adding :" << a.name() << ":" << a.value();
                        QDomElement e = m_document.createElement("property");
                        e.setAttribute("name", a.name());
                        QDomText value = m_document.createTextNode(a.value());
                        e.appendChild(value);
                        filt.appendChild(e);

                    }
                }
            }
            m = m.nextSibling();
        }
    }

    /*
        QDomNodeList filters = m_document.elementsByTagName("filter");
        max = filters.count();
        QString last_id;
        int effectix = 0;
        for (int i = 0; i < max; i++) {
            QDomElement filt = filters.at(i).toElement();
            QDomNamedNodeMap attrs = filt.attributes();
     QString current_id = filt.attribute("kdenlive_id");
     if (current_id != last_id) {
         effectix++;
         last_id = current_id;
     }
     QDomElement e = m_document.createElement("property");
            e.setAttribute("name", "kdenlive_ix");
            QDomText value = m_document.createTextNode(QString::number(1));
            e.appendChild(value);
            filt.appendChild(e);
            for (int j = 0; j < attrs.count(); j++) {
                QDomAttr a = attrs.item(j).toAttr();
                if (!a.isNull()) {
                    kDebug() << " FILTER; adding :" << a.name() << ":" << a.value();
                    QDomElement e = m_document.createElement("property");
                    e.setAttribute("name", a.name());
                    QDomText value = m_document.createTextNode(a.value());
                    e.appendChild(value);
                    filt.appendChild(e);
                }
            }
        }*/

    // fix slowmotion
    QDomNodeList producers = westley.toElement().elementsByTagName("producer");
    max = producers.count();
    for (int i = 0; i < max; i++) {
        QDomElement prod = producers.at(i).toElement();
        if (prod.attribute("mlt_service") == "framebuffer") {
            QString slowmotionprod = prod.attribute("resource");
            slowmotionprod.replace(':', '?');
            kDebug() << "// FOUND WRONG SLOWMO, new: " << slowmotionprod;
            prod.setAttribute("resource", slowmotionprod);
        }
    }
    // move producers to correct place, markers to a global list, fix clip descriptions
    QDomElement markers = m_document.createElement("markers");
    // This will get the westley producers:
    producers = m_document.elementsByTagName("producer");
    max = producers.count();
    for (int i = 0; i < max; i++) {
        QDomElement prod = producers.at(0).toElement();
        // add resource also as a property (to allow path correction in setNewResource())
        // TODO: will it work with slowmotion? needs testing
        /*if (!prod.attribute("resource").isEmpty()) {
            QDomElement prop_resource = m_document.createElement("property");
            prop_resource.setAttribute("name", "resource");
            QDomText resource = m_document.createTextNode(prod.attribute("resource"));
            prop_resource.appendChild(resource);
            prod.appendChild(prop_resource);
        }*/
        QDomNode m = prod.firstChild();
        if (!m.isNull()) {
            if (m.toElement().tagName() == "markers") {
                QDomNodeList prodchilds = m.childNodes();
                int maxchild = prodchilds.count();
                for (int k = 0; k < maxchild; k++) {
                    QDomElement mark = prodchilds.at(0).toElement();
                    mark.setAttribute("id", prod.attribute("id"));
                    markers.insertAfter(mark, QDomNode());
                }
                prod.removeChild(m);
            } else if (prod.attribute("type").toInt() == TEXT) {
                // convert title clip
                if (m.toElement().tagName() == "textclip") {
                    QDomDocument tdoc;
                    QDomElement titleclip = m.toElement();
                    QDomElement title = tdoc.createElement("kdenlivetitle");
                    tdoc.appendChild(title);
                    QDomNodeList objects = titleclip.childNodes();
                    int maxchild = objects.count();
                    for (int k = 0; k < maxchild; k++) {
                        QString objectxml;
                        QDomElement ob = objects.at(k).toElement();
                        if (ob.attribute("type") == "3") {
                            // text object - all of this goes into "xmldata"...
                            QDomElement item = tdoc.createElement("item");
                            item.setAttribute("z-index", ob.attribute("z"));
                            item.setAttribute("type", "QGraphicsTextItem");
                            QDomElement position = tdoc.createElement("position");
                            position.setAttribute("x", ob.attribute("x"));
                            position.setAttribute("y", ob.attribute("y"));
                            QDomElement content = tdoc.createElement("content");
                            content.setAttribute("font", ob.attribute("font_family"));
                            content.setAttribute("font-size", ob.attribute("font_size"));
                            content.setAttribute("font-bold", ob.attribute("bold"));
                            content.setAttribute("font-italic", ob.attribute("italic"));
                            content.setAttribute("font-underline", ob.attribute("underline"));
                            QString col = ob.attribute("color");
                            QColor c(col);
                            content.setAttribute("font-color", colorToString(c));
                            // todo: These fields are missing from the newly generated xmldata:
                            // transform, startviewport, endviewport, background

                            QDomText conttxt = tdoc.createTextNode(ob.attribute("text"));
                            content.appendChild(conttxt);
                            item.appendChild(position);
                            item.appendChild(content);
                            title.appendChild(item);
                        } else if (ob.attribute("type") == "5") {
                            // rectangle object
                            QDomElement item = tdoc.createElement("item");
                            item.setAttribute("z-index", ob.attribute("z"));
                            item.setAttribute("type", "QGraphicsRectItem");
                            QDomElement position = tdoc.createElement("position");
                            position.setAttribute("x", ob.attribute("x"));
                            position.setAttribute("y", ob.attribute("y"));
                            QDomElement content = tdoc.createElement("content");
                            QString col = ob.attribute("color");
                            QColor c(col);
                            content.setAttribute("brushcolor", colorToString(c));
                            QString rect = "0,0,";
                            rect.append(ob.attribute("width"));
                            rect.append(",");
                            rect.append(ob.attribute("height"));
                            content.setAttribute("rect", rect);
                            item.appendChild(position);
                            item.appendChild(content);
                            title.appendChild(item);
                        }
                    }
                    prod.setAttribute("xmldata", tdoc.toString());
                    // mbd todo: This clearly does not work, as every title gets the same name - trying to leave it empty
                    // QStringList titleInfo = TitleWidget::getFreeTitleInfo(projectFolder());
                    // prod.setAttribute("titlename", titleInfo.at(0));
                    // prod.setAttribute("resource", titleInfo.at(1));
                    //kDebug()<<"TITLE DATA:\n"<<tdoc.toString();
                    prod.removeChild(m);
                } // End conversion of title clips.

            } else if (m.isText()) {
                QString comment = m.nodeValue();
                if (!comment.isEmpty()) {
                    prod.setAttribute("description", comment);
                }
                prod.removeChild(m);
            }
        }
        int duration = prod.attribute("duration").toInt();
        if (duration > 0) prod.setAttribute("out", QString::number(duration));
        // The clip goes back in, but text clips should not go back in, at least not modified
        westley.insertBefore(prod, QDomNode());

    }

    QDomNode westley0 = m_document.elementsByTagName("westley").at(0);
    if (!markers.firstChild().isNull()) westley0.appendChild(markers);


    // Convert as much of the kdenlivedoc as possible. Use the producer in westley
    // First, remove the old stuff from westley, and add a new empty one
    // Also, track the max id in order to use it for the adding of groups/folders
    int max_kproducer_id = 0;
    westley0.removeChild(kdenlivedoc);
    QDomElement kdenlivedoc_new = m_document.createElement("kdenlivedoc");
    kdenlivedoc_new.setAttribute("profile", profile);

    // Add tracks info
    QDomNodeList tracks = m_document.elementsByTagName("track");
    max = tracks.count();
    QDomElement tracksinfo = m_document.createElement("tracksinfo");
    for (int i = 0; i < max; i++) {
        QDomElement trackinfo = m_document.createElement("trackinfo");
        QDomElement t = tracks.at(i).toElement();
        if (t.attribute("hide") == "video") {
            trackinfo.setAttribute("type", "audio");
            trackinfo.setAttribute("blind", true);
        } else trackinfo.setAttribute("blind", false);
        trackinfo.setAttribute("mute", false);
        trackinfo.setAttribute("locked", false);
        if (t.attribute("producer") != "black_track") tracksinfo.appendChild(trackinfo);
    }
    kdenlivedoc_new.appendChild(tracksinfo);

    // Add all the producers that has a ressource in westley
    QDomElement westley_element = westley0.toElement();
    if (westley_element.isNull()) {
        kWarning() << "westley0 element in document was not a QDomElement - unable to add producers to new kdenlivedoc";
    } else {
        QDomNodeList wproducers = westley_element.elementsByTagName("producer");
        int kmax = wproducers.count();
        for (int i = 0; i < kmax; i++) {
            QDomElement wproducer = wproducers.at(i).toElement();
            if (wproducer.isNull()) {
                kWarning() << "Found producer in westley0, that was not a QDomElement";
                continue;
            }
            if (wproducer.attribute("id") == "black") continue;
            // We have to do slightly different things, depending on the type
            kDebug() << "Converting producer element with type" << wproducer.attribute("type");
            if (wproducer.attribute("type").toInt() == TEXT) {
                kDebug() << "Found TEXT element in producer" << endl;
                QDomElement kproducer = wproducer.cloneNode(true).toElement();
                kproducer.setTagName("kdenlive_producer");
                kdenlivedoc_new.appendChild(kproducer);
                // TODO: Perhaps needs some more changes here to "frequency", aspect ratio as a float, frame_size, channels, and later, ressource and title name
            } else {
                QDomElement kproducer = m_document.createElement("kdenlive_producer");
                kproducer.setAttribute("id", wproducer.attribute("id"));
                if (!wproducer.attribute("description").isEmpty())
                    kproducer.setAttribute("description", wproducer.attribute("description"));
                kproducer.setAttribute("resource", wproducer.attribute("resource"));
                kproducer.setAttribute("type", wproducer.attribute("type"));
                // Testing fix for 358
                if (!wproducer.attribute("aspect_ratio").isEmpty()) {
                    kproducer.setAttribute("aspect_ratio", wproducer.attribute("aspect_ratio"));
                }
                if (!wproducer.attribute("source_fps").isEmpty()) {
                    kproducer.setAttribute("fps", wproducer.attribute("source_fps"));
                }
                if (!wproducer.attribute("length").isEmpty()) {
                    kproducer.setAttribute("duration", wproducer.attribute("length"));
                }
                kdenlivedoc_new.appendChild(kproducer);
            }
            if (wproducer.attribute("id").toInt() > max_kproducer_id) {
                max_kproducer_id = wproducer.attribute("id").toInt();
            }
        }
    }
#define LOOKUP_FOLDER 1
#ifdef LOOKUP_FOLDER
    // Look through all the folder elements of the old doc, for each folder, for each producer,
    // get the id, look it up in the new doc, set the groupname and groupid
    // Note, this does not work at the moment - at least one folders shows up missing, and clips with no folder
    // does not show up.
    //    QDomElement kdenlivedoc_old = kdenlivedoc.toElement();
    if (!kdenlivedoc_old.isNull()) {
        QDomNodeList folders = kdenlivedoc_old.elementsByTagName("folder");
        int fsize = folders.size();
        int groupId = max_kproducer_id + 1; // Start at +1 of max id of the kdenlive_producers
        for (int i = 0; i < fsize; ++i) {
            QDomElement folder = folders.at(i).toElement();
            if (!folder.isNull()) {
                QString groupName = folder.attribute("name");
                kDebug() << "groupName: " << groupName << " with groupId: " << groupId;
                QDomNodeList fproducers = folder.elementsByTagName("producer");
                int psize = fproducers.size();
                for (int j = 0; j < psize; ++j) {
                    QDomElement fproducer = fproducers.at(j).toElement();
                    if (!fproducer.isNull()) {
                        QString id = fproducer.attribute("id");
                        // This is not very effective, but compared to loading the clips, its a breeze
                        QDomNodeList kdenlive_producers = kdenlivedoc_new.elementsByTagName("kdenlive_producer");
                        int kpsize = kdenlive_producers.size();
                        for (int k = 0; k < kpsize; ++k) {
                            QDomElement kproducer = kdenlive_producers.at(k).toElement(); // Its an element for sure
                            if (id == kproducer.attribute("id")) {
                                // We do not check that it already is part of a folder
                                kproducer.setAttribute("groupid", groupId);
                                kproducer.setAttribute("groupname", groupName);
                                break;
                            }
                        }
                    }
                }
                ++groupId;
            }
        }
    }
#endif
    westley0.appendChild(kdenlivedoc_new);

    QDomNodeList elements = westley.childNodes();
    max = elements.count();
    for (int i = 0; i < max; i++) {
        QDomElement prod = elements.at(0).toElement();
        westley0.insertAfter(prod, QDomNode());
    }

    westley0.removeChild(westley);

    // experimental and probably slow
    // adds <avfile /> information to <kdenlive_producer />
    QDomNodeList kproducers = m_document.elementsByTagName("kdenlive_producer");
    QDomNodeList avfiles = kdenlivedoc_old.elementsByTagName("avfile");
    kDebug() << "found" << avfiles.count() << "<avfile />s and" << kproducers.count() << "<kdenlive_producer />s";
    for (int i = 0; i < avfiles.count(); ++i) {
        QDomElement avfile = avfiles.at(i).toElement();
        QDomElement kproducer;
        if (avfile.isNull())
            kWarning() << "found an <avfile /> that is not a QDomElement";
        else {
            QString id = avfile.attribute("id");
            // this is horrible, must be rewritten, it's just for test
            for (int j = 0; j < kproducers.count(); ++j) {
                //kDebug() << "checking <kdenlive_producer /> with id" << kproducers.at(j).toElement().attribute("id");
                if (kproducers.at(j).toElement().attribute("id") == id) {
                    kproducer = kproducers.at(j).toElement();
                    break;
                }
            }
            if (kproducer == QDomElement())
                kWarning() << "no match for <avfile /> with id =" << id;
            else {
                //kDebug() << "ready to set additional <avfile />'s attributes (id =" << id << ")";
                kproducer.setAttribute("channels", avfile.attribute("channels"));
                kproducer.setAttribute("duration", avfile.attribute("duration"));
                kproducer.setAttribute("frame_size", avfile.attribute("width") + 'x' + avfile.attribute("height"));
                kproducer.setAttribute("frequency", avfile.attribute("frequency"));
                if (kproducer.attribute("description").isEmpty() && !avfile.attribute("description").isEmpty())
                    kproducer.setAttribute("description", avfile.attribute("description"));
            }
        }
    }

    /*kDebug() << "/////////////////  CONVERTED DOC:";
    kDebug() << m_document.toString();
    kDebug() << "/////////////////  END CONVERTED DOC:";

    QFile file("converted.kdenlive");
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << m_document.toString().toUtf8();
        file.close();
    } else {
        kDebug() << "Unable to dump file to converted.kdenlive";
    }*/

    //kDebug() << "/////////////////  END CONVERTED DOC:";

    return true;
}

QString KdenliveDoc::colorToString(const QColor& c)
{
    QString ret = "%1,%2,%3,%4";
    ret = ret.arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    return ret;
}

void KdenliveDoc::setZone(int start, int end)
{
    m_zoneStart = start;
    m_zoneEnd = end;
}

QPoint KdenliveDoc::zone() const
{
    return QPoint(m_zoneStart, m_zoneEnd);
}

bool KdenliveDoc::saveSceneList(const QString &path, const QString &scene)
{
    QDomDocument sceneList;
    sceneList.setContent(scene, true);
    QDomNode wes = sceneList.elementsByTagName("westley").at(0);
    QDomElement addedXml = sceneList.createElement("kdenlivedoc");
    wes.appendChild(addedXml);

    QDomElement markers = sceneList.createElement("markers");
    addedXml.setAttribute("version", "0.82");
    addedXml.setAttribute("kdenliveversion", VERSION);
    addedXml.setAttribute("profile", profilePath());
    addedXml.setAttribute("position", m_render->seekPosition().frames(m_fps));
    addedXml.setAttribute("zonein", m_zoneStart);
    addedXml.setAttribute("zoneout", m_zoneEnd);
    addedXml.setAttribute("projectfolder", m_projectFolder.path());
    addedXml.setAttribute("zoom", m_zoom);

    // tracks info
    QDomElement tracksinfo = sceneList.createElement("tracksinfo");
    foreach(const TrackInfo &info, m_tracksList) {
        QDomElement trackinfo = sceneList.createElement("trackinfo");
        if (info.type == AUDIOTRACK) trackinfo.setAttribute("type", "audio");
        trackinfo.setAttribute("mute", info.isMute);
        trackinfo.setAttribute("blind", info.isBlind);
        trackinfo.setAttribute("locked", info.isLocked);
        tracksinfo.appendChild(trackinfo);
    }
    addedXml.appendChild(tracksinfo);

    // save project folders
    QMap <QString, QString> folderlist = m_clipManager->documentFolderList();

    QMapIterator<QString, QString> f(folderlist);
    while (f.hasNext()) {
        f.next();
        QDomElement folder = sceneList.createElement("folder");
        folder.setAttribute("id", f.key());
        folder.setAttribute("name", f.value());
        addedXml.appendChild(folder);
    }

    // Save project clips
    QDomElement e;
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        e = list.at(i)->toXML();
        e.setTagName("kdenlive_producer");
        addedXml.appendChild(sceneList.importNode(e, true));
        QList < CommentedTime > marks = list.at(i)->commentedSnapMarkers();
        for (int j = 0; j < marks.count(); j++) {
            QDomElement marker = sceneList.createElement("marker");
            marker.setAttribute("time", marks.at(j).time().ms() / 1000);
            marker.setAttribute("comment", marks.at(j).comment());
            marker.setAttribute("id", e.attribute("id"));
            markers.appendChild(marker);
        }
    }
    addedXml.appendChild(markers);

    // Add guides
    if (!m_guidesXml.isNull()) addedXml.appendChild(sceneList.importNode(m_guidesXml.documentElement(), true));

    // Add clip groups
    addedXml.appendChild(sceneList.importNode(m_clipManager->groupsXml(), true));

    //wes.appendChild(doc.importNode(kdenliveData, true));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing to file: " << path;
        KMessageBox::error(kapp->activeWindow(), i18n("Cannot write to file %1", path));
        return false;
    }

    file.write(sceneList.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(kapp->activeWindow(), i18n("Cannot write to file %1", path));
        file.close();
        return false;
    }
    file.close();
    return true;
}

ClipManager *KdenliveDoc::clipManager()
{
    return m_clipManager;
}

KUrl KdenliveDoc::projectFolder() const
{
    //if (m_projectFolder.isEmpty()) return KUrl(KStandardDirs::locateLocal("appdata", "/projects/"));
    return m_projectFolder;
}

void KdenliveDoc::setProjectFolder(KUrl url)
{
    if (url == m_projectFolder) return;
    setModified(true);
    KStandardDirs::makeDir(url.path());
    KStandardDirs::makeDir(url.path() + "/titles/");
    KStandardDirs::makeDir(url.path() + "/thumbs/");
    if (KMessageBox::questionYesNo(kapp->activeWindow(), i18n("You have changed the project folder. Do you want to copy the cached data from %1 to the new folder %2?").arg(m_projectFolder.path(), url.path())) == KMessageBox::Yes) moveProjectData(url);
    m_projectFolder = url;
}

void KdenliveDoc::moveProjectData(KUrl url)
{
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    //TODO: Also move ladspa effects files
    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip->clipType() == TEXT) {
            // the image for title clip must be moved
            KUrl oldUrl = clip->fileURL();
            KUrl newUrl = KUrl(url.path() + "/titles/" + oldUrl.fileName());
            KIO::Job *job = KIO::copy(oldUrl, newUrl);
            if (KIO::NetAccess::synchronousRun(job, 0)) clip->setProperty("resource", newUrl.path());
        }
        QString hash = clip->getClipHash();
        KUrl oldVideoThumbUrl = KUrl(m_projectFolder.path() + "/thumbs/" + hash + ".png");
        KUrl oldAudioThumbUrl = KUrl(m_projectFolder.path() + "/thumbs/" + hash + ".thumb");
        if (KIO::NetAccess::exists(oldVideoThumbUrl, KIO::NetAccess::SourceSide, 0)) {
            KUrl newUrl = KUrl(url.path() + "/thumbs/" + hash + ".png");
            KIO::Job *job = KIO::copy(oldVideoThumbUrl, newUrl);
            KIO::NetAccess::synchronousRun(job, 0);
        }
        if (KIO::NetAccess::exists(oldAudioThumbUrl, KIO::NetAccess::SourceSide, 0)) {
            KUrl newUrl = KUrl(url.path() + "/thumbs/" + hash + ".thumb");
            KIO::Job *job = KIO::copy(oldAudioThumbUrl, newUrl);
            if (KIO::NetAccess::synchronousRun(job, 0)) clip->refreshThumbUrl();
        }
    }
}

const QString &KdenliveDoc::profilePath() const
{
    return m_profile.path;
}

MltVideoProfile KdenliveDoc::mltProfile() const
{
    return m_profile;
}

void KdenliveDoc::setProfilePath(QString path)
{
    if (path.isEmpty()) path = KdenliveSettings::default_profile();
    if (path.isEmpty()) path = "dv_pal";
    m_profile = ProfilesDialog::getVideoProfile(path);
    KdenliveSettings::setProject_display_ratio((double) m_profile.display_aspect_num / m_profile.display_aspect_den);
    m_fps = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
    KdenliveSettings::setProject_fps(m_fps);
    m_width = m_profile.width;
    m_height = m_profile.height;
    kDebug() << "KDEnnlive document, init timecode from path: " << path << ",  " << m_fps;
    if (m_fps == 30000.0 / 1001.0) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) m_fps);
}

double KdenliveDoc::dar()
{
    return (double) m_profile.display_aspect_num / m_profile.display_aspect_den;
}

void KdenliveDoc::setThumbsProgress(const QString &message, int progress)
{
    emit progressInfo(message, progress);
}

void KdenliveDoc::loadingProgressed()
{
    m_documentLoadingProgress += m_documentLoadingStep;
    emit progressInfo(QString(), (int) m_documentLoadingProgress);
}

QUndoStack *KdenliveDoc::commandStack()
{
    return m_commandStack;
}

/*
void KdenliveDoc::setRenderer(Render *render) {
    if (m_render) return;
    m_render = render;
    emit progressInfo(i18n("Loading playlist..."), 0);
    //qApp->processEvents();
    if (m_render) {
        m_render->setSceneList(m_document.toString(), m_startPos);
        kDebug() << "// SETTING SCENE LIST:\n\n" << m_document.toString();
        checkProjectClips();
    }
    emit progressInfo(QString(), -1);
}*/

void KdenliveDoc::checkProjectClips()
{
    kDebug() << "+++++++++++++ + + + + CHK PCLIPS";
    if (m_render == NULL) return;
    m_clipManager->resetProducersList(m_render->producersList());
    return;

    // Useless now...
    QList <Mlt::Producer *> prods = m_render->producersList();
    QString id ;
    QString prodId ;
    QString prodTrack ;
    for (int i = 0; i < prods.count(); i++) {
        id = prods.at(i)->get("id");
        prodId = id.section('_', 0, 0);
        prodTrack = id.section('_', 1, 1);
        DocClipBase *clip = m_clipManager->getClipById(prodId);
        if (clip) clip->setProducer(prods.at(i));
        if (clip && clip->clipType() == TEXT && !QFile::exists(clip->fileURL().path())) {
            // regenerate text clip image if required
            //kDebug() << "// TITLE: " << clip->getProperty("titlename") << " Preview file: " << clip->getProperty("resource") << " DOES NOT EXIST";
            QString titlename = clip->getProperty("name");
            QString titleresource;
            if (titlename.isEmpty()) {
                QStringList titleInfo = TitleWidget::getFreeTitleInfo(projectFolder());
                titlename = titleInfo.at(0);
                titleresource = titleInfo.at(1);
                clip->setProperty("name", titlename);
                kDebug() << "// New title set to: " << titlename;
            } else {
                titleresource = TitleWidget::getFreeTitleInfo(projectFolder()).at(1);
                //titleresource = TitleWidget::getTitleResourceFromName(projectFolder(), titlename);
            }
            TitleWidget *dia_ui = new TitleWidget(KUrl(), KUrl(titleresource).directory(), m_render, kapp->activeWindow());
            QDomDocument doc;
            doc.setContent(clip->getProperty("xmldata"));
            dia_ui->setXml(doc);
            QImage pix = dia_ui->renderedPixmap();
            pix.save(titleresource);
            clip->setProperty("resource", titleresource);
            delete dia_ui;
            clip->producer()->set("force_reload", 1);
        }
    }
}

void KdenliveDoc::updatePreviewSettings()
{
    m_clipManager->updatePreviewSettings();
    m_render->updatePreviewSettings();
    m_clipManager->resetProducersList(m_render->producersList());

}

Render *KdenliveDoc::renderer()
{
    return m_render;
}

void KdenliveDoc::updateClip(const QString &id)
{
    emit updateClipDisplay(id);
}

int KdenliveDoc::getFramePos(QString duration)
{
    return m_timecode.getFrameCount(duration, m_fps);
}

QString KdenliveDoc::producerName(const QString &id)
{
    QString result = "unnamed";
    QDomNodeList prods = producersList();
    int ct = prods.count();
    for (int i = 0; i <  ct ; i++) {
        QDomElement e = prods.item(i).toElement();
        if (e.attribute("id") != "black" && e.attribute("id") == id) {
            result = e.attribute("name");
            if (result.isEmpty()) result = KUrl(e.attribute("resource")).fileName();
            break;
        }
    }
    return result;
}

QDomDocument KdenliveDoc::toXml()
{
    return m_document;
}

Timecode KdenliveDoc::timecode() const
{
    return m_timecode;
}

QDomNodeList KdenliveDoc::producersList()
{
    return m_document.elementsByTagName("producer");
}

double KdenliveDoc::projectDuration() const
{
    if (m_render)
        return GenTime(m_render->getLength(), m_fps).ms() / 1000;
    else
        return 0;
}

double KdenliveDoc::fps() const
{
    return m_fps;
}

int KdenliveDoc::width() const
{
    return m_width;
}

int KdenliveDoc::height() const
{
    return m_height;
}

KUrl KdenliveDoc::url() const
{
    return m_url;
}

void KdenliveDoc::setUrl(KUrl url)
{
    m_url = url;
}

void KdenliveDoc::setModified(bool mod)
{
    if (!m_url.isEmpty() && mod && KdenliveSettings::crashrecovery()) {
        m_autoSaveTimer->start(3000);
    }
    if (mod == m_modified) return;
    m_modified = mod;
    emit docModified(m_modified);
}

bool KdenliveDoc::isModified() const
{
    return m_modified;
}

const QString KdenliveDoc::description() const
{
    if (m_url.isEmpty())
        return i18n("Untitled") + " / " + m_profile.description;
    else
        return m_url.fileName() + " / " + m_profile.description;
}

void KdenliveDoc::addClip(QDomElement elem, QString clipId, bool createClipItem)
{
    const QString producerId = clipId.section('_', 0, 0);
    DocClipBase *clip = m_clipManager->getClipById(producerId);
    bool placeHolder = false;
    if (clip == NULL) {
        elem.setAttribute("id", producerId);
        QString path = elem.attribute("resource");
        QString extension;
        if (elem.attribute("type").toInt() == SLIDESHOW) {
            extension = KUrl(path).fileName();
            path = KUrl(path).directory();
        } else if (elem.attribute("type").toInt() == TEXT && QFile::exists(path) == false) {
            kDebug() << "// TITLE: " << elem.attribute("name") << " Preview file: " << elem.attribute("resource") << " DOES NOT EXIST";
            QString titlename = elem.attribute("name");
            QString titleresource;
            if (titlename.isEmpty()) {
                QStringList titleInfo = TitleWidget::getFreeTitleInfo(projectFolder());
                titlename = titleInfo.at(0);
                titleresource = titleInfo.at(1);
                elem.setAttribute("name", titlename);
                kDebug() << "// New title set to: " << titlename;
            } else {
                titleresource = TitleWidget::getFreeTitleInfo(projectFolder()).at(1);
                //titleresource = TitleWidget::getTitleResourceFromName(projectFolder(), titlename);
            }
            TitleWidget *dia_ui = new TitleWidget(KUrl(), KUrl(titleresource).directory(), m_render, kapp->activeWindow());
            QDomDocument doc;
            doc.setContent(elem.attribute("xmldata"));
            dia_ui->setXml(doc);
            QImage pix = dia_ui->renderedPixmap();
            pix.save(titleresource);
            elem.setAttribute("resource", titleresource);
            setNewClipResource(clipId, titleresource);
            delete dia_ui;
        }

        if (path.isEmpty() == false && QFile::exists(path) == false && elem.attribute("type").toInt() != TEXT) {
            kDebug() << "// FOUND MISSING CLIP: " << path << ", TYPE: " << elem.attribute("type").toInt();
            const QString size = elem.attribute("file_size");
            const QString hash = elem.attribute("file_hash");
            QString newpath;
            int action = KMessageBox::No;
            if (!size.isEmpty() && !hash.isEmpty()) {
                if (!m_searchFolder.isEmpty()) newpath = searchFileRecursively(m_searchFolder, size, hash);
                else action = (KMessageBox::ButtonCode) KMessageBox::questionYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br>is invalid, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search automatically")), KGuiItem(i18n("Keep as placeholder")));
            } else {
                if (elem.attribute("type").toInt() == SLIDESHOW) {
                    int res = KMessageBox::questionYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br>is invalid or missing, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search manually")), KGuiItem(i18n("Keep as placeholder")));
                    if (res == KMessageBox::Yes)
                        newpath = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///clipfolder"), kapp->activeWindow(), i18n("Looking for %1", path));
                    else {
                        // Abort project loading
                        action = res;
                    }
                } else {
                    int res = KMessageBox::questionYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br>is invalid or missing, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search manually")), KGuiItem(i18n("Keep as placeholder")));
                    if (res == KMessageBox::Yes)
                        newpath = KFileDialog::getOpenFileName(KUrl("kfiledialog:///clipfolder"), QString(), kapp->activeWindow(), i18n("Looking for %1", path));
                    else {
                        // Abort project loading
                        action = res;
                    }
                }
            }
            if (action == KMessageBox::Yes) {
                kDebug() << "// ASKED FOR SRCH CLIP: " << clipId;
                m_searchFolder = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///clipfolder"), kapp->activeWindow());
                if (!m_searchFolder.isEmpty()) {
                    newpath = searchFileRecursively(QDir(m_searchFolder), size, hash);
                }
            } else if (action == KMessageBox::Cancel) {
                m_abortLoading = true;
                return;
            } else if (action == KMessageBox::No) {
                // Keep clip as placeHolder
                placeHolder = true;
            }
            if (!newpath.isEmpty()) {
                if (elem.attribute("type").toInt() == SLIDESHOW) newpath.append('/' + extension);
                elem.setAttribute("resource", newpath);
                setNewClipResource(clipId, newpath);
                setModified(true);
            }
        }
        clip = new DocClipBase(m_clipManager, elem, producerId, placeHolder);
        m_clipManager->addClip(clip);
    }

    if (createClipItem) {
        emit addProjectClip(clip);
        qApp->processEvents();
        m_render->getFileProperties(clip->toXML(), clip->getId());
    }
}


void KdenliveDoc::setNewClipResource(const QString &id, const QString &path)
{
    QDomNodeList prods = m_document.elementsByTagName("producer");
    int maxprod = prods.count();
    for (int i = 0; i < maxprod; i++) {
        QDomNode m = prods.at(i);
        QString prodId = m.toElement().attribute("id");
        if (prodId == id || prodId.startsWith(id + '_')) {
            QDomNodeList params = m.childNodes();
            for (int j = 0; j < params.count(); j++) {
                QDomElement e = params.item(j).toElement();
                if (e.attribute("name") == "resource") {
                    e.firstChild().setNodeValue(path);
                    break;
                }
            }
        }
    }
}

QString KdenliveDoc::searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const
{
    QString foundFileName;
    QByteArray fileData;
    QByteArray fileHash;
    QStringList filesAndDirs = dir.entryList(QDir::Files | QDir::Readable);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); i++) {
        QFile file(dir.absoluteFilePath(filesAndDirs.at(i)));
        if (file.open(QIODevice::ReadOnly)) {
            if (QString::number(file.size()) == matchSize) {
                /*
                * 1 MB = 1 second per 450 files (or faster)
                * 10 MB = 9 seconds per 450 files (or faster)
                */
                if (file.size() > 1000000*2) {
                    fileData = file.read(1000000);
                    if (file.seek(file.size() - 1000000))
                        fileData.append(file.readAll());
                } else
                    fileData = file.readAll();
                file.close();
                fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
                if (QString(fileHash.toHex()) == matchHash)
                    return file.fileName();
            }
        }
        kDebug() << filesAndDirs.at(i) << file.size() << fileHash.toHex();
    }
    filesAndDirs = dir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); i++) {
        foundFileName = searchFileRecursively(dir.absoluteFilePath(filesAndDirs.at(i)), matchSize, matchHash);
        if (!foundFileName.isEmpty())
            break;
    }
    return foundFileName;
}

void KdenliveDoc::addClipInfo(QDomElement elem, QDomElement orig, QString clipId)
{
    DocClipBase *clip = m_clipManager->getClipById(clipId);
    if (clip == NULL) {
        addClip(elem, clipId, false);
    } else {
        QMap <QString, QString> properties;
        QDomNamedNodeMap attributes = elem.attributes();
        QString attrname;
        for (int i = 0; i < attributes.count(); i++) {
            attrname = attributes.item(i).nodeName();
            if (attrname != "resource")
                properties.insert(attrname, attributes.item(i).nodeValue());
            kDebug() << attrname << " = " << attributes.item(i).nodeValue();
        }
        clip->setProperties(properties);
        emit addProjectClip(clip, false);
    }
    if (orig != QDomElement()) {
        QMap<QString, QString> meta;
        QDomNode m = orig.firstChild();
        while (!m.isNull()) {
            QString name = m.toElement().attribute("name");
            if (name.startsWith("meta.attr")) {
                meta.insert(name.section('.', 2, 3), m.firstChild().nodeValue());
            }
            m = m.nextSibling();
        }
        if (!meta.isEmpty()) {
            if (clip == NULL) clip = m_clipManager->getClipById(clipId);
            if (clip) clip->setMetadata(meta);
        }
    }
}

void KdenliveDoc::deleteProjectClip(QList <QString> ids)
{
    for (int i = 0; i < ids.size(); ++i) {
        emit deleteTimelineClip(ids.at(i));
        m_clipManager->slotDeleteClip(ids.at(i));
    }
    setModified(true);
}

void KdenliveDoc::deleteClip(const QString &clipId)
{
    emit signalDeleteProjectClip(clipId);
    m_clipManager->deleteClip(clipId);
}

void KdenliveDoc::slotAddClipList(const KUrl::List urls, const QString group, const QString &groupId)
{
    m_clipManager->slotAddClipList(urls, group, groupId);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
    setModified(true);
}


void KdenliveDoc::slotAddClipFile(const KUrl url, const QString group, const QString &groupId)
{
    //kDebug() << "/////////  DOCUM, ADD CLP: " << url;
    m_clipManager->slotAddClipFile(url, group, groupId);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
    setModified(true);
}

const QString KdenliveDoc::getFreeClipId()
{
    return QString::number(m_clipManager->getFreeClipId());
}

DocClipBase *KdenliveDoc::getBaseClip(const QString &clipId)
{
    return m_clipManager->getClipById(clipId);
}

void KdenliveDoc::slotCreateTextClip(QString /*group*/, const QString &/*groupId*/)
{
    QString titlesFolder = projectFolder().path() + "/titles/";
    KStandardDirs::makeDir(titlesFolder);
    TitleWidget *dia_ui = new TitleWidget(KUrl(), titlesFolder, m_render, kapp->activeWindow());
    if (dia_ui->exec() == QDialog::Accepted) {
        QStringList titleInfo = TitleWidget::getFreeTitleInfo(projectFolder());
        QImage pix = dia_ui->renderedPixmap();
        pix.save(titleInfo.at(1));
        //dia_ui->saveTitle(path + ".kdenlivetitle");
        m_clipManager->slotAddTextClipFile(titleInfo.at(0), titleInfo.at(1), dia_ui->xml().toString(), QString(), QString());
        setModified(true);
    }
    delete dia_ui;
}

int KdenliveDoc::tracksCount() const
{
    return m_tracksList.count();
}

TrackInfo KdenliveDoc::trackInfoAt(int ix) const
{
    return m_tracksList.at(ix);
}

void KdenliveDoc::switchTrackAudio(int ix, bool hide)
{
    m_tracksList[ix].isMute = hide; // !m_tracksList.at(ix).isMute;
}

void KdenliveDoc::switchTrackLock(int ix, bool lock)
{
    m_tracksList[ix].isLocked = lock;
}

bool KdenliveDoc::isTrackLocked(int ix) const
{
    return m_tracksList.at(ix).isLocked;
}

void KdenliveDoc::switchTrackVideo(int ix, bool hide)
{
    m_tracksList[ix].isBlind = hide; // !m_tracksList.at(ix).isBlind;
}

void KdenliveDoc::insertTrack(int ix, TrackInfo type)
{
    if (ix == -1) m_tracksList << type;
    else m_tracksList.insert(ix, type);
}

void KdenliveDoc::deleteTrack(int ix)
{
    m_tracksList.removeAt(ix);
}

void KdenliveDoc::setTrackType(int ix, TrackInfo type)
{
    m_tracksList[ix].type = type.type;
    m_tracksList[ix].isMute = type.isMute;
    m_tracksList[ix].isBlind = type.isBlind;
    m_tracksList[ix].isLocked = type.isLocked;
}

const QList <TrackInfo> KdenliveDoc::tracksList() const
{
    return m_tracksList;
}

QPoint KdenliveDoc::getTracksCount() const
{
    int audio = 0;
    int video = 0;
    foreach(const TrackInfo &info, m_tracksList) {
        if (info.type == VIDEOTRACK) video++;
        else audio++;
    }
    return QPoint(video, audio);
}

void KdenliveDoc::cachePixmap(const QString &fileId, const QPixmap &pix) const
{
    pix.save(m_projectFolder.path() + "/thumbs/" + fileId + ".png");
}

QString KdenliveDoc::getLadspaFile() const
{
    int ct = 0;
    QString counter = QString::number(ct).rightJustified(5, '0', false);
    while (QFile::exists(m_projectFolder.path() + "/ladspa/" + counter + ".ladspa")) {
        ct++;
        counter = QString::number(ct).rightJustified(5, '0', false);
    }
    return m_projectFolder.path() + "/ladspa/" + counter + ".ladspa";
}

void KdenliveDoc::checkDocumentClips()
{
    DocumentChecker d(m_document);
    d.exec();
}


#include "kdenlivedoc.moc"

