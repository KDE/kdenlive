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
#include "documentvalidator.h"
#include "config-kdenlive.h"
#include "initeffects.h"

#include <KDebug>
#include <KStandardDirs>
#include <KMessageBox>
#include <KProgressDialog>
#include <KLocale>
#include <KFileDialog>
#include <KIO/NetAccess>
#include <KIO/CopyJob>
#include <KIO/JobUiDelegate>
#include <KApplication>
#include <KGlobal>
#include <KBookmarkManager>
#include <KBookmark>
#include <KStandardDirs>

#include <QCryptographicHash>
#include <QFile>
#include <QInputDialog>
#include <QDomImplementation>

#include <mlt++/Mlt.h>

#include "locale.h"


const double DOCUMENTVERSION = 0.88;

KdenliveDoc::KdenliveDoc(const KUrl &url, const KUrl &projectFolder, QUndoGroup *undoGroup, QString profileName, QMap <QString, QString> properties, QMap <QString, QString> metadata, const QPoint &tracks, Render *render, KTextEdit *notes, bool *openBackup, MainWindow *parent, KProgressDialog *progressDialog) :
    QObject(parent),
    m_autosave(NULL),
    m_url(url),
    m_render(render),
    m_notesWidget(notes),
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
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_clipManager, SIGNAL(displayMessage(QString, int)), parent, SLOT(slotGotProgressInfo(QString,int)));
    bool success = false;

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
#if QT_VERSION >= 0x040700
    m_documentProperties["documentid"] = QString::number(QDateTime::currentMSecsSinceEpoch());
#else
    QDateTime date = QDateTime::currentDateTime();
    m_documentProperties["documentid"] = QString::number(date.toTime_t());
#endif

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
	initEffects::parseEffectFiles(setlocale(LC_NUMERIC, NULL));
    }

    *openBackup = false;
    
    if (!url.isEmpty()) {
        QString tmpFile;
        success = KIO::NetAccess::download(url.path(), tmpFile, parent);
        if (!success) {
            // The file cannot be opened
            if (KMessageBox::warningContinueCancel(parent, i18n("Cannot open the project file, error is:\n%1\nDo you want to open a backup file?", KIO::NetAccess::lastErrorString()), i18n("Error opening file"), KGuiItem(i18n("Open Backup"))) == KMessageBox::Continue) {
                *openBackup = true;
            }
            //KMessageBox::error(parent, KIO::NetAccess::lastErrorString());
        }
        else {
            QFile file(tmpFile);
            QString errorMsg;
            QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidChars);
            success = m_document.setContent(&file, false, &errorMsg);
            file.close();
            KIO::NetAccess::removeTempFile(tmpFile);

            if (!success) {
                // It is corrupted
                if (KMessageBox::warningContinueCancel(parent, i18n("Cannot open the project file, error is:\n%1\nDo you want to open a backup file?", errorMsg), i18n("Error opening file"), KGuiItem(i18n("Open Backup"))) == KMessageBox::Continue) {
                *openBackup = true;
            }
                //KMessageBox::error(parent, errorMsg);
            }
            else {
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
                    if (success) { // Let the validator handle error messages
                        parent->slotGotProgressInfo(i18n("Check missing clips"), 0);
                        qApp->processEvents();
                        QDomNodeList infoproducers = m_document.elementsByTagName("kdenlive_producer");
                        success = checkDocumentClips(infoproducers);
                        if (success) {
                            if (m_document.documentElement().attribute("modified") == "1") setModified(true);
                            parent->slotGotProgressInfo(i18n("Loading"), 0);
                            QDomElement mlt = m_document.firstChildElement("mlt");
                            QDomElement infoXml = mlt.firstChildElement("kdenlivedoc");

                            // Set profile, fps, etc for the document
                            setProfilePath(infoXml.attribute("profile"));

                            // Check embedded effects
                            QDomElement customeffects = infoXml.firstChildElement("customeffects");
                            if (!customeffects.isNull() && customeffects.hasChildNodes()) {
                                parent->slotGotProgressInfo(i18n("Importing project effects"), 0);
                                qApp->processEvents();
                                if (saveCustomEffects(customeffects.childNodes())) parent->slotReloadEffects();
                            }

                            QDomElement e;
                            // Read notes
                            QDomElement notesxml = infoXml.firstChildElement("documentnotes");
                            if (!notesxml.isNull()) m_notesWidget->setText(notesxml.firstChild().nodeValue());

                            // Build tracks
                            QDomElement tracksinfo = infoXml.firstChildElement("tracksinfo");
                            if (!tracksinfo.isNull()) {
                                QDomNodeList trackslist = tracksinfo.childNodes();
                                int maxchild = trackslist.count();
                                for (int k = 0; k < maxchild; k++) {
                                    e = trackslist.at(k).toElement();
                                    if (e.tagName() == "trackinfo") {
                                        TrackInfo projectTrack;
                                        if (e.attribute("type") == "audio")
                                            projectTrack.type = AUDIOTRACK;
                                        else
                                            projectTrack.type = VIDEOTRACK;
                                        projectTrack.isMute = e.attribute("mute").toInt();
                                        projectTrack.isBlind = e.attribute("blind").toInt();
                                        projectTrack.isLocked = e.attribute("locked").toInt();
                                        projectTrack.trackName = e.attribute("trackname");
					projectTrack.effectsList = EffectsList(true);
                                        m_tracksList.append(projectTrack);
                                    }
                                }
                                mlt.removeChild(tracksinfo);
                            }
                            QStringList expandedFolders;
                            QDomNodeList folders = m_document.elementsByTagName("folder");
                            for (int i = 0; i < folders.count(); i++) {
                                e = folders.item(i).cloneNode().toElement();
                                if (e.hasAttribute("opened")) expandedFolders.append(e.attribute("id"));
                                m_clipManager->addFolder(e.attribute("id"), e.attribute("name"));
                            }
                            m_documentProperties["expandedfolders"] = expandedFolders.join(";");

                            const int infomax = infoproducers.count();
                            QDomNodeList producers = m_document.elementsByTagName("producer");
                            const int max = producers.count();

                            if (!progressDialog) {
                                progressDialog = new KProgressDialog(parent, i18n("Loading project"), i18n("Adding clips"));
                                progressDialog->setAllowCancel(false);
                            } else {
                                progressDialog->setLabelText(i18n("Adding clips"));
                            }
                            progressDialog->progressBar()->setMaximum(infomax);
                            progressDialog->show();
                            qApp->processEvents();

                            for (int i = 0; i < infomax; i++) {
                                e = infoproducers.item(i).cloneNode().toElement();
                                QString prodId = e.attribute("id");
                                if (!e.isNull() && prodId != "black" && !prodId.startsWith("slowmotion")) {
                                    e.setTagName("producer");
                                    // Get MLT's original producer properties
                                    QDomElement orig;
                                    for (int j = 0; j < max; j++) {
                                        QDomNode o = producers.item(j);
                                        QString origId = o.attributes().namedItem("id").nodeValue().section('_', 0, 0);
                                        if (origId == prodId) {
                                            orig = o.cloneNode().toElement();
                                            break;
                                        }
                                    }

                                    if (!addClipInfo(e, orig, prodId)) {
                                        // The user manually aborted the loading.
                                        success = false;
                                        emit resetProjectList();
                                        m_tracksList.clear();
                                        m_clipManager->clear();
                                        break;
                                    }
                                }
                                if (i % 10 == 0)
                                    progressDialog->progressBar()->setValue(i);
                            }

                            if (success) {
                                QDomElement markers = infoXml.firstChildElement("markers");
                                if (!markers.isNull()) {
                                    QDomNodeList markerslist = markers.childNodes();
                                    int maxchild = markerslist.count();
                                    for (int k = 0; k < maxchild; k++) {
                                        e = markerslist.at(k).toElement();
                                        if (e.tagName() == "marker") {
					    CommentedTime marker(GenTime(e.attribute("time").toDouble()), e.attribute("comment"), e.attribute("type").toInt());
                                            m_clipManager->getClipById(e.attribute("id"))->addSnapMarker(marker);
					}
                                    }
                                    infoXml.removeChild(markers);
                                }

                                m_projectFolder = KUrl(infoXml.attribute("projectfolder"));
                                QDomElement docproperties = infoXml.firstChildElement("documentproperties");
                                QDomNamedNodeMap props = docproperties.attributes();
                                for (int i = 0; i < props.count(); i++)
                                    m_documentProperties.insert(props.item(i).nodeName(), props.item(i).nodeValue());
                                docproperties = infoXml.firstChildElement("documentmetadata");
                                props = docproperties.attributes();
                                for (int i = 0; i < props.count(); i++)
                                    m_documentMetadata.insert(props.item(i).nodeName(), props.item(i).nodeValue());

                                if (validator.isModified()) setModified(true);
                                kDebug() << "Reading file: " << url.path() << ", found clips: " << producers.count();
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
        setProfilePath(profileName);
        m_document = createEmptyDocument(tracks.x(), tracks.y());
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
    if (m_projectFolder.isEmpty() || !KIO::NetAccess::exists(m_projectFolder.path(), KIO::NetAccess::DestinationSide, parent)) {
        KMessageBox::information(parent, i18n("Document project folder is invalid, setting it to the default one: %1", KdenliveSettings::defaultprojectfolder()));
        m_projectFolder = KUrl(KdenliveSettings::defaultprojectfolder());
    }

    // Make sure that the necessary folders exist
    KStandardDirs::makeDir(m_projectFolder.path(KUrl::AddTrailingSlash) + "titles/");
    KStandardDirs::makeDir(m_projectFolder.path(KUrl::AddTrailingSlash) + "thumbs/");
    KStandardDirs::makeDir(m_projectFolder.path(KUrl::AddTrailingSlash) + "proxy/");

    updateProjectFolderPlacesEntry();

    //kDebug() << "// SETTING SCENE LIST:\n\n" << m_document.toString();
    connect(m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(slotAutoSave()));
    connect(m_render, SIGNAL(addClip(const KUrl &, stringMap)), this, SLOT(slotAddClipFile(const KUrl &, stringMap)));
}

KdenliveDoc::~KdenliveDoc()
{
    m_autoSaveTimer->stop();
    delete m_commandStack;
    kDebug() << "// DEL CLP MAN";
    delete m_clipManager;
    kDebug() << "// DEL CLP MAN done";
    delete m_autoSaveTimer;
    if (m_autosave) {
        if (!m_autosave->fileName().isEmpty()) m_autosave->remove();
        delete m_autosave;
    }
}

int KdenliveDoc::setSceneList()
{
    m_render->resetProfile(KdenliveSettings::current_profile(), true);
    if (m_render->setSceneList(m_document.toString(), m_documentProperties.value("position").toInt()) == -1) {
        // INVALID MLT Consumer, something is wrong
        return -1;
    }
    m_documentProperties.remove("position");
    // m_document xml is now useless, clear it
    m_document.clear();
    return 0;
}

QDomDocument KdenliveDoc::createEmptyDocument(int videotracks, int audiotracks)
{
    m_tracksList.clear();

    // Tracks are added «backwards», so we need to reverse the track numbering
    // mbt 331: http://www.kdenlive.org/mantis/view.php?id=331
    // Better default names for tracks: Audio 1 etc. instead of blank numbers
    for (int i = 0; i < audiotracks; i++) {
        TrackInfo audioTrack;
        audioTrack.type = AUDIOTRACK;
        audioTrack.isMute = false;
        audioTrack.isBlind = true;
        audioTrack.isLocked = false;
        audioTrack.trackName = QString("Audio ") + QString::number(audiotracks - i);
        audioTrack.duration = 0;
	audioTrack.effectsList = EffectsList(true);
        m_tracksList.append(audioTrack);

    }
    for (int i = 0; i < videotracks; i++) {
        TrackInfo videoTrack;
        videoTrack.type = VIDEOTRACK;
        videoTrack.isMute = false;
        videoTrack.isBlind = false;
        videoTrack.isLocked = false;
        videoTrack.trackName = QString("Video ") + QString::number(videotracks - i);
        videoTrack.duration = 0;
	videoTrack.effectsList = EffectsList(true);
        m_tracksList.append(videoTrack);
    }
    return createEmptyDocument(m_tracksList);
}

QDomDocument KdenliveDoc::createEmptyDocument(QList <TrackInfo> tracks)
{
    // Creating new document
    QDomDocument doc;
    QDomElement mlt = doc.createElement("mlt");
    mlt.setAttribute("LC_NUMERIC", "");
    doc.appendChild(mlt);
    
    // Create black producer
    // For some unknown reason, we have to build the black producer here and not in renderer.cpp, otherwise
    // the composite transitions with the black track are corrupted.
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
    QDomElement multitrack = doc.createElement("multitrack");
    QDomElement playlist = doc.createElement("playlist");
    playlist.setAttribute("id", "black_track");
    mlt.appendChild(playlist);

    QDomElement blank0 = doc.createElement("entry");
    blank0.setAttribute("in", "0");
    blank0.setAttribute("out", "1");
    blank0.setAttribute("producer", "black");
    playlist.appendChild(blank0);

    // create playlists
    int total = tracks.count() + 1;

    for (int i = 1; i < total; i++) {
        QDomElement playlist = doc.createElement("playlist");
        playlist.setAttribute("id", "playlist" + QString::number(i));
        mlt.appendChild(playlist);
    }

    QDomElement track0 = doc.createElement("track");
    track0.setAttribute("producer", "black_track");
    tractor.appendChild(track0);

    // create audio and video tracks
    for (int i = 1; i < total; i++) {
        QDomElement track = doc.createElement("track");
        track.setAttribute("producer", "playlist" + QString::number(i));
        if (tracks.at(i - 1).type == AUDIOTRACK) {
            track.setAttribute("hide", "video");
        } else if (tracks.at(i - 1).isBlind)
            track.setAttribute("hide", "video");
        if (tracks.at(i - 1).isMute)
            track.setAttribute("hide", "audio");
        tractor.appendChild(track);
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
    mlt.appendChild(tractor);
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
    setModified(true);
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
        saveSceneList(m_autosave->fileName(), m_render->sceneList(), QStringList(), true);
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

QDomDocument KdenliveDoc::xmlSceneList(const QString &scene, const QStringList &expandedFolders)
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
        for (int i = 0; i < props.count(); i++) {
            if (props.at(i).toElement().attribute("name") == "meta.volume") {
                props.at(i).firstChild().setNodeValue("1");
                break;
            }
        }
    }

    QDomElement addedXml = sceneList.createElement("kdenlivedoc");
    mlt.appendChild(addedXml);

    // check if project contains custom effects to embed them in project file
    QDomNodeList effects = mlt.elementsByTagName("filter");
    int maxEffects = effects.count();
    kDebug() << "// FOUD " << maxEffects << " EFFECTS+++++++++++++++++++++";
    QMap <QString, QString> effectIds;
    for (int i = 0; i < maxEffects; i++) {
        QDomNode m = effects.at(i);
        QDomNodeList params = m.childNodes();
        QString id;
        QString tag;
        for (int j = 0; j < params.count(); j++) {
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
    QDomDocument customeffects = initEffects::getUsedCustomEffects(effectIds);
    addedXml.appendChild(sceneList.importNode(customeffects.documentElement(), true));

    QDomElement markers = sceneList.createElement("markers");
    addedXml.setAttribute("version", DOCUMENTVERSION);
    addedXml.setAttribute("kdenliveversion", VERSION);
    addedXml.setAttribute("profile", profilePath());
    addedXml.setAttribute("projectfolder", m_projectFolder.path());

    QDomElement docproperties = sceneList.createElement("documentproperties");
    QMapIterator<QString, QString> i(m_documentProperties);
    while (i.hasNext()) {
        i.next();
        docproperties.setAttribute(i.key(), i.value());
    }
    docproperties.setAttribute("position", m_render->seekPosition().frames(m_fps));
    addedXml.appendChild(docproperties);
    
    QDomElement docmetadata = sceneList.createElement("documentmetadata");
    QMapIterator<QString, QString> j(m_documentMetadata);
    while (j.hasNext()) {
        j.next();
        docmetadata.setAttribute(j.key(), j.value());
    }
    addedXml.appendChild(docmetadata);

    QDomElement docnotes = sceneList.createElement("documentnotes");
    QDomText value = sceneList.createTextNode(m_notesWidget->toHtml());
    docnotes.appendChild(value);
    addedXml.appendChild(docnotes);

    // Add profile info
    QDomElement profileinfo = sceneList.createElement("profileinfo");
    profileinfo.setAttribute("description", m_profile.description);
    profileinfo.setAttribute("frame_rate_num", m_profile.frame_rate_num);
    profileinfo.setAttribute("frame_rate_den", m_profile.frame_rate_den);
    profileinfo.setAttribute("width", m_profile.width);
    profileinfo.setAttribute("height", m_profile.height);
    profileinfo.setAttribute("progressive", m_profile.progressive);
    profileinfo.setAttribute("sample_aspect_num", m_profile.sample_aspect_num);
    profileinfo.setAttribute("sample_aspect_den", m_profile.sample_aspect_den);
    profileinfo.setAttribute("display_aspect_num", m_profile.display_aspect_num);
    profileinfo.setAttribute("display_aspect_den", m_profile.display_aspect_den);
    addedXml.appendChild(profileinfo);

    // tracks info
    QDomElement tracksinfo = sceneList.createElement("tracksinfo");
    foreach(const TrackInfo & info, m_tracksList) {
        QDomElement trackinfo = sceneList.createElement("trackinfo");
        if (info.type == AUDIOTRACK) trackinfo.setAttribute("type", "audio");
        trackinfo.setAttribute("mute", info.isMute);
        trackinfo.setAttribute("blind", info.isBlind);
        trackinfo.setAttribute("locked", info.isLocked);
        trackinfo.setAttribute("trackname", info.trackName);
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
        if (expandedFolders.contains(f.key())) folder.setAttribute("opened", "1");
        addedXml.appendChild(folder);
    }

    // Save project clips
    QDomElement e;
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    for (int i = 0; i < list.count(); i++) {
        e = list.at(i)->toXML(true);
        e.setTagName("kdenlive_producer");
        addedXml.appendChild(sceneList.importNode(e, true));
        QList < CommentedTime > marks = list.at(i)->commentedSnapMarkers();
        for (int j = 0; j < marks.count(); j++) {
            QDomElement marker = sceneList.createElement("marker");
            marker.setAttribute("time", marks.at(j).time().ms() / 1000);
            marker.setAttribute("comment", marks.at(j).comment());
            marker.setAttribute("id", e.attribute("id"));
	    marker.setAttribute("type", marks.at(j).markerType());
            markers.appendChild(marker);
        }
    }
    addedXml.appendChild(markers);

    // Add guides
    if (!m_guidesXml.isNull()) addedXml.appendChild(sceneList.importNode(m_guidesXml.documentElement(), true));

    // Add clip groups
    addedXml.appendChild(sceneList.importNode(m_clipManager->groupsXml(), true));

    //wes.appendChild(doc.importNode(kdenliveData, true));
    return sceneList;
}

bool KdenliveDoc::saveSceneList(const QString &path, const QString &scene, const QStringList &expandedFolders, bool autosave)
{
    QDomDocument sceneList = xmlSceneList(scene, expandedFolders);
    if (sceneList.isNull()) {
        //Make sure we don't save if scenelist is corrupted
        KMessageBox::error(kapp->activeWindow(), i18n("Cannot write to file %1, scene list is corrupted.", path));
        return false;
    }
    
    // Backup current version
    if (!autosave) backupLastSavedVersion(path);
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
    if (!autosave) {
        cleanupBackupFiles();
        QFileInfo info(file);
        QString fileName = KUrl(path).fileName().section('.', 0, -2);   
        fileName.append('-' + m_documentProperties.value("documentid"));
        fileName.append(info.lastModified().toString("-yyyy-MM-dd-hh-mm"));
        fileName.append(".kdenlive.png");
        KUrl backupFile = m_projectFolder;
        backupFile.addPath(".backup/");
        backupFile.addPath(fileName);
        emit saveTimelinePreview(backupFile.path());
    }
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
    KStandardDirs::makeDir(url.path(KUrl::AddTrailingSlash) + "titles/");
    KStandardDirs::makeDir(url.path(KUrl::AddTrailingSlash) + "thumbs/");
    if (KMessageBox::questionYesNo(kapp->activeWindow(), i18n("You have changed the project folder. Do you want to copy the cached data from %1 to the new folder %2?", m_projectFolder.path(), url.path())) == KMessageBox::Yes) moveProjectData(url);
    m_projectFolder = url;

    updateProjectFolderPlacesEntry();
}

void KdenliveDoc::moveProjectData(KUrl url)
{
    QList <DocClipBase*> list = m_clipManager->documentClipList();
    KUrl::List cacheUrls;
    for (int i = 0; i < list.count(); i++) {
        DocClipBase *clip = list.at(i);
        if (clip->clipType() == TEXT) {
            // the image for title clip must be moved
            KUrl oldUrl = clip->fileURL();
            KUrl newUrl = KUrl(url.path(KUrl::AddTrailingSlash) + "titles/" + oldUrl.fileName());
            KIO::Job *job = KIO::copy(oldUrl, newUrl);
            if (KIO::NetAccess::synchronousRun(job, 0)) clip->setProperty("resource", newUrl.path());
        }
        QString hash = clip->getClipHash();
        KUrl oldVideoThumbUrl = KUrl(m_projectFolder.path(KUrl::AddTrailingSlash) + "thumbs/" + hash + ".png");
        KUrl oldAudioThumbUrl = KUrl(m_projectFolder.path(KUrl::AddTrailingSlash) + "thumbs/" + hash + ".thumb");
        if (KIO::NetAccess::exists(oldVideoThumbUrl, KIO::NetAccess::SourceSide, 0)) {
            cacheUrls << oldVideoThumbUrl;
        }
        if (KIO::NetAccess::exists(oldAudioThumbUrl, KIO::NetAccess::SourceSide, 0)) {
            cacheUrls << oldAudioThumbUrl;
        }
    }
    if (!cacheUrls.isEmpty()) {
        KIO::Job *job = KIO::copy(cacheUrls, KUrl(url.path(KUrl::AddTrailingSlash) + "thumbs/"));
        job->ui()->setWindow(kapp->activeWindow());
        KIO::NetAccess::synchronousRun(job, 0);
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

bool KdenliveDoc::setProfilePath(QString path)
{
    if (path.isEmpty()) path = KdenliveSettings::default_profile();
    if (path.isEmpty()) path = "dv_pal";
    m_profile = ProfilesDialog::getVideoProfile(path);
    double current_fps = m_fps;
    if (m_profile.path.isEmpty()) {
        // Profile not found, use embedded profile
        QDomElement profileInfo = m_document.elementsByTagName("profileinfo").at(0).toElement();
        if (profileInfo.isNull()) {
            KMessageBox::information(kapp->activeWindow(), i18n("Project profile was not found, using default profile."), i18n("Missing Profile"));
            m_profile = ProfilesDialog::getVideoProfile(KdenliveSettings::default_profile());
        } else {
            m_profile.description = profileInfo.attribute("description");
            m_profile.frame_rate_num = profileInfo.attribute("frame_rate_num").toInt();
            m_profile.frame_rate_den = profileInfo.attribute("frame_rate_den").toInt();
            m_profile.width = profileInfo.attribute("width").toInt();
            m_profile.height = profileInfo.attribute("height").toInt();
            m_profile.progressive = profileInfo.attribute("progressive").toInt();
            m_profile.sample_aspect_num = profileInfo.attribute("sample_aspect_num").toInt();
            m_profile.sample_aspect_den = profileInfo.attribute("sample_aspect_den").toInt();
            m_profile.display_aspect_num = profileInfo.attribute("display_aspect_num").toInt();
            m_profile.display_aspect_den = profileInfo.attribute("display_aspect_den").toInt();
            QString existing = ProfilesDialog::existingProfile(m_profile);
            if (!existing.isEmpty()) {
                m_profile = ProfilesDialog::getVideoProfile(existing);
                KMessageBox::information(kapp->activeWindow(), i18n("Project profile not found, replacing with existing one: %1", m_profile.description), i18n("Missing Profile"));
            } else {
                QString newDesc = m_profile.description;
                bool ok = true;
                while (ok && (newDesc.isEmpty() || ProfilesDialog::existingProfileDescription(newDesc))) {
                    newDesc = QInputDialog::getText(kapp->activeWindow(), i18n("Existing Profile"), i18n("Your project uses an unknown profile.\nIt uses an existing profile name: %1.\nPlease choose a new name to save it", newDesc), QLineEdit::Normal, newDesc, &ok);
                }
                if (ok == false) {
                    // User canceled, use default profile
                    m_profile = ProfilesDialog::getVideoProfile(KdenliveSettings::default_profile());
                } else {
                    if (newDesc != m_profile.description) {
                        // Profile description existed, was replaced by new one
                        m_profile.description = newDesc;
                    } else {
                        KMessageBox::information(kapp->activeWindow(), i18n("Project profile was not found, it will be added to your system now."), i18n("Missing Profile"));
                    }
                    ProfilesDialog::saveProfile(m_profile);
                }
            }
            setModified(true);
        }
    }

    KdenliveSettings::setProject_display_ratio((double) m_profile.display_aspect_num / m_profile.display_aspect_den);
    m_fps = (double) m_profile.frame_rate_num / m_profile.frame_rate_den;
    KdenliveSettings::setProject_fps(m_fps);
    m_width = m_profile.width;
    m_height = m_profile.height;
    kDebug() << "Kdenlive document, init timecode from path: " << path << ",  " << m_fps;
    m_timecode.setFormat(m_fps);
    KdenliveSettings::setCurrent_profile(m_profile.path);
    return (current_fps != m_fps);
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

void KdenliveDoc::checkProjectClips(bool displayRatioChanged, bool fpsChanged)
{
    if (m_render == NULL) return;
    m_clipManager->resetProducersList(m_render->producersList(), displayRatioChanged, fpsChanged);
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
    return m_timecode.getFrameCount(duration);
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

bool KdenliveDoc::addClip(QDomElement elem, QString clipId, bool createClipItem)
{
    const QString producerId = clipId.section('_', 0, 0);
    DocClipBase *clip = m_clipManager->getClipById(producerId);

    if (clip == NULL) {
        elem.setAttribute("id", producerId);
        QString path = elem.attribute("resource");
        QString extension;
        if (elem.attribute("type").toInt() == SLIDESHOW) {
            extension = KUrl(path).fileName();
            path = KUrl(path).directory();
        }
        if (elem.hasAttribute("_missingsource")) {
            // Clip has proxy but missing original source
        }
        else if (path.isEmpty() == false && QFile::exists(path) == false && elem.attribute("type").toInt() != TEXT && !elem.hasAttribute("placeholder")) {
            kDebug() << "// FOUND MISSING CLIP: " << path << ", TYPE: " << elem.attribute("type").toInt();
            const QString size = elem.attribute("file_size");
            const QString hash = elem.attribute("file_hash");
            QString newpath;
            int action = KMessageBox::No;
            if (!size.isEmpty() && !hash.isEmpty()) {
                if (!m_searchFolder.isEmpty())
                    newpath = searchFileRecursively(m_searchFolder, size, hash);
                else
                    action = (KMessageBox::ButtonCode) KMessageBox::questionYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br />is invalid, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search automatically")), KGuiItem(i18n("Keep as placeholder")));
            } else {
                if (elem.attribute("type").toInt() == SLIDESHOW) {
                    int res = KMessageBox::questionYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br />is invalid or missing, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search manually")), KGuiItem(i18n("Keep as placeholder")));
                    if (res == KMessageBox::Yes)
                        newpath = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///clipfolder"), kapp->activeWindow(), i18n("Looking for %1", path));
                    else {
                        // Abort project loading
                        action = res;
                    }
                } else {
                    int res = KMessageBox::questionYesNoCancel(kapp->activeWindow(), i18n("Clip <b>%1</b><br />is invalid or missing, what do you want to do?", path), i18n("File not found"), KGuiItem(i18n("Search manually")), KGuiItem(i18n("Keep as placeholder")));
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
                if (!m_searchFolder.isEmpty())
                    newpath = searchFileRecursively(QDir(m_searchFolder), size, hash);
            } else if (action == KMessageBox::Cancel) {
                return false;
            } else if (action == KMessageBox::No) {
                // Keep clip as placeHolder
                elem.setAttribute("placeholder", '1');
            }
            if (!newpath.isEmpty()) {
                if (elem.attribute("type").toInt() == SLIDESHOW)
                    newpath.append('/' + extension);
                elem.setAttribute("resource", newpath);
                setNewClipResource(clipId, newpath);
                setModified(true);
            }
        }
        clip = new DocClipBase(m_clipManager, elem, producerId);
        m_clipManager->addClip(clip);
    }

    if (createClipItem) {
        emit addProjectClip(clip);
    }

    return true;
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

bool KdenliveDoc::addClipInfo(QDomElement elem, QDomElement orig, QString clipId)
{
    DocClipBase *clip = m_clipManager->getClipById(clipId);
    if (clip == NULL) {
        if (!addClip(elem, clipId, false))
            return false;
    } else {
        QMap <QString, QString> properties;
        QDomNamedNodeMap attributes = elem.attributes();
        for (int i = 0; i < attributes.count(); i++) {
            QString attrname = attributes.item(i).nodeName();
            if (attrname != "resource")
                properties.insert(attrname, attributes.item(i).nodeValue());
            kDebug() << attrname << " = " << attributes.item(i).nodeValue();
        }
        clip->setProperties(properties);
        emit addProjectClip(clip, false);
    }
    if (orig != QDomElement()) {
        QMap<QString, QString> meta;
        for (QDomNode m = orig.firstChild(); !m.isNull(); m = m.nextSibling()) {
            QString name = m.toElement().attribute("name");
            if (name.startsWith("meta.attr"))
                meta.insert(name.section('.', 2, 3), m.firstChild().nodeValue());
        }
        if (!meta.isEmpty()) {
            if (clip == NULL)
                clip = m_clipManager->getClipById(clipId);
            if (clip)
                clip->setMetadata(meta);
        }
    }
    return true;
}


void KdenliveDoc::deleteClip(const QString &clipId)
{
    emit signalDeleteProjectClip(clipId);
}

void KdenliveDoc::slotAddClipList(const KUrl::List urls, stringMap data)
{
    m_clipManager->slotAddClipList(urls, data);
    //emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
    setModified(true);
}


void KdenliveDoc::slotAddClipFile(const KUrl &url, stringMap data)
{
    m_clipManager->slotAddClipFile(url, data);
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

void KdenliveDoc::slotCreateXmlClip(const QString &name, const QDomElement xml, QString group, const QString &groupId)
{
    m_clipManager->slotAddXmlClipFile(name, xml, group, groupId);
    setModified(true);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
}

void KdenliveDoc::slotCreateColorClip(const QString &name, const QString &color, const QString &duration, QString group, const QString &groupId)
{
    m_clipManager->slotAddColorClipFile(name, color, duration, group, groupId);
    setModified(true);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
}

void KdenliveDoc::slotCreateSlideshowClipFile(QMap <QString, QString> properties, QString group, const QString &groupId)
{
    m_clipManager->slotAddSlideshowClipFile(properties, group, groupId);
    setModified(true);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
}

void KdenliveDoc::slotCreateTextClip(QString group, const QString &groupId, const QString &templatePath)
{
    QString titlesFolder = projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
    KStandardDirs::makeDir(titlesFolder);
    QPointer<TitleWidget> dia_ui = new TitleWidget(templatePath, m_timecode, titlesFolder, m_render, kapp->activeWindow());
    if (dia_ui->exec() == QDialog::Accepted) {
        m_clipManager->slotAddTextClipFile(i18n("Title clip"), dia_ui->outPoint(), dia_ui->xml().toString(), group, groupId);
        setModified(true);
        emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
    }
    delete dia_ui;
}

void KdenliveDoc::slotCreateTextTemplateClip(QString group, const QString &groupId, KUrl path)
{
    QString titlesFolder = projectFolder().path(KUrl::AddTrailingSlash) + "titles/";
    if (path.isEmpty()) {
        path = KFileDialog::getOpenUrl(KUrl(titlesFolder), "application/x-kdenlivetitle", kapp->activeWindow(), i18n("Enter Template Path"));
    }

    if (path.isEmpty()) return;

    //TODO: rewrite with new title system (just set resource)
    m_clipManager->slotAddTextTemplateClip(i18n("Template title clip"), path, group, groupId);
    setModified(true);
    emit selectLastAddedClip(QString::number(m_clipManager->lastClipId()));
}

int KdenliveDoc::tracksCount() const
{
    return m_tracksList.count();
}

TrackInfo KdenliveDoc::trackInfoAt(int ix) const
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Track INFO outisde of range";
        return TrackInfo();
    }
    return m_tracksList.at(ix);
}

void KdenliveDoc::switchTrackAudio(int ix, bool hide)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "SWITCH Track outisde of range";
        return;
    }
    m_tracksList[ix].isMute = hide; // !m_tracksList.at(ix).isMute;
}

void KdenliveDoc::switchTrackLock(int ix, bool lock)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Track Lock outisde of range";
        return;
    }
    m_tracksList[ix].isLocked = lock;
}

bool KdenliveDoc::isTrackLocked(int ix) const
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Track Lock outisde of range";
        return true;
    }
    return m_tracksList.at(ix).isLocked;
}

void KdenliveDoc::switchTrackVideo(int ix, bool hide)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "SWITCH Track outisde of range";
        return;
    }
    m_tracksList[ix].isBlind = hide; // !m_tracksList.at(ix).isBlind;
}

int KdenliveDoc::trackDuration(int ix)
{
    return m_tracksList.at(ix).duration; 
}

void KdenliveDoc::setTrackDuration(int ix, int duration)
{
    m_tracksList[ix].duration = duration;
}

void KdenliveDoc::insertTrack(int ix, TrackInfo type)
{
    if (ix == -1) m_tracksList << type;
    else m_tracksList.insert(ix, type);
}

void KdenliveDoc::deleteTrack(int ix)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Delete Track outisde of range";
        return;
    }
    m_tracksList.removeAt(ix);
}

void KdenliveDoc::setTrackType(int ix, TrackInfo type)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "SET Track Type outisde of range";
        return;
    }
    m_tracksList[ix].type = type.type;
    m_tracksList[ix].isMute = type.isMute;
    m_tracksList[ix].isBlind = type.isBlind;
    m_tracksList[ix].isLocked = type.isLocked;
    m_tracksList[ix].trackName = type.trackName;
}

const QList <TrackInfo> KdenliveDoc::tracksList() const
{
    return m_tracksList;
}

QPoint KdenliveDoc::getTracksCount() const
{
    int audio = 0;
    int video = 0;
    foreach(const TrackInfo & info, m_tracksList) {
        if (info.type == VIDEOTRACK) video++;
        else audio++;
    }
    return QPoint(video, audio);
}

void KdenliveDoc::cacheImage(const QString &fileId, const QImage &img) const
{
    img.save(m_projectFolder.path(KUrl::AddTrailingSlash) + "thumbs/" + fileId + ".png");
}

bool KdenliveDoc::checkDocumentClips(QDomNodeList infoproducers)
{
    DocumentChecker d(infoproducers, m_document);
    return (d.hasErrorInClips() == false);

    /*    int clipType;
        QDomElement e;
        QString id;
        QString resource;
        QList <QDomElement> missingClips;
        for (int i = 0; i < infoproducers.count(); i++) {
            e = infoproducers.item(i).toElement();
            clipType = e.attribute("type").toInt();
            if (clipType == COLOR) continue;
            if (clipType == TEXT) {
                //TODO: Check is clip template is missing (xmltemplate) or hash changed
                continue;
            }
            id = e.attribute("id");
            resource = e.attribute("resource");
            if (clipType == SLIDESHOW) resource = KUrl(resource).directory();
            if (!KIO::NetAccess::exists(KUrl(resource), KIO::NetAccess::SourceSide, 0)) {
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
        if (i.key().startsWith("render")) renderProperties.insert(i.key(), i.value());
    }
    return renderProperties;
}

void KdenliveDoc::addTrackEffect(int ix, QDomElement effect)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Add Track effect outisde of range";
        return;
    }
    effect.setAttribute("kdenlive_ix", m_tracksList.at(ix).effectsList.count() + 1);

    // Init parameter value & keyframes if required
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();

        // Check if this effect has a variable parameter
        if (e.attribute("default").contains('%')) {
            double evaluatedValue = ProfilesDialog::getStringEval(m_profile, e.attribute("default"));
            e.setAttribute("default", evaluatedValue);
            if (e.hasAttribute("value") && e.attribute("value").startsWith('%')) {
                e.setAttribute("value", evaluatedValue);
            }
        }

        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe")) {
            QString def = e.attribute("default");
            // Effect has a keyframe type parameter, we need to set the values
            if (e.attribute("keyframes").isEmpty()) {
                e.setAttribute("keyframes", "0:" + def + ';');
                kDebug() << "///// EFFECT KEYFRAMES INITED: " << e.attribute("keyframes");
                //break;
            }
        }

        if (effect.attribute("id") == "crop") {
            // default use_profile to 1 for clips with proxies to avoid problems when rendering
            if (e.attribute("name") == "use_profile" && getDocumentProperty("enableproxy") == "1")
                e.setAttribute("value", "1");
        }
    }

    m_tracksList[ix].effectsList.append(effect);
}

void KdenliveDoc::removeTrackEffect(int ix, QDomElement effect)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Remove Track effect outisde of range";
        return;
    }
    int index;
    int toRemove = effect.attribute("kdenlive_ix").toInt();
    for (int i = 0; i < m_tracksList.at(ix).effectsList.count(); ++i) {
        index = m_tracksList.at(ix).effectsList.at(i).attribute("kdenlive_ix").toInt();
        if (toRemove == index) {
            m_tracksList[ix].effectsList.removeAt(toRemove);
            break;
        }
    }
}

void KdenliveDoc::setTrackEffect(int trackIndex, int effectIndex, QDomElement effect)
{
    if (trackIndex < 0 || trackIndex >= m_tracksList.count()) {
        kWarning() << "Set Track effect outisde of range";
        return;
    }
    if (effectIndex <= 0 || effectIndex > (m_tracksList.at(trackIndex).effectsList.count()) || effect.isNull()) {
        kDebug() << "Invalid effect index: " << effectIndex;
        return;
    }
    m_tracksList[trackIndex].effectsList.removeAt(effect.attribute("kdenlive_ix").toInt());
    effect.setAttribute("kdenlive_ix", effectIndex);
    m_tracksList[trackIndex].effectsList.insert(effect);
    //m_tracksList[trackIndex].effectsList.updateEffect(effect);
}

void KdenliveDoc::enableTrackEffects(int trackIndex, QList <int> effectIndexes, bool disable)
{
    if (trackIndex < 0 || trackIndex >= m_tracksList.count()) {
        kWarning() << "Set Track effect outisde of range";
        return;
    }
    EffectsList list = m_tracksList.at(trackIndex).effectsList;
    QDomElement effect;
    for (int i = 0; i < effectIndexes.count(); i++) {
	effect = list.itemFromIndex(effectIndexes.at(i));
	if (!effect.isNull()) effect.setAttribute("disable", (int) disable);
    }
}

const EffectsList KdenliveDoc::getTrackEffects(int ix)
{
    if (ix < 0 || ix >= m_tracksList.count()) {
        kWarning() << "Get Track effects outisde of range";
        return EffectsList();
    }
    return m_tracksList.at(ix).effectsList;
}

QDomElement KdenliveDoc::getTrackEffect(int trackIndex, int effectIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_tracksList.count()) {
        kWarning() << "Get Track effect outisde of range";
        return QDomElement();
    }
    EffectsList list = m_tracksList.at(trackIndex).effectsList;
    if (effectIndex > list.count() || effectIndex < 1 || list.itemFromIndex(effectIndex).isNull()) return QDomElement();
    return list.itemFromIndex(effectIndex).cloneNode().toElement();
}

int KdenliveDoc::hasTrackEffect(int trackIndex, const QString &tag, const QString &id) const
{
    if (trackIndex < 0 || trackIndex >= m_tracksList.count()) {
        kWarning() << "Get Track effect outisde of range";
        return -1;
    }
    EffectsList list = m_tracksList.at(trackIndex).effectsList;
    return list.hasEffect(tag, id);
}

bool KdenliveDoc::saveCustomEffects(QDomNodeList customeffects)
{
    QDomElement e;
    QStringList importedEffects;
    int maxchild = customeffects.count();
    for (int i = 0; i < maxchild; i++) {
        e = customeffects.at(i).toElement();
        QString id = e.attribute("id");
        QString tag = e.attribute("tag");
        if (!id.isEmpty()) {
            // Check if effect exists or save it
            if (MainWindow::customEffects.hasEffect(tag, id) == -1) {
                QDomDocument doc;
                doc.appendChild(doc.importNode(e, true));
                QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
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
    if (!importedEffects.isEmpty()) KMessageBox::informationList(kapp->activeWindow(), i18n("The following effects were imported from the project:"), importedEffects);
    return (!importedEffects.isEmpty());
}

void KdenliveDoc::updateProjectFolderPlacesEntry()
{
    /*
     * For similar and more code have a look at kfileplacesmodel.cpp and the included files:
     * http://websvn.kde.org/trunk/KDE/kdelibs/kfile/kfileplacesmodel.cpp?view=markup
     */

    const QString file = KStandardDirs::locateLocal("data", "kfileplaces/bookmarks.xml");
    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForFile(file, "kfilePlaces");
    if (!bookmarkManager) return;
    KBookmarkGroup root = bookmarkManager->root();
    
    KBookmark bookmark = root.first();

    QString kdenliveName = KGlobal::mainComponent().componentName();
    KUrl documentLocation = m_projectFolder;

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

// static
double KdenliveDoc::getDisplayRatio(const QString &path)
{
    QFile file(path);
    QDomDocument doc;
    if (!file.open(QIODevice::ReadOnly)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
        return 0;
    }
    if (!doc.setContent(&file)) {
        kWarning() << "ERROR, CANNOT READ: " << path;
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
    KUrl backupFile = m_projectFolder;
    backupFile.addPath(".backup/");
    KIO::NetAccess::mkdir(backupFile, kapp->activeWindow());
    QString fileName = KUrl(path).fileName().section('.', 0, -2);
    QFileInfo info(file);
    fileName.append('-' + m_documentProperties.value("documentid"));
    fileName.append(info.lastModified().toString("-yyyy-MM-dd-hh-mm"));
    fileName.append(".kdenlive");
    backupFile.addPath(fileName);

    if (file.exists()) {
        // delete previous backup if it was done less than 60 seconds ago
        QFile::remove(backupFile.path());
        if (!QFile::copy(path, backupFile.path())) {
            KMessageBox::information(kapp->activeWindow(), i18n("Cannot create backup copy:\n%1", backupFile.path()));
        }
    }    
}

void KdenliveDoc::cleanupBackupFiles()
{
    KUrl backupFile = m_projectFolder;
    backupFile.addPath(".backup/");
    QDir dir(backupFile.path());
    QString projectFile = url().fileName().section('.', 0, -2);
    projectFile.append('-' + m_documentProperties.value("documentid"));
    projectFile.append("-??");
    projectFile.append("??");
    projectFile.append("-??");
    projectFile.append("-??");
    projectFile.append("-??");
    projectFile.append("-??.kdenlive");

    QStringList filter;
    backupFile.addPath(projectFile);
    filter << projectFile;
    dir.setNameFilters(filter);
    QFileInfoList resultList = dir.entryInfoList(QDir::Files, QDir::Time);

    QDateTime d = QDateTime::currentDateTime();
    QStringList hourList;
    QStringList dayList;
    QStringList weekList;
    QStringList oldList;
    for (int i = 0; i < resultList.count(); i++) {
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
            kDebug()<<"REMOVE AT: "<<i<<", COUNT: "<<hourList.count();
            hourList.removeAt(i);
            i--;
        }
    } else hourList.clear();
    if (dayList.count() > 20) {
        int step = dayList.count() / 10;
        for (int i = 0; i < dayList.count(); i += step) {
            dayList.removeAt(i);
            i--;
        }
    } else dayList.clear();
    if (weekList.count() > 20) {
        int step = weekList.count() / 10;
        for (int i = 0; i < weekList.count(); i += step) {
            weekList.removeAt(i);
            i--;
        }
    } else weekList.clear();
    if (oldList.count() > 20) {
        int step = oldList.count() / 10;
        for (int i = 0; i < oldList.count(); i += step) {
            oldList.removeAt(i);
            i--;
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

void KdenliveDoc::setMetadata(const QMap <QString, QString> meta)
{
    m_documentMetadata = meta;
}

#include "kdenlivedoc.moc"

