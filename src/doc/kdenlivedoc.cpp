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
#include "documentchecker.h"
#include "documentvalidator.h"
#include "mltcontroller/clipcontroller.h"
#include <config-kdenlive.h>
#include "kdenlivesettings.h"
#include "renderer.h"
#include "mainwindow.h"
#include "project/clipmanager.h"
#include "project/projectcommands.h"
#include "bin/bincommands.h"
#include "project/projectlist.h"
#include "effectslist/initeffects.h"
#include "dialogs/profilesdialog.h"
#include "titler/titlewidget.h"
#include "project/notesplugin.h"
#include "project/dialogs/noteswidget.h"
#include "core.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "mltcontroller/bincontroller.h"
#include "mltcontroller/effectscontroller.h"

#include <KMessageBox>
#include <KRecentDirs>
#include <klocalizedstring.h>
#include <KIO/CopyJob>
#include <KIO/JobUiDelegate>
#include <KBookmarkManager>
#include <KBookmark>

#include <QProgressDialog>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QDomImplementation>
#include <QUndoGroup>
#include <QTimer>
#include <QUndoStack>
#include <QTextEdit>

#include <mlt++/Mlt.h>
#include <KJobWidgets/KJobWidgets>
#include <QStandardPaths>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

const double DOCUMENTVERSION = 0.91;

KdenliveDoc::KdenliveDoc(const QUrl &url, const QUrl &projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QMap <QString, QString>& properties, const QMap <QString, QString>& metadata, const QPoint &tracks, Render *render, NotesPlugin *notes, bool *openBackup, MainWindow *parent, QProgressDialog *progressDialog) :
    QObject(parent),
    m_autosave(NULL),
    m_url(url),
    m_render(render),
    m_notesWidget(notes->widget()),
    m_commandStack(new QUndoStack(undoGroup)),
    m_modified(false),
    m_projectFolder(projectFolder)
{
    // init m_profile struct
    m_profile.frame_rate_num = 0;
    m_profile.frame_rate_den = 0;
    m_profile.width = 0;
    m_profile.height = 0;
    m_profile.progressive = 0;
    m_profile.sample_aspect_num = 0;
    m_profile.sample_aspect_den = 0;
    m_profile.display_aspect_num = 0;
    m_profile.display_aspect_den = 0;
    m_profile.colorspace = 0;

    m_clipManager = new ClipManager(this);
    connect(m_clipManager, SIGNAL(displayMessage(QString,int)), parent, SLOT(slotGotProgressInfo(QString,int)));
    bool success = false;
    connect(m_commandStack, SIGNAL(indexChanged(int)), this, SLOT(slotModified()));
    connect(m_render, SIGNAL(setDocumentNotes(QString)), this, SLOT(slotSetDocumentNotes(QString)));
    //connect(m_commandStack, SIGNAL(cleanChanged(bool)), this, SLOT(setModified(bool)));

    // Init clip modification tracker
    m_modifiedTimer.setInterval(1500);
    connect(&m_fileWatcher, &KDirWatch::dirty, this, &KdenliveDoc::slotClipModified);
    connect(&m_fileWatcher, &KDirWatch::deleted, this, &KdenliveDoc::slotClipMissing);
    connect(&m_modifiedTimer, &QTimer::timeout, this, &KdenliveDoc::slotProcessModifiedClips);

    // init default document properties
    m_documentProperties["zoom"] = '7';
    m_documentProperties["verticalzoom"] = '1';
    m_documentProperties["zonein"] = '0';
    m_documentProperties["zoneout"] = "100";
    m_documentProperties["enableproxy"] = QString::number((int) KdenliveSettings::enableproxy());
    m_documentProperties["proxyparams"] = KdenliveSettings::proxyparams();
    m_documentProperties["proxyextension"] = KdenliveSettings::proxyextension();
    m_documentProperties["generateproxy"] = QString::number((int) KdenliveSettings::generateproxy());
    m_documentProperties["proxyminsize"] = QString::number(KdenliveSettings::proxyminsize());
    m_documentProperties["generateimageproxy"] = QString::number((int) KdenliveSettings::generateimageproxy());
    m_documentProperties["proxyimageminsize"] = QString::number(KdenliveSettings::proxyimageminsize());
    m_documentProperties["documentid"] = QString::number(QDateTime::currentMSecsSinceEpoch());

    // Load properties
    QMapIterator<QString, QString> i(properties);
    while (i.hasNext()) {
        i.next();
        m_documentProperties[i.key()] = i.value();
    }

    // Load metadata
    QMapIterator<QString, QString> j(metadata);
    while (j.hasNext()) {
        j.next();
        m_documentMetadata[j.key()] = j.value();
    }

    if (QLocale().decimalPoint() != QLocale::system().decimalPoint()) {
        setlocale(LC_NUMERIC, "");
        QLocale systemLocale = QLocale::system();
        systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
        QLocale::setDefault(systemLocale);
        // locale conversion might need to be redone
        initEffects::parseEffectFiles(pCore->binController()->mltRepository(), setlocale(LC_NUMERIC, NULL));
    }

    *openBackup = false;
    
    if (url.isValid()) {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // The file cannot be opened
            if (KMessageBox::warningContinueCancel(parent, i18n("Cannot open the project file,\nDo you want to open a backup file?"), i18n("Error opening file"), KGuiItem(i18n("Open Backup"))) == KMessageBox::Continue) {
                *openBackup = true;
            }
            //KMessageBox::error(parent, KIO::NetAccess::lastErrorString());
        }
        else {
            qDebug()<<" // / processing file open";
            QString errorMsg;
            int line;
            int col;
            QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidChars);
            success = m_document.setContent(&file, false, &errorMsg, &line, &col);
            file.close();

            if (!success) {
                // It is corrupted
                int answer = KMessageBox::warningYesNoCancel (parent, i18n("Cannot open the project file, error is:\n%1 (line %2, col %3)\nDo you want to open a backup file?", errorMsg, line, col), i18n("Error opening file"), KGuiItem(i18n("Open Backup")), KGuiItem(i18n("Recover")));
                if (answer == KMessageBox::Yes) {
                    *openBackup = true;
                }
                else if (answer == KMessageBox::No) {
                    // Try to recover broken file produced by Kdenlive 0.9.4
                    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        int correction = 0;
                        QString playlist = file.readAll();
                        while (!success && correction < 2) {
                            int errorPos = 0;
                            line--;
                            col = col - 2;
                            for (int j = 0; j < line && errorPos < playlist.length(); ++j) {
                                errorPos = playlist.indexOf("\n", errorPos);
                                errorPos++;
                            }
                            errorPos += col;
                            if (errorPos >= playlist.length()) break;
                            playlist.remove(errorPos, 1);
                            line = 0;
                            col = 0;
                            success = m_document.setContent(playlist, false, &errorMsg, &line, &col);
                            correction++;
                        }
                        if (!success) {
                            KMessageBox::sorry(parent, i18n("Cannot recover this project file"));
                        }
                        else {
                            // Document was modified, ask for backup
                            QDomElement mlt = m_document.documentElement();
                            mlt.setAttribute("modified", 1);
                        }
                    }
                }
            }
            else {
                qDebug()<<" // / processing file open: validate";
                parent->slotGotProgressInfo(i18n("Validating"), 0);
                qApp->processEvents();
                DocumentValidator validator(m_document, url);
                success = validator.isProject();
                if (!success) {
                    // It is not a project file
                    parent->slotGotProgressInfo(i18n("File %1 is not a Kdenlive project file", m_url.path()), 100);
                    if (KMessageBox::warningContinueCancel(parent, i18n("File %1 is not a valid project file.\nDo you want to open a backup file?", m_url.path()), i18n("Error opening file"), KGuiItem(i18n("Open Backup"))) == KMessageBox::Continue) {
                        *openBackup = true;
                    }
                } else {
                    /*
                     * Validate the file against the current version (upgrade
                     * and recover it if needed). It is NOT a passive operation
                     */
                    // TODO: backup the document or alert the user?
                    success = validator.validate(DOCUMENTVERSION);
                    if (success && !KdenliveSettings::gpu_accel()) {
                        success = validator.checkMovit();
                    }
                    if (success) { // Let the validator handle error messages
                        qDebug()<<" // / processing file validate ok";
                        parent->slotGotProgressInfo(i18n("Check missing clips"), 0);
                        qApp->processEvents();
                        success = checkDocumentClips();
                        if (success) {
                            loadDocumentProperties();
                            if (m_document.documentElement().attribute("modified") == "1") setModified(true);
                            parent->slotGotProgressInfo(i18n("Loading"), 0);
                            QDomElement mlt = m_document.firstChildElement("mlt");
                            QDomElement infoXml = mlt.firstChildElement("kdenlivedoc");

                            QDomElement e;
                            QStringList expandedFolders;
                            QDomNodeList folders = m_document.elementsByTagName("folder");
                            for (int i = 0; i < folders.count(); ++i) {
                                e = folders.item(i).cloneNode().toElement();
                                if (e.hasAttribute("opened")) expandedFolders.append(e.attribute("id"));
                                //Deprecated
                                //m_clipManager->addFolder(e.attribute("id"), e.attribute("name"));
                            }
                            m_documentProperties["expandedfolders"] = expandedFolders.join(";");

                            QDomNodeList producers = m_document.elementsByTagName("producer");
                            const int max = producers.count();

                            if (!progressDialog) {
                                progressDialog = new QProgressDialog(parent);
                                progressDialog->setWindowTitle(i18n("Loading project"));
                                progressDialog->setLabelText(i18n("Adding clips"));
                                progressDialog->setCancelButton(0);
                            } else {
                                progressDialog->setLabelText(i18n("Adding clips"));
                            }
                            progressDialog->setMaximum(max);
                            progressDialog->show();
                            qApp->processEvents();
                            if (success) {
                                /*QDomElement markers = infoXml.firstChildElement("markers");
                                if (!markers.isNull()) {
                                    QDomNodeList markerslist = markers.childNodes();
                                    int maxchild = markerslist.count();
                                    for (int k = 0; k < maxchild; ++k) {
                                        e = markerslist.at(k).toElement();
                                        if (e.tagName() == "marker") {
                                            CommentedTime marker(GenTime(e.attribute("time").toDouble()), e.attribute("comment"), e.attribute("type").toInt());
                                            ClipController *controller = getClipController(e.attribute("id"));
                                            if (controller) controller->addSnapMarker(marker);
                                            else qDebug()<< " / / Warning, missing clip: "<< e.attribute("id");
                                        }
                                    }
                                    infoXml.removeChild(markers);
                                }

                                m_projectFolder = QUrl::fromLocalFile(infoXml.attribute("projectfolder"));*/
                                /*QDomElement docproperties = infoXml.firstChildElement("documentproperties");
                                QDomNamedNodeMap props = docproperties.attributes();
                                for (int i = 0; i < props.count(); ++i)
                                    m_documentProperties.insert(props.item(i).nodeName(), props.item(i).nodeValue());
                                docproperties = infoXml.firstChildElement("documentmetadata");
                                props = docproperties.attributes();
                                for (int i = 0; i < props.count(); ++i)
                                    m_documentMetadata.insert(props.item(i).nodeName(), props.item(i).nodeValue());
                                */

                                if (validator.isModified()) setModified(true);
                                //qDebug() << "Reading file: " << url.path() << ", found clips: " << producers.count();
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Something went wrong, or a new file was requested: create a new project
    if (!success) {
        m_url.clear();
        m_profile = ProfilesDialog::getVideoProfile(profileName);
        m_document = createEmptyDocument(tracks.x(), tracks.y());
        updateProjectProfile();
    }

    // Ask to create the project directory if it does not exist
    if (!QFile::exists(m_projectFolder.path())) {
        int create = KMessageBox::questionYesNo(parent, i18n("Project directory %1 does not exist. Create it?", m_projectFolder.path()));
        if (create == KMessageBox::Yes) {
            QDir projectDir(m_projectFolder.path());
            bool ok = projectDir.mkpath(m_projectFolder.path());
            if (!ok) {
                KMessageBox::sorry(parent, i18n("The directory %1, could not be created.\nPlease make sure you have the required permissions.", m_projectFolder.path()));
            }
        }
    }

    // Make sure the project folder is usable
    if (m_projectFolder.isEmpty() || !QFile::exists(m_projectFolder.path())) {
        KMessageBox::information(parent, i18n("Document project folder is invalid, setting it to the default one: %1", KdenliveSettings::defaultprojectfolder()));
        m_projectFolder = QUrl::fromLocalFile(KdenliveSettings::defaultprojectfolder());
    }

    // Make sure that the necessary folders exist
    QDir dir(m_projectFolder.path());
    dir.mkdir("titles");
    dir.mkdir("thumbs");
    dir.mkdir("proxy");
    dir.mkdir(".backup");

    updateProjectFolderPlacesEntry();

    ////qDebug() << "// SETTING SCENE LIST:\n\n" << m_document.toString();
    connect(m_render, SIGNAL(addClip(const QString&,const QMap<QString,QString>&)), pCore->bin(), SLOT(slotAddUrl(const QString&,const QMap<QString,QString>&)));
}

void KdenliveDoc::slotSetDocumentNotes(const QString &notes)
{
    m_notesWidget->setHtml(notes);
}


KdenliveDoc::~KdenliveDoc()
{
    delete m_commandStack;
    //qDebug() << "// DEL CLP MAN";
    delete m_clipManager;
    //qDebug() << "// DEL CLP MAN done";
    if (m_autosave) {
        if (!m_autosave->fileName().isEmpty()) m_autosave->remove();
        delete m_autosave;
    }
}

int KdenliveDoc::setSceneList()
{
    //m_render->resetProfile(m_profile);
    pCore->bin()->isLoading = true;
    if (m_render->setSceneList(m_document.toString(), m_documentProperties.value("position").toInt()) == -1) {
        // INVALID MLT Consumer, something is wrong
        return -1;
    }
    pCore->bin()->isLoading = false;
    pCore->binController()->checkThumbnails(projectFolder().path() + "/thumbs/");
    m_documentProperties.remove("position");
    return 0;
}

QDomDocument KdenliveDoc::createEmptyDocument(int videotracks, int audiotracks)
{
    QList <TrackInfo> tracks;
    // Tracks are added «backwards», so we need to reverse the track numbering
    // mbt 331: http://www.kdenlive.org/mantis/view.php?id=331
    // Better default names for tracks: Audio 1 etc. instead of blank numbers
    for (int i = 0; i < audiotracks; ++i) {
        TrackInfo audioTrack;
        audioTrack.type = AudioTrack;
        audioTrack.isMute = false;
        audioTrack.isBlind = true;
        audioTrack.isLocked = false;
        audioTrack.trackName = QString("Audio ") + QString::number(audiotracks - i);
        audioTrack.duration = 0;
        audioTrack.effectsList = EffectsList(true);
        tracks.append(audioTrack);

    }
    for (int i = 0; i < videotracks; ++i) {
        TrackInfo videoTrack;
        videoTrack.type = VideoTrack;
        videoTrack.isMute = false;
        videoTrack.isBlind = false;
        videoTrack.isLocked = false;
        videoTrack.trackName = QString("Video ") + QString::number(videotracks - i);
        videoTrack.duration = 0;
        videoTrack.effectsList = EffectsList(true);
        tracks.append(videoTrack);
    }
    return createEmptyDocument(tracks);
}

QDomDocument KdenliveDoc::createEmptyDocument(const QList <TrackInfo> &tracks)
{
    // Creating new document
    QDomDocument doc;
    QDomElement mlt = doc.createElement("mlt");
    mlt.setAttribute("LC_NUMERIC", "");
    doc.appendChild(mlt);
    
    // Create black producer
    // For some unknown reason, we have to build the black producer here and not in renderer.cpp, otherwise
    // the composite transitions with the black track are corrupted.
    /*QDomElement pro = doc.createElement("profile");
    pro.setAttribute("frame_rate_den", QString::number(m_profile.frame_rate_den));
    pro.setAttribute("frame_rate_num", QString::number(m_profile.frame_rate_num));
    pro.setAttribute("display_aspect_den", QString::number(m_profile.display_aspect_den));
    pro.setAttribute("display_aspect_num", QString::number(m_profile.display_aspect_num));
    pro.setAttribute("sample_aspect_den", QString::number(m_profile.sample_aspect_den));
    pro.setAttribute("sample_aspect_num", QString::number(m_profile.sample_aspect_num));
    pro.setAttribute("width", QString::number(m_profile.width));
    pro.setAttribute("height", QString::number(m_profile.height));
    pro.setAttribute("progressive", QString::number((int) m_profile.progressive));
    pro.setAttribute("colorspace", QString::number(m_profile.colorspace));
    pro.setAttribute("description", m_profile.description);
    mlt.appendChild(pro);*/

    QDomElement blk = doc.createElement("producer");
    blk.setAttribute("in", 0);
    blk.setAttribute("out", 500);
    blk.setAttribute("id", "black");

    QDomElement property = doc.createElement("property");
    property.setAttribute("name", "mlt_type");
    QDomText value = doc.createTextNode("producer");
    property.appendChild(value);
    blk.appendChild(property);

    property = doc.createElement("property");
    property.setAttribute("name", "aspect_ratio");
    value = doc.createTextNode(QString::number(0));
    property.appendChild(value);
    blk.appendChild(property);

    property = doc.createElement("property");
    property.setAttribute("name", "length");
    value = doc.createTextNode(QString::number(15000));
    property.appendChild(value);
    blk.appendChild(property);

    property = doc.createElement("property");
    property.setAttribute("name", "eof");
    value = doc.createTextNode("pause");
    property.appendChild(value);
    blk.appendChild(property);

    property = doc.createElement("property");
    property.setAttribute("name", "resource");
    value = doc.createTextNode("black");
    property.appendChild(value);
    blk.appendChild(property);

    property = doc.createElement("property");
    property.setAttribute("name", "mlt_service");
    value = doc.createTextNode("colour");
    property.appendChild(value);
    blk.appendChild(property);

    mlt.appendChild(blk);


    QDomElement tractor = doc.createElement("tractor");
    tractor.setAttribute("id", "maintractor");
    tractor.setAttribute("global_feed", 1);
    //QDomElement multitrack = doc.createElement("multitrack");
    QDomElement playlist = doc.createElement("playlist");
    playlist.setAttribute("id", "black_track");
    
    mlt.appendChild(playlist);

    QDomElement blank0 = doc.createElement("entry");
    blank0.setAttribute("in", "0");
    blank0.setAttribute("out", "1");
    blank0.setAttribute("producer", "black");
    playlist.appendChild(blank0);

    // create playlists
    int total = tracks.count();

    for (int i = 0; i < total; ++i) {
        QDomElement playlist = doc.createElement("playlist");
        playlist.setAttribute("id", "playlist" + QString::number(i));
        playlist.setAttribute("kdenlive:track_name", tracks.at(i).trackName);
        if (tracks.at(i).type == AudioTrack) {
            playlist.setAttribute("kdenlive:audio_track", 1);
        }
        mlt.appendChild(playlist);
    }

    QDomElement track0 = doc.createElement("track");
    track0.setAttribute("producer", "black_track");
    tractor.appendChild(track0);

    // create audio and video tracks
    for (int i = 0; i < total; ++i) {
        QDomElement track = doc.createElement("track");
        track.setAttribute("producer", "playlist" + QString::number(i));
        if (tracks.at(i).type == AudioTrack) {
            track.setAttribute("hide", "video");
        } else if (tracks.at(i).isBlind) {
            if (tracks.at(i).isMute) {
                track.setAttribute("hide", "all");
            }
            else track.setAttribute("hide", "video");
        }
        else if (tracks.at(i).isMute)
            track.setAttribute("hide", "audio");
        tractor.appendChild(track);
    }

    for (int i = 2; i < total + 1 ; ++i) {
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
    mlt.appendChild(tractor);
    return doc;
}

bool KdenliveDoc::useProxy() const
{
    return m_documentProperties.value("enableproxy").toInt();
}


void KdenliveDoc::slotAutoSave()
{
    if (m_render && m_autosave) {
        if (!m_autosave->isOpen() && !m_autosave->open(QIODevice::ReadWrite)) {
            // show error: could not open the autosave file
            qDebug() << "ERROR; CANNOT CREATE AUTOSAVE FILE";
        }
        //qDebug() << "// AUTOSAVE FILE: " << m_autosave->fileName();
        QDomDocument sceneList = xmlSceneList(m_render->sceneList());
        if (sceneList.isNull()) {
            //Make sure we don't save if scenelist is corrupted
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1, scene list is corrupted.", m_autosave->fileName()));
            return;
        }
        m_autosave->resize(0); 
        m_autosave->write(sceneList.toString().toUtf8());
        m_autosave->flush();
    }
}

void KdenliveDoc::setZoom(int horizontal, int vertical)
{
    m_documentProperties["zoom"] = QString::number(horizontal);
    m_documentProperties["verticalzoom"] = QString::number(vertical);
}

QPoint KdenliveDoc::zoom() const
{
    return QPoint(m_documentProperties.value("zoom").toInt(), m_documentProperties.value("verticalzoom").toInt());
}

void KdenliveDoc::setZone(int start, int end)
{
    m_documentProperties["zonein"] = QString::number(start);
    m_documentProperties["zoneout"] = QString::number(end);
}

QPoint KdenliveDoc::zone() const
{
    return QPoint(m_documentProperties.value("zonein").toInt(), m_documentProperties.value("zoneout").toInt());
}

QDomDocument KdenliveDoc::xmlSceneList(const QString &scene)
{
    QDomDocument sceneList;
    sceneList.setContent(scene, true);
    QDomElement mlt = sceneList.firstChildElement("mlt");
    if (mlt.isNull() || !mlt.hasChildNodes()) {
        //scenelist is corrupted
        return sceneList;
    }

    // Set playlist audio volume to 100%
    QDomElement tractor = mlt.firstChildElement("tractor");
    if (!tractor.isNull()) {
        QDomNodeList props = tractor.elementsByTagName("property");
        for (int i = 0; i < props.count(); ++i) {
            if (props.at(i).toElement().attribute("name") == "meta.volume") {
                props.at(i).firstChild().setNodeValue("1");
                break;
            }
        }
    }
    QDomNodeList pls = mlt.elementsByTagName("playlist");
    QDomElement mainPlaylist;
    for (int i = 0; i < pls.count(); ++i) {
        if (pls.at(i).toElement().attribute("id") == pCore->binController()->binPlaylistId()) {
            mainPlaylist = pls.at(i).toElement();
            break;
        }
    }

    // check if project contains custom effects to embed them in project file
    QDomNodeList effects = mlt.elementsByTagName("filter");
    int maxEffects = effects.count();
    //qDebug() << "// FOUD " << maxEffects << " EFFECTS+++++++++++++++++++++";
    QMap <QString, QString> effectIds;
    for (int i = 0; i < maxEffects; ++i) {
        QDomNode m = effects.at(i);
        QDomNodeList params = m.childNodes();
        QString id;
        QString tag;
        for (int j = 0; j < params.count(); ++j) {
            QDomElement e = params.item(j).toElement();
            if (e.attribute("name") == "kdenlive_id") {
                id = e.firstChild().nodeValue();
            }
            if (e.attribute("name") == "tag") {
                tag = e.firstChild().nodeValue();
            }
            if (!id.isEmpty() && !tag.isEmpty()) effectIds.insert(id, tag);
        }
    }
    //TODO: find a way to process this before rendering MLT scenelist to xml
    QDomDocument customeffects = initEffects::getUsedCustomEffects(effectIds);
    mainPlaylist.setAttribute("kdenlive:customeffects", customeffects.toString());
    //addedXml.appendChild(sceneList.importNode(customeffects.documentElement(), true));

    //TODO: move metadata to previous step in saving process
    QDomElement docmetadata = sceneList.createElement("documentmetadata");
    QMapIterator<QString, QString> j(m_documentMetadata);
    while (j.hasNext()) {
        j.next();
        docmetadata.setAttribute(j.key(), j.value());
    }
    //addedXml.appendChild(docmetadata);

    return sceneList;
}

QString KdenliveDoc::documentNotes() const
{
    QString text = m_notesWidget->toPlainText().simplified();
    if (text.isEmpty()) return QString();
    return m_notesWidget->toHtml();
}

bool KdenliveDoc::saveSceneList(const QString &path, const QString &scene)
{
    QDomDocument sceneList = xmlSceneList(scene);
    if (sceneList.isNull()) {
        //Make sure we don't save if scenelist is corrupted
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1, scene list is corrupted.", path));
        return false;
    }
    
    // Backup current version
    backupLastSavedVersion(path);
    QFile file(path);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "//////  ERROR writing to file: " << path;
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", path));
        return false;
    }

    file.write(sceneList.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", path));
        file.close();
        return false;
    }
    file.close();
    cleanupBackupFiles();
    QFileInfo info(file);
    QString fileName = QUrl::fromLocalFile(path).fileName().section('.', 0, -2);
    fileName.append('-' + m_documentProperties.value("documentid"));
    fileName.append(info.lastModified().toString("-yyyy-MM-dd-hh-mm"));
    fileName.append(".kdenlive.png");
    QDir backupFolder(m_projectFolder.path() + "/.backup");
    emit saveTimelinePreview(backupFolder.absoluteFilePath(fileName));
    return true;
}

ClipManager *KdenliveDoc::clipManager()
{
    return m_clipManager;
}

QString KdenliveDoc::groupsXml() const
{
    return m_clipManager->groupsXml();
}

QUrl KdenliveDoc::projectFolder() const
{
    //if (m_projectFolder.isEmpty()) return QUrl(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "//projects/");
    return m_projectFolder;
}

void KdenliveDoc::setProjectFolder(QUrl url)
{
    if (url == m_projectFolder) return;
    setModified(true);
    QDir dir(url.toLocalFile());
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }
    dir.mkdir("titles");
    dir.mkdir("thumbs");
    dir.mkdir("proxy");
    dir.mkdir(".backup");
    if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("You have changed the project folder. Do you want to copy the cached data from %1 to the new folder %2?", m_projectFolder.path(), url.path())) == KMessageBox::Yes) moveProjectData(url);
    m_projectFolder = url;

    updateProjectFolderPlacesEntry();
}

void KdenliveDoc::moveProjectData(const QUrl &url)
{
    QList <ClipController*> list = pCore->binController()->getControllerList();
    QList<QUrl> cacheUrls;
    for (int i = 0; i < list.count(); ++i) {
        ClipController *clip = list.at(i);
        if (clip->clipType() == Text) {
            // the image for title clip must be moved
            QUrl oldUrl = clip->clipUrl();
            QUrl newUrl = QUrl::fromLocalFile(url.toLocalFile() + QDir::separator() + "titles/" + oldUrl.fileName());
            KIO::Job *job = KIO::copy(oldUrl, newUrl);
            if (job->exec()) clip->setProperty("resource", newUrl.path());
        }
        QString hash = clip->getClipHash();
        QUrl oldVideoThumbUrl = QUrl::fromLocalFile(m_projectFolder.path() + QDir::separator() + "thumbs/" + hash + ".png");
        if (QFile::exists(oldVideoThumbUrl.path())) {
            cacheUrls << oldVideoThumbUrl;
        }
        QUrl oldAudioThumbUrl = QUrl::fromLocalFile(m_projectFolder.path() + QDir::separator() + "thumbs/" + hash + ".thumb");
        if (QFile::exists(oldAudioThumbUrl.path())) {
            cacheUrls << oldAudioThumbUrl;
        }
        QUrl oldVideoProxyUrl = QUrl::fromLocalFile(m_projectFolder.path() + QDir::separator() + "proxy/" + hash + '.' + KdenliveSettings::proxyextension());
        if (QFile::exists(oldVideoProxyUrl.path())) {
            cacheUrls << oldVideoProxyUrl;
        }
    }
    if (!cacheUrls.isEmpty()) {
        KIO::Job *job = KIO::copy(cacheUrls, QUrl::fromLocalFile(url.path() + QDir::separator() + "thumbs/"));
        KJobWidgets::setWindow(job, QApplication::activeWindow());
        job->exec();
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

ProfileInfo KdenliveDoc::getProfileInfo() const
{
        ProfileInfo info;
        info.profileSize = getRenderSize();
        info.profileFps = fps();
        return info;
}

double KdenliveDoc::dar() const
{
    return (double) m_profile.display_aspect_num / m_profile.display_aspect_den;
}

void KdenliveDoc::setThumbsProgress(const QString &message, int progress)
{
    emit progressInfo(message, progress);
}

QUndoStack *KdenliveDoc::commandStack()
{
    return m_commandStack;
}

Render *KdenliveDoc::renderer()
{
    return m_render;
}

void KdenliveDoc::updateClip(const QString &id)
{
    emit updateClipDisplay(id);
}

int KdenliveDoc::getFramePos(const QString &duration)
{
    return m_timecode.getFrameCount(duration);
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
        return GenTime(m_render->getLength(), m_render->fps()).ms() / 1000;
    else
        return 0;
}

double KdenliveDoc::fps() const
{
    return m_render->fps();
}

int KdenliveDoc::width() const
{
    return m_width;
}

int KdenliveDoc::height() const
{
    return m_height;
}

QUrl KdenliveDoc::url() const
{
    return m_url;
}

void KdenliveDoc::setUrl(const QUrl &url)
{
    m_url = url;
}

void KdenliveDoc::slotModified()
{
    setModified(m_commandStack->isClean() == false);
}

void KdenliveDoc::setModified(bool mod)
{
    // fix mantis#3160: The document may have an empty URL if not saved yet, but should have a m_autosave in any case
    if (m_autosave && mod && KdenliveSettings::crashrecovery()) {
        emit startAutoSave();
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
    if (!m_url.isValid())
        return i18n("Untitled") + "[*] / " + m_profile.description;
    else
        return m_url.fileName() + " [*]/ " + m_profile.description;
}

bool KdenliveDoc::addClip(QDomElement elem, const QString &clipId)
{
    const QString producerId = clipId.section('_', 0, 0);
    elem.setAttribute("id", producerId);
    pCore->bin()->createClip(elem);
    m_render->getFileProperties(elem, producerId, 150, true);

    /*QString str;
    QTextStream stream(&str);
    elem.save(stream, 4);
    qDebug()<<"ADDING CLIP COMMAND\n-----------\n"<<str;*/
    return true;
    /*DocClipBase *clip = m_clipManager->getClipById(producerId);

    if (clip == NULL) {
        QString clipFolder = KRecentDirs::dir(":KdenliveClipFolder");
        elem.setAttribute("id", producerId);
        QString path = elem.attribute("resource");
        QString extension;
        if (elem.attribute("type").toInt() == SlideShow) {
            QUrl f = QUrl::fromLocalFile(path);
            extension = f.fileName();
            path = f.adjusted(QUrl::RemoveFilename).path();
        }
        if (elem.hasAttribute("_missingsource")) {
            // Clip has proxy but missing original source
        }
        else if (path.isEmpty() == false && QFile::exists(path) == false && elem.attribute("type").toInt() != Text && !elem.hasAttribute("placeholder")) {
            //qDebug() << "// FOUND MISSING CLIP: " << path << ", TYPE: " << elem.attribute("type").toInt();
            const QString size = elem.attribute("file_size");
            const QString hash = elem.attribute("file_hash");
            QString newpath;
            int action = KMessageBox::No;
            if (!size.isEmpty() && !hash.isEmpty()) {
                if (!m_searchFolder.isEmpty())
                    newpath = searchFileRecursively(m_searchFolder, size, hash);
                else
                    action = (KMessageBox::ButtonCode) KMessageBox::questionYesNoCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br />is invalid, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search automatically")), KGuiItem(i18n("Keep as placeholder")));
            } else {
                if (elem.attribute("type").toInt() == SlideShow) {
                    int res = KMessageBox::questionYesNoCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br />is invalid or missing, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search manually")), KGuiItem(i18n("Keep as placeholder")));
                    if (res == KMessageBox::Yes)
                        newpath = QFileDialog::getExistingDirectory(QApplication::activeWindow(), i18n("Looking for %1", path), clipFolder);
                    else {
                        // Abort project loading
                        action = res;
                    }
                } else {
                    int res = KMessageBox::questionYesNoCancel(QApplication::activeWindow(), i18n("Clip <b>%1</b><br />is invalid or missing, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search manually")), KGuiItem(i18n("Keep as placeholder")));
                    if (res == KMessageBox::Yes)
                        newpath = QFileDialog::getOpenFileName(QApplication::activeWindow(), i18n("Looking for %1", path), clipFolder);
                    else {
                        // Abort project loading
                        action = res;
                    }
                }
            }
            if (action == KMessageBox::Yes) {
                //qDebug() << "// ASKED FOR SRCH CLIP: " << clipId;
                m_searchFolder = QFileDialog::getExistingDirectory(QApplication::activeWindow(), QString(), clipFolder);
                if (!m_searchFolder.isEmpty())
                    newpath = searchFileRecursively(QDir(m_searchFolder), size, hash);
            } else if (action == KMessageBox::Cancel) {
                return false;
            } else if (action == KMessageBox::No) {
                // Keep clip as placeHolder
                elem.setAttribute("placeholder", '1');
            }
            if (!newpath.isEmpty()) {
                //qDebug() << "// NEW CLIP PATH FOR CLIP " << clipId << " : " << newpath;
                if (elem.attribute("type").toInt() == SlideShow)
                    newpath.append('/' + extension);
                elem.setAttribute("resource", newpath);
                setNewClipResource(clipId, newpath);
                setModified(true);
            }
        }
        clip = new DocClipBase(m_clipManager, elem, producerId);
        m_clipManager->addClip(clip);
    }
    return true; */
}

void KdenliveDoc::setNewClipResource(const QString &id, const QString &path)
{
    QDomNodeList prods = m_document.elementsByTagName("producer");
    int maxprod = prods.count();
    for (int i = 0; i < maxprod; ++i) {
        QDomNode m = prods.at(i);
        QString prodId = m.toElement().attribute("id");
        if (prodId == id || prodId.startsWith(id + '_')) {
            QDomNodeList params = m.childNodes();
            for (int j = 0; j < params.count(); ++j) {
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
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
        QFile file(dir.absoluteFilePath(filesAndDirs.at(i)));
        if (file.open(QIODevice::ReadOnly)) {
            if (QString::number(file.size()) == matchSize) {
                /*
                * 1 MB = 1 second per 450 files (or faster)
                * 10 MB = 9 seconds per 450 files (or faster)
                */
                if (file.size() > 1000000 * 2) {
                    fileData = file.read(1000000);
                    if (file.seek(file.size() - 1000000))
                        fileData.append(file.readAll());
                } else
                    fileData = file.readAll();
                file.close();
                fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
                if (QString(fileHash.toHex()) == matchHash)
                    return file.fileName();
                else
                    qDebug() << filesAndDirs.at(i) << "size match but not hash";
            }
        }
        ////qDebug() << filesAndDirs.at(i) << file.size() << fileHash.toHex();
    }
    filesAndDirs = dir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
        foundFileName = searchFileRecursively(dir.absoluteFilePath(filesAndDirs.at(i)), matchSize, matchHash);
        if (!foundFileName.isEmpty())
            break;
    }
    return foundFileName;
}

/*
bool KdenliveDoc::addClipInfo(QDomElement elem, QDomElement orig, const QString &clipId)
{
    DocClipBase *clip = m_clipManager->getClipById(clipId);
    if (clip == NULL) {
        if (!addClip(elem, clipId, false))
            return false;
    } else {
        QMap <QString, QString> properties;
        QDomNamedNodeMap attributes = elem.attributes();
        for (int i = 0; i < attributes.count(); ++i) {
            QString attrname = attributes.item(i).nodeName();
            if (attrname != "resource")
                properties.insert(attrname, attributes.item(i).nodeValue());
            ////qDebug() << attrname << " = " << attributes.item(i).nodeValue();
        }
        clip->setProperties(properties);
        emit addProjectClip(clip, false);
    }
    if (!orig.isNull()) {
        QMap<QString, QString> meta;
        for (QDomNode m = orig.firstChild(); !m.isNull(); m = m.nextSibling()) {
            QString name = m.toElement().attribute("name");
            if (name.startsWith(QLatin1String("meta.attr"))) {
                if (name.endsWith(QLatin1String(".markup"))) name = name.section('.', 0, -2);
                meta.insert(name.section('.', 2, -1), m.firstChild().nodeValue());
            }
        }
        if (!meta.isEmpty()) {
            if (clip == NULL)
                clip = m_clipManager->getClipById(clipId);
            if (clip)
                clip->setMetadata(meta);
        }
    }
    return true;
}*/


void KdenliveDoc::deleteClip(const QString &clipId)
{
    ClipController *controller = pCore->binController()->getController(clipId);
    ClipType type = controller->clipType();
    QString url = controller->clipUrl().toLocalFile();
    if (type != Color && type != SlideShow  && !url.isEmpty()) {
        m_fileWatcher.removeFile(url);
    }
    // Delete clip in bin
    pCore->bin()->deleteClip(clipId);
    // Delete controller and Mlt::Producer
    pCore->binController()->removeBinClip(clipId);
}

ProjectClip *KdenliveDoc::getBinClip(const QString &clipId)
{
    return pCore->bin()->getBinClip(clipId);
}

QStringList KdenliveDoc::getBinFolderClipIds(const QString &folderId) const
{
    return pCore->bin()->getBinFolderClipIds(folderId);
}

ClipController *KdenliveDoc::getClipController(const QString &clipId)
{
    return pCore->binController()->getController(clipId);
}


void KdenliveDoc::slotCreateTextTemplateClip(const QString &group, const QString &groupId, QUrl path)
{
    QString titlesFolder = QDir::cleanPath(projectFolder().path() + QDir::separator() + "titles/");
    if (path.isEmpty()) {
        QPointer<QFileDialog> d = new QFileDialog(QApplication::activeWindow(),  i18n("Enter Template Path"), titlesFolder);
        d->setMimeTypeFilters(QStringList() << "application/x-kdenlivetitle");
        d->setFileMode(QFileDialog::ExistingFile);
        if (d->exec() == QDialog::Accepted && !d->selectedUrls().isEmpty()) {
            path = d->selectedUrls().first();
        }
        delete d;
    }

    if (path.isEmpty()) return;

    //TODO: rewrite with new title system (just set resource)
    m_clipManager->slotAddTextTemplateClip(i18n("Template title clip"), path, group, groupId);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
}

void KdenliveDoc::cacheImage(const QString &fileId, const QImage &img) const
{
    img.save(QDir::cleanPath(m_projectFolder.path() +QDir::separator() + "thumbs/" + fileId + ".png"));
}

bool KdenliveDoc::checkDocumentClips()
{
    DocumentChecker d(m_document);
    return (d.hasErrorInClips() == false);

    /*    int clipType;
        QDomElement e;
        QString id;
        QString resource;
        QList <QDomElement> missingClips;
        for (int i = 0; i < infoproducers.count(); ++i) {
            e = infoproducers.item(i).toElement();
            clipType = e.attribute("type").toInt();
            if (clipType == COLOR) continue;
            if (clipType == TEXT) {
                //TODO: Check is clip template is missing (xmltemplate) or hash changed
                continue;
            }
            id = e.attribute("id");
            resource = e.attribute("resource");
            if (clipType == SLIDESHOW) resource = QUrl(resource).directory();
            if (!KIO::NetAccess::exists(QUrl(resource), KIO::NetAccess::SourceSide, 0)) {
                // Missing clip found
                missingClips.append(e);
            } else {
                // Check if the clip has changed
                if (clipType != SLIDESHOW && e.hasAttribute("file_hash")) {
                    if (e.attribute("file_hash") != DocClipBase::getHash(e.attribute("resource")))
                        e.removeAttribute("file_hash");
                }
            }
        }
        if (missingClips.isEmpty()) return true;
        DocumentChecker d(missingClips, m_document);
        return (d.exec() == QDialog::Accepted);*/
}

void KdenliveDoc::setDocumentProperty(const QString &name, const QString &value)
{
    m_documentProperties[name] = value;
}

const QString KdenliveDoc::getDocumentProperty(const QString &name) const
{
    return m_documentProperties.value(name);
}

QMap <QString, QString> KdenliveDoc::getRenderProperties() const
{
    QMap <QString, QString> renderProperties;
    QMapIterator<QString, QString> i(m_documentProperties);
    while (i.hasNext()) {
        i.next();
        if (i.key().startsWith(QLatin1String("render"))) renderProperties.insert(i.key(), i.value());
    }
    return renderProperties;
}



void KdenliveDoc::saveCustomEffects(const QDomNodeList &customeffects)
{
    QDomElement e;
    QStringList importedEffects;
    int maxchild = customeffects.count();
    for (int i = 0; i < maxchild; ++i) {
        e = customeffects.at(i).toElement();
        const QString id = e.attribute("id");
        const QString tag = e.attribute("tag");
        if (!id.isEmpty()) {
            // Check if effect exists or save it
            if (MainWindow::customEffects.hasEffect(tag, id) == -1) {
                QDomDocument doc;
                doc.appendChild(doc.importNode(e, true));
                QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/effects";
                path += id + ".xml";
                if (!QFile::exists(path)) {
                    importedEffects << id;
                    QFile file(path);
                    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
                        QTextStream out(&file);
                        out << doc.toString();
                    }
                }
            }
        }
    }
    if (!importedEffects.isEmpty())
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following effects were imported from the project:"), importedEffects);
    if (!importedEffects.isEmpty()) {
        emit reloadEffects();
    }
}

void KdenliveDoc::updateProjectFolderPlacesEntry()
{
    /*
     * For similar and more code have a look at kfileplacesmodel.cpp and the included files:
     * http://websvn.kde.org/trunk/KDE/kdelibs/kfile/kfileplacesmodel.cpp?view=markup
     */

    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/user-places.xbel";
    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForExternalFile(file);
    if (!bookmarkManager) return;
    KBookmarkGroup root = bookmarkManager->root();
    
    KBookmark bookmark = root.first();

    QString kdenliveName = QCoreApplication::applicationName();
    QUrl documentLocation = m_projectFolder;

    bool exists = false;

    while (!bookmark.isNull()) {
        // UDI not empty indicates a device
        QString udi = bookmark.metaDataItem("UDI");
        QString appName = bookmark.metaDataItem("OnlyInApp");

        if (udi.isEmpty() && appName == kdenliveName && bookmark.text() == i18n("Project Folder")) {
            if (bookmark.url() != documentLocation) {
                bookmark.setUrl(documentLocation);
                bookmarkManager->emitChanged(root);
            }
            exists = true;
            break;
        }

        bookmark = root.next(bookmark);
    }

    // if entry does not exist yet (was not found), well, create it then
    if (!exists) {
        bookmark = root.addBookmark(i18n("Project Folder"), documentLocation, "folder-favorites");
        // Make this user selectable ?
        bookmark.setMetaDataItem("OnlyInApp", kdenliveName);
        bookmarkManager->emitChanged(root);
    }
}

QStringList KdenliveDoc::getExpandedFolders()
{
    QStringList result = m_documentProperties.value("expandedfolders").split(';');
    // this property is only needed once when opening project, so clear it now
    m_documentProperties.remove("expandedfolders");
    return result;
}

const QSize KdenliveDoc::getRenderSize() const
{
    QSize size;
    if (m_render) {
	size.setWidth(m_render->frameRenderWidth());
	size.setHeight(m_render->renderHeight());
    }
    return size;
}
// static
double KdenliveDoc::getDisplayRatio(const QString &path)
{
    QFile file(path);
    QDomDocument doc;
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ERROR, CANNOT READ: " << path;
        return 0;
    }
    if (!doc.setContent(&file)) {
        qWarning() << "ERROR, CANNOT READ: " << path;
        file.close();
        return 0;
    }
    file.close();
    QDomNodeList list = doc.elementsByTagName("profile");
    if (list.isEmpty()) return 0;
    QDomElement profile = list.at(0).toElement();
    double den = profile.attribute("display_aspect_den").toDouble();
    if (den > 0) return profile.attribute("display_aspect_num").toDouble() / den;
    return 0;
}

void KdenliveDoc::backupLastSavedVersion(const QString &path)
{
    // Ensure backup folder exists
    if (path.isEmpty()) return;
    QFile file(path);
    QDir backupFolder(m_projectFolder.path() + "/.backup");

    QString fileName = QUrl::fromLocalFile(path).fileName().section('.', 0, -2);
    QFileInfo info(file);
    fileName.append('-' + m_documentProperties.value("documentid"));
    fileName.append(info.lastModified().toString("-yyyy-MM-dd-hh-mm"));
    fileName.append(".kdenlive");
    QString backupFile = backupFolder.absoluteFilePath(fileName);
    if (file.exists()) {
        // delete previous backup if it was done less than 60 seconds ago
        QFile::remove(backupFile);
        if (!QFile::copy(path, backupFile)) {
            KMessageBox::information(QApplication::activeWindow(), i18n("Cannot create backup copy:\n%1", backupFile));
        }
    }
}

void KdenliveDoc::cleanupBackupFiles()
{
    QDir backupFolder(m_projectFolder.path() + "/.backup");
    QString projectFile = url().fileName().section('.', 0, -2);
    projectFile.append('-' + m_documentProperties.value("documentid"));
    projectFile.append("-??");
    projectFile.append("??");
    projectFile.append("-??");
    projectFile.append("-??");
    projectFile.append("-??");
    projectFile.append("-??.kdenlive");

    QStringList filter;
    filter << projectFile;
    backupFolder.setNameFilters(filter);
    QFileInfoList resultList = backupFolder.entryInfoList(QDir::Files, QDir::Time);

    QDateTime d = QDateTime::currentDateTime();
    QStringList hourList;
    QStringList dayList;
    QStringList weekList;
    QStringList oldList;
    for (int i = 0; i < resultList.count(); ++i) {
        if (d.secsTo(resultList.at(i).lastModified()) < 3600) {
            // files created in the last hour
            hourList.append(resultList.at(i).absoluteFilePath());
        }
        else if (d.secsTo(resultList.at(i).lastModified()) < 43200) {
            // files created in the day
            dayList.append(resultList.at(i).absoluteFilePath());
        }
        else if (d.daysTo(resultList.at(i).lastModified()) < 8) {
            // files created in the week
            weekList.append(resultList.at(i).absoluteFilePath());
        }
        else {
            // older files
            oldList.append(resultList.at(i).absoluteFilePath());
        }
    }
    if (hourList.count() > 20) {
        int step = hourList.count() / 10;
        for (int i = 0; i < hourList.count(); i += step) {
            //qDebug()<<"REMOVE AT: "<<i<<", COUNT: "<<hourList.count();
            hourList.removeAt(i);
            --i;
        }
    } else hourList.clear();
    if (dayList.count() > 20) {
        int step = dayList.count() / 10;
        for (int i = 0; i < dayList.count(); i += step) {
            dayList.removeAt(i);
            --i;
        }
    } else dayList.clear();
    if (weekList.count() > 20) {
        int step = weekList.count() / 10;
        for (int i = 0; i < weekList.count(); i += step) {
            weekList.removeAt(i);
            --i;
        }
    } else weekList.clear();
    if (oldList.count() > 20) {
        int step = oldList.count() / 10;
        for (int i = 0; i < oldList.count(); i += step) {
            oldList.removeAt(i);
            --i;
        }
    } else oldList.clear();
    
    QString f;
    while (hourList.count() > 0) {
        f = hourList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + ".png");
    }
    while (dayList.count() > 0) {
        f = dayList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + ".png");
    }
    while (weekList.count() > 0) {
        f = weekList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + ".png");
    }
    while (oldList.count() > 0) {
        f = oldList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + ".png");
    }
}

const QMap <QString, QString> KdenliveDoc::metadata() const
{
    return m_documentMetadata;
}

void KdenliveDoc::setMetadata(const QMap<QString, QString> &meta)
{
    setModified(true);
    m_documentMetadata = meta;
}

void KdenliveDoc::slotProxyCurrentItem(bool doProxy)
{
    QList<ProjectClip *> clipList = pCore->bin()->selectedClips();
    QUndoCommand *command = new QUndoCommand();
    if (doProxy) command->setText(i18np("Add proxy clip", "Add proxy clips", clipList.count()));
    else command->setText(i18np("Remove proxy clip", "Remove proxy clips", clipList.count()));

    // Make sure the proxy folder exists
    QString proxydir = projectFolder().path() + QDir::separator() + "proxy/";
    QDir dir(projectFolder().path());
    dir.mkdir("proxy");

    // Prepare updated properties
    QMap <QString, QString> newProps;
    QMap <QString, QString> oldProps;
    if (!doProxy) newProps.insert("kdenlive:proxy", "-");

    // Parse clips
    for (int i = 0; i < clipList.count(); ++i) {
        ProjectClip *item = clipList.at(i);
        ClipType t = item->clipType();
        
        // Only allow proxy on some clip types
        if ((t == Video || t == AV || t == Unknown || t == Image || t == Playlist) && item->isReady()) {
	    if ((doProxy && item->hasProxy()) || (!doProxy && !item->hasProxy() && pCore->binController()->hasClip(item->clipId()))) continue;
            if (m_render->isProcessing(item->clipId())) {
                continue;
            }

            if (doProxy) {
                newProps.clear();
                QString path = proxydir + item->hash() + '.' + (t == Image ? "png" : getDocumentProperty("proxyextension"));
                // insert required duration for proxy
                newProps.insert("proxy_out", item->getProducerProperty("out"));
                newProps.insert("kdenlive:proxy", path);
            }
            else if (!pCore->binController()->hasClip(item->clipId())) {
                // Force clip reload
                newProps.insert("resource", item->url().toLocalFile());
            }
            // We need to insert empty proxy so that undo will work
            //TODO: how to handle clip properties
            //oldProps = clip->currentProperties(newProps);
            if (doProxy) oldProps.insert("kdenlive:proxy", "-");
            new EditClipCommand(pCore->bin(), item->clipId(), oldProps, newProps, true, command);
        }
    }
    if (command->childCount() > 0) {
        m_commandStack->push(command);
    }
    else delete command;
}


//TODO put all file watching stuff in own class

void KdenliveDoc::watchFile(const QUrl &url)
{
    m_fileWatcher.addFile(url.toLocalFile());
}

void KdenliveDoc::slotClipModified(const QString &path)
{
    qDebug() << "// CLIP: " << path << " WAS MODIFIED";
    QStringList ids = pCore->binController()->getBinIdsByResource(QUrl::fromLocalFile(path));
    foreach (const QString &id, ids) {
        if (!m_modifiedClips.contains(id)) {
            pCore->bin()->setWaitingStatus(id);
        }
        m_modifiedClips[id] = QTime::currentTime();
    }
    if (!m_modifiedTimer.isActive()) m_modifiedTimer.start();
}


void KdenliveDoc::slotClipMissing(const QString &path)
{
    qDebug() << "// CLIP: " << path << " WAS MISSING";
    QStringList ids = pCore->binController()->getBinIdsByResource(QUrl::fromLocalFile(path));
    //TODO handle missing clips by replacing producer with an invalid producer
    /*foreach (const QString &id, ids) {    
        emit missingClip(id);
    }*/
}

void KdenliveDoc::slotProcessModifiedClips()
{
    if (!m_modifiedClips.isEmpty()) {
        QMapIterator<QString, QTime> i(m_modifiedClips);
        while (i.hasNext()) {
            i.next();
            if (QTime::currentTime().msecsTo(i.value()) <= -1500) {
                pCore->bin()->reloadClip(i.key());
                m_modifiedClips.remove(i.key());
                break;
            }
        }
    }
    if (m_modifiedClips.isEmpty()) m_modifiedTimer.stop();
}

const QMap <QString, QString> KdenliveDoc::documentProperties()
{
    m_documentProperties.insert("version", QString::number(DOCUMENTVERSION));
    m_documentProperties.insert("kdenliveversion", QString(KDENLIVE_VERSION));
    m_documentProperties.insert("projectfolder", m_projectFolder.path());
    m_documentProperties.insert("profile", profilePath());
    m_documentProperties.insert("position", QString::number(m_render->seekPosition().frames(m_render->fps())));
    return m_documentProperties;
}

void KdenliveDoc::loadDocumentProperties()
{
    QDomNodeList list = m_document.elementsByTagName("playlist");
    if (!list.isEmpty()) {
        QDomElement pl = list.at(0).toElement();
        if (pl.isNull()) return;
        QDomNodeList props = pl.elementsByTagName("property");
        QString name;
        QDomElement e;
        for (int i = 0; i < props.count(); i++) {
            e = props.at(i).toElement();
            name = e.attribute("name");
            if (name.startsWith("kdenlive:docproperties.")) {
                name = name.section(".", 1);
                m_documentProperties.insert(name, e.firstChild().nodeValue());
            }
        }
    }
    QString path = m_documentProperties.value("projectfolder");
    if (!path.startsWith('/')) {
	QDir dir = QDir::home();
	path = dir.absoluteFilePath(path);
    }
    m_projectFolder = QUrl::fromLocalFile(path);
    list = m_document.elementsByTagName("profile");
    if (!list.isEmpty()) {
        m_profile = ProfilesDialog::getVideoProfileFromXml(list.at(0).toElement());
    }
    updateProjectProfile();
}

void KdenliveDoc::updateProjectProfile()
{
    KdenliveSettings::setProject_display_ratio((double) m_profile.display_aspect_num / m_profile.display_aspect_den);
    double fps = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
    KdenliveSettings::setProject_fps(fps);
    m_width = m_profile.width;
    m_height = m_profile.height;
    m_timecode.setFormat(fps);
    KdenliveSettings::setCurrent_profile(m_profile.path);
    pCore->monitorManager()->resetProfiles(m_profile, m_timecode);
}

void KdenliveDoc::resetProfile()
{
    m_profile = ProfilesDialog::getVideoProfile(KdenliveSettings::current_profile());
    updateProjectProfile();
}
