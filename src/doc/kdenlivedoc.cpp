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
#include "bin/bin.h"
#include "bin/bincommands.h"
#include "bin/binplaylist.hpp"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "documentchecker.h"
#include "documentvalidator.h"
#include "docundostack.hpp"
#include "effects/effectsrepository.hpp"
#include "jobs/jobmanager.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mltcontroller/clipcontroller.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectcommands.h"
#include "titler/titlewidget.h"
#include "transitions/transitionsrepository.hpp"

#include <config-kdenlive.h>

#include <KBookmark>
#include <KBookmarkManager>
#include <KIO/CopyJob>
#include <KIO/FileCopyJob>
#include <KIO/JobUiDelegate>
#include <KMessageBox>
#include <klocalizedstring.h>

#include "kdenlive_debug.h"
#include <QCryptographicHash>
#include <QDomImplementation>
#include <QFile>
#include <QFileDialog>
#include <QUndoGroup>
#include <QUndoStack>

#include <KJobWidgets/KJobWidgets>
#include <QStandardPaths>
#include <mlt++/Mlt.h>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

const double DOCUMENTVERSION = 1.00;

KdenliveDoc::KdenliveDoc(const QUrl &url, QString projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QMap<QString, QString> &properties,
                         const QMap<QString, QString> &metadata, const QPair<int, int> &tracks, int audioChannels, bool *openBackup, MainWindow *parent)
    : QObject(parent)
    , m_autosave(nullptr)
    , m_url(url)
    , m_clipsCount(0)
    , m_commandStack(std::make_shared<DocUndoStack>(undoGroup))
    , m_modified(false)
    , m_documentOpenStatus(CleanProject)
    , m_projectFolder(std::move(projectFolder))
{
    m_guideModel.reset(new MarkerListModel(m_commandStack, this));
    connect(m_guideModel.get(), &MarkerListModel::modelChanged, this, &KdenliveDoc::guidesChanged);
    connect(this, SIGNAL(updateCompositionMode(int)), parent, SLOT(slotUpdateCompositeAction(int)));
    bool success = false;
    connect(m_commandStack.get(), &QUndoStack::indexChanged, this, &KdenliveDoc::slotModified);
    connect(m_commandStack.get(), &DocUndoStack::invalidate, this, &KdenliveDoc::checkPreviewStack, Qt::DirectConnection);
    // connect(m_commandStack, SIGNAL(cleanChanged(bool)), this, SLOT(setModified(bool)));

    // init default document properties
    m_documentProperties[QStringLiteral("zoom")] = QLatin1Char('8');
    m_documentProperties[QStringLiteral("verticalzoom")] = QLatin1Char('1');
    m_documentProperties[QStringLiteral("zonein")] = QLatin1Char('0');
    m_documentProperties[QStringLiteral("zoneout")] = QStringLiteral("-1");
    m_documentProperties[QStringLiteral("enableproxy")] = QString::number((int)KdenliveSettings::enableproxy());
    m_documentProperties[QStringLiteral("proxyparams")] = KdenliveSettings::proxyparams();
    m_documentProperties[QStringLiteral("proxyextension")] = KdenliveSettings::proxyextension();
    m_documentProperties[QStringLiteral("previewparameters")] = KdenliveSettings::previewparams();
    m_documentProperties[QStringLiteral("previewextension")] = KdenliveSettings::previewextension();
    m_documentProperties[QStringLiteral("externalproxyparams")] = KdenliveSettings::externalProxyProfile();
    m_documentProperties[QStringLiteral("enableexternalproxy")] = QString::number((int)KdenliveSettings::externalproxy());
    m_documentProperties[QStringLiteral("generateproxy")] = QString::number((int)KdenliveSettings::generateproxy());
    m_documentProperties[QStringLiteral("proxyminsize")] = QString::number(KdenliveSettings::proxyminsize());
    m_documentProperties[QStringLiteral("generateimageproxy")] = QString::number((int)KdenliveSettings::generateimageproxy());
    m_documentProperties[QStringLiteral("proxyimageminsize")] = QString::number(KdenliveSettings::proxyimageminsize());
    m_documentProperties[QStringLiteral("proxyimagesize")] = QString::number(KdenliveSettings::proxyimagesize());
    m_documentProperties[QStringLiteral("videoTarget")] = QString::number(tracks.second);
    m_documentProperties[QStringLiteral("audioTarget")] = QString::number(tracks.second - 1);
    m_documentProperties[QStringLiteral("activeTrack")] = QString::number(tracks.second);
    m_documentProperties[QStringLiteral("audioChannels")] = QString::number(audioChannels);
    m_documentProperties[QStringLiteral("enableTimelineZone")] = QLatin1Char('0');
    m_documentProperties[QStringLiteral("zonein")] = QLatin1Char('0');
    m_documentProperties[QStringLiteral("zoneout")] = QStringLiteral("75");
    m_documentProperties[QStringLiteral("seekOffset")] = QString::number(TimelineModel::seekDuration);

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
    *openBackup = false;
    if (url.isValid()) {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // The file cannot be opened
            if (KMessageBox::warningContinueCancel(parent, i18n("Cannot open the project file,\nDo you want to open a backup file?"),
                                                   i18n("Error opening file"), KGuiItem(i18n("Open Backup"))) == KMessageBox::Continue) {
                *openBackup = true;
            }
            // KMessageBox::error(parent, KIO::NetAccess::lastErrorString());
        } else {
            qCDebug(KDENLIVE_LOG) << " // / processing file open";
            QString errorMsg;
            int line;
            int col;
            QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidChars);
            success = m_document.setContent(&file, false, &errorMsg, &line, &col);
            file.close();

            if (!success) {
                // It is corrupted
                int answer = KMessageBox::warningYesNoCancel(
                    parent, i18n("Cannot open the project file, error is:\n%1 (line %2, col %3)\nDo you want to open a backup file?", errorMsg, line, col),
                    i18n("Error opening file"), KGuiItem(i18n("Open Backup")), KGuiItem(i18n("Recover")));
                if (answer == KMessageBox::Yes) {
                    *openBackup = true;
                } else if (answer == KMessageBox::No) {
                    // Try to recover broken file produced by Kdenlive 0.9.4
                    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        int correction = 0;
                        QString playlist = QString::fromUtf8(file.readAll());
                        while (!success && correction < 2) {
                            int errorPos = 0;
                            line--;
                            col = col - 2;
                            for (int k = 0; k < line && errorPos < playlist.length(); ++k) {
                                errorPos = playlist.indexOf(QLatin1Char('\n'), errorPos);
                                errorPos++;
                            }
                            errorPos += col;
                            if (errorPos >= playlist.length()) {
                                break;
                            }
                            playlist.remove(errorPos, 1);
                            line = 0;
                            col = 0;
                            success = m_document.setContent(playlist, false, &errorMsg, &line, &col);
                            correction++;
                        }
                        if (!success) {
                            KMessageBox::sorry(parent, i18n("Cannot recover this project file"));
                        } else {
                            // Document was modified, ask for backup
                            QDomElement mlt = m_document.documentElement();
                            mlt.setAttribute(QStringLiteral("modified"), 1);
                        }
                    }
                }
            } else {
                qCDebug(KDENLIVE_LOG) << " // / processing file open: validate";
                pCore->displayMessage(i18n("Validating"), OperationCompletedMessage, 100);
                qApp->processEvents();
                DocumentValidator validator(m_document, url);
                success = validator.isProject();
                if (!success) {
                    // It is not a project file
                    pCore->displayMessage(i18n("File %1 is not a Kdenlive project file", m_url.toLocalFile()), OperationCompletedMessage, 100);
                    if (KMessageBox::warningContinueCancel(
                            parent, i18n("File %1 is not a valid project file.\nDo you want to open a backup file?", m_url.toLocalFile()),
                            i18n("Error opening file"), KGuiItem(i18n("Open Backup"))) == KMessageBox::Continue) {
                        *openBackup = true;
                    }
                } else {
                    /*
                     * Validate the file against the current version (upgrade
                     * and recover it if needed). It is NOT a passive operation
                     */
                    // TODO: backup the document or alert the user?
                    auto validationResult = validator.validate(DOCUMENTVERSION);
                    success = validationResult.first;

                    if (!validationResult.second.isEmpty()) {
                        qDebug() << "DECIMAL POINT has changed to ., was " << validationResult.second;
                        m_modifiedDecimalPoint = validationResult.second;
                    }

                    if (success && !KdenliveSettings::gpu_accel()) {
                        success = validator.checkMovit();
                    }
                    if (success) { // Let the validator handle error messages
                        qCDebug(KDENLIVE_LOG) << " // / processing file validate ok";
                        pCore->displayMessage(i18n("Check missing clips"), InformationMessage, 300);
                        qApp->processEvents();
                        DocumentChecker d(m_url, m_document);
                        success = !d.hasErrorInClips();
                        if (success) {
                            loadDocumentProperties();
                            if (m_document.documentElement().hasAttribute(QStringLiteral("upgraded"))) {
                                m_documentOpenStatus = UpgradedProject;
                                pCore->displayMessage(i18n("Your project was upgraded, a backup will be created on next save"), ErrorMessage);
                            } else if (m_document.documentElement().hasAttribute(QStringLiteral("modified")) || validator.isModified()) {
                                m_documentOpenStatus = ModifiedProject;
                                pCore->displayMessage(i18n("Your project was modified on opening, a backup will be created on next save"), ErrorMessage);
                                setModified(true);
                            }
                            pCore->displayMessage(QString(), OperationCompletedMessage);
                        }
                    }
                }
            }
        }
    }

    // Something went wrong, or a new file was requested: create a new project
    if (!success) {
        m_url.clear();
        pCore->setCurrentProfile(profileName);
        m_document = createEmptyDocument(tracks.first, tracks.second);
        updateProjectProfile(false);
    } else {
        m_clipsCount = m_document.elementsByTagName(QLatin1String("entry")).size();
    }

    if (!m_projectFolder.isEmpty()) {
        // Ask to create the project directory if it does not exist
        QDir folder(m_projectFolder);
        if (!folder.mkpath(QStringLiteral("."))) {
            // Project folder is not writable
            m_projectFolder = m_url.toString(QUrl::RemoveFilename | QUrl::RemoveScheme);
            folder.setPath(m_projectFolder);
            if (folder.exists()) {
                KMessageBox::sorry(
                    parent,
                    i18n("The project directory %1, could not be created.\nPlease make sure you have the required permissions.\nDefaulting to system folders",
                         m_projectFolder));
            } else {
                KMessageBox::information(parent, i18n("Document project folder is invalid, using system default folders"));
            }
            m_projectFolder.clear();
        }
    }
    initCacheDirs();

    updateProjectFolderPlacesEntry();
}

KdenliveDoc::~KdenliveDoc()
{
    if (m_url.isEmpty()) {
        // Document was never saved, delete cache folder
        QString documentId = QDir::cleanPath(getDocumentProperty(QStringLiteral("documentid")));
        bool ok;
        documentId.toLongLong(&ok, 10);
        if (ok && !documentId.isEmpty()) {
            QDir baseCache = getCacheDir(CacheBase, &ok);
            if (baseCache.dirName() == documentId && baseCache.entryList(QDir::Files).isEmpty()) {
                baseCache.removeRecursively();
            }
        }
    }
    // qCDebug(KDENLIVE_LOG) << "// DEL CLP MAN";
    // Clean up guide model
    m_guideModel.reset();
    // qCDebug(KDENLIVE_LOG) << "// DEL CLP MAN done";
    if (m_autosave) {
        if (!m_autosave->fileName().isEmpty()) {
            m_autosave->remove();
        }
        delete m_autosave;
    }
}

int KdenliveDoc::clipsCount() const
{
    return m_clipsCount;
}


const QByteArray KdenliveDoc::getAndClearProjectXml()
{
    const QByteArray result = m_document.toString().toUtf8();
    // We don't need the xml data anymore, throw away
    m_document.clear();
    return result;
}

QDomDocument KdenliveDoc::createEmptyDocument(int videotracks, int audiotracks)
{
    QList<TrackInfo> tracks;
    // Tracks are added «backwards», so we need to reverse the track numbering
    // mbt 331: http://www.kdenlive.org/mantis/view.php?id=331
    // Better default names for tracks: Audio 1 etc. instead of blank numbers
    tracks.reserve(audiotracks + videotracks);
    for (int i = 0; i < audiotracks; ++i) {
        TrackInfo audioTrack;
        audioTrack.type = AudioTrack;
        audioTrack.isMute = false;
        audioTrack.isBlind = true;
        audioTrack.isLocked = false;
        // audioTrack.trackName = i18n("Audio %1", audiotracks - i);
        audioTrack.duration = 0;
        tracks.append(audioTrack);
    }
    for (int i = 0; i < videotracks; ++i) {
        TrackInfo videoTrack;
        videoTrack.type = VideoTrack;
        videoTrack.isMute = false;
        videoTrack.isBlind = false;
        videoTrack.isLocked = false;
        // videoTrack.trackName = i18n("Video %1", i + 1);
        videoTrack.duration = 0;
        tracks.append(videoTrack);
    }
    return createEmptyDocument(tracks);
}

QDomDocument KdenliveDoc::createEmptyDocument(const QList<TrackInfo> &tracks)
{
    // Creating new document
    QDomDocument doc;
    Mlt::Profile docProfile;
    Mlt::Consumer xmlConsumer(docProfile, "xml:kdenlive_playlist");
    xmlConsumer.set("no_profile", 1);
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("store", "kdenlive");
    Mlt::Tractor tractor(docProfile);
    Mlt::Producer bk(docProfile, "color:black");
    tractor.insert_track(bk, 0);
    for (int i = 0; i < tracks.count(); ++i) {
        Mlt::Tractor track(docProfile);
        track.set("kdenlive:track_name", tracks.at(i).trackName.toUtf8().constData());
        track.set("kdenlive:timeline_active", 1);
        track.set("kdenlive:trackheight", KdenliveSettings::trackheight());
        if (tracks.at(i).type == AudioTrack) {
            track.set("kdenlive:audio_track", 1);
        }
        if (tracks.at(i).isLocked) {
            track.set("kdenlive:locked_track", 1);
        }
        if (tracks.at(i).isMute) {
            if (tracks.at(i).isBlind) {
                track.set("hide", 3);
            } else {
                track.set("hide", 2);
            }
        } else if (tracks.at(i).isBlind) {
            track.set("hide", 1);
        }
        Mlt::Playlist playlist1(docProfile);
        Mlt::Playlist playlist2(docProfile);
        track.insert_track(playlist1, 0);
        track.insert_track(playlist2, 1);
        tractor.insert_track(track, i + 1);
    }
    QScopedPointer<Mlt::Field> field(tractor.field());
    QString compositeService = TransitionsRepository::get()->getCompositingTransition();
    if (!compositeService.isEmpty()) {
        for (int i = 0; i <= tracks.count(); i++) {
            if (i > 0 && tracks.at(i - 1).type == AudioTrack) {
                Mlt::Transition tr(docProfile, "mix");
                tr.set("a_track", 0);
                tr.set("b_track", i);
                tr.set("always_active", 1);
                tr.set("sum", 1);
                tr.set("internal_added", 237);
                field->plant_transition(tr, 0, i);
            }
            if (i > 0 && tracks.at(i - 1).type == VideoTrack) {
                Mlt::Transition tr(docProfile, compositeService.toUtf8().constData());
                tr.set("a_track", 0);
                tr.set("b_track", i);
                tr.set("always_active", 1);
                tr.set("internal_added", 237);
                field->plant_transition(tr, 0, i);
            }
        }
    }
    Mlt::Producer prod(tractor.get_producer());
    xmlConsumer.connect(prod);
    xmlConsumer.run();
    QString playlist = QString::fromUtf8(xmlConsumer.get("kdenlive_playlist"));
    doc.setContent(playlist);
    return doc;
}

bool KdenliveDoc::useProxy() const
{
    return m_documentProperties.value(QStringLiteral("enableproxy")).toInt() != 0;
}

bool KdenliveDoc::useExternalProxy() const
{
    return m_documentProperties.value(QStringLiteral("enableexternalproxy")).toInt() != 0;
}

bool KdenliveDoc::autoGenerateProxy(int width) const
{
    return (m_documentProperties.value(QStringLiteral("generateproxy")).toInt() != 0) &&
           width > m_documentProperties.value(QStringLiteral("proxyminsize")).toInt();
}

bool KdenliveDoc::autoGenerateImageProxy(int width) const
{
    return (m_documentProperties.value(QStringLiteral("generateimageproxy")).toInt() != 0) &&
           width > m_documentProperties.value(QStringLiteral("proxyimageminsize")).toInt();
}

void KdenliveDoc::slotAutoSave(const QString &scene)
{
    if (m_autosave != nullptr) {
        if (!m_autosave->isOpen() && !m_autosave->open(QIODevice::ReadWrite)) {
            // show error: could not open the autosave file
            qCDebug(KDENLIVE_LOG) << "ERROR; CANNOT CREATE AUTOSAVE FILE";
            pCore->displayMessage(i18n("Cannot create autosave file %1", m_autosave->fileName()), ErrorMessage);
            return;
        }
        if (scene.isEmpty()) {
            // Make sure we don't save if scenelist is corrupted
            KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1, scene list is corrupted.", m_autosave->fileName()));
            return;
        }
        m_autosave->resize(0);
        if (m_autosave->write(scene.toUtf8()) < 0) {
            pCore->displayMessage(i18n("Cannot create autosave file %1", m_autosave->fileName()), ErrorMessage);
        };
        m_autosave->flush();
    }
}

void KdenliveDoc::setZoom(int horizontal, int vertical)
{
    m_documentProperties[QStringLiteral("zoom")] = QString::number(horizontal);
    if (vertical > -1) {
        m_documentProperties[QStringLiteral("verticalzoom")] = QString::number(vertical);
    }
}

QPoint KdenliveDoc::zoom() const
{
    return QPoint(m_documentProperties.value(QStringLiteral("zoom")).toInt(), m_documentProperties.value(QStringLiteral("verticalzoom")).toInt());
}

void KdenliveDoc::setZone(int start, int end)
{
    m_documentProperties[QStringLiteral("zonein")] = QString::number(start);
    m_documentProperties[QStringLiteral("zoneout")] = QString::number(end);
}

QPoint KdenliveDoc::zone() const
{
    return QPoint(m_documentProperties.value(QStringLiteral("zonein")).toInt(), m_documentProperties.value(QStringLiteral("zoneout")).toInt());
}

QPair<int, int> KdenliveDoc::targetTracks() const
{
    return {m_documentProperties.value(QStringLiteral("videoTarget")).toInt(), m_documentProperties.value(QStringLiteral("audioTarget")).toInt()};
}

QDomDocument KdenliveDoc::xmlSceneList(const QString &scene)
{
    QDomDocument sceneList;
    sceneList.setContent(scene, true);
    QDomElement mlt = sceneList.firstChildElement(QStringLiteral("mlt"));
    if (mlt.isNull() || !mlt.hasChildNodes()) {
        // scenelist is corrupted
        return QDomDocument();
    }

    // Set playlist audio volume to 100%
    QDomNodeList tractors = mlt.elementsByTagName(QStringLiteral("tractor"));
    for (int i = 0; i < tractors.count(); ++i) {
        if (tractors.at(i).toElement().hasAttribute(QStringLiteral("global_feed"))) {
            // This is our main tractor
            QDomElement tractor = tractors.at(i).toElement();
            if (Xml::hasXmlProperty(tractor, QLatin1String("meta.volume"))) {
                Xml::setXmlProperty(tractor, QStringLiteral("meta.volume"), QStringLiteral("1"));
            }
            break;
        }
    }
    QDomNodeList tracks = mlt.elementsByTagName(QStringLiteral("track"));
    if (tracks.isEmpty()) {
        // Something is very wrong, inform user.
        qDebug()<<" = = = =  = =  CORRUPTED DOC\n"<<scene;
        return QDomDocument();
    }

    QDomNodeList pls = mlt.elementsByTagName(QStringLiteral("playlist"));
    QDomElement mainPlaylist;
    for (int i = 0; i < pls.count(); ++i) {
        if (pls.at(i).toElement().attribute(QStringLiteral("id")) == BinPlaylist::binPlaylistId) {
            mainPlaylist = pls.at(i).toElement();
            break;
        }
    }

    // check if project contains custom effects to embed them in project file
    QDomNodeList effects = mlt.elementsByTagName(QStringLiteral("filter"));
    int maxEffects = effects.count();
    // qCDebug(KDENLIVE_LOG) << "// FOUD " << maxEffects << " EFFECTS+++++++++++++++++++++";
    QMap<QString, QString> effectIds;
    for (int i = 0; i < maxEffects; ++i) {
        QDomNode m = effects.at(i);
        QDomNodeList params = m.childNodes();
        QString id;
        QString tag;
        for (int j = 0; j < params.count(); ++j) {
            QDomElement e = params.item(j).toElement();
            if (e.attribute(QStringLiteral("name")) == QLatin1String("kdenlive_id")) {
                id = e.firstChild().nodeValue();
            }
            if (e.attribute(QStringLiteral("name")) == QLatin1String("tag")) {
                tag = e.firstChild().nodeValue();
            }
            if (!id.isEmpty() && !tag.isEmpty()) {
                effectIds.insert(id, tag);
            }
        }
    }
    // TODO: find a way to process this before rendering MLT scenelist to xml
    /*QDomDocument customeffects = initEffects::getUsedCustomEffects(effectIds);
    if (!customeffects.documentElement().childNodes().isEmpty()) {
        Xml::setXmlProperty(mainPlaylist, QStringLiteral("kdenlive:customeffects"), customeffects.toString());
    }*/
    // addedXml.appendChild(sceneList.importNode(customeffects.documentElement(), true));

    // TODO: move metadata to previous step in saving process
    QDomElement docmetadata = sceneList.createElement(QStringLiteral("documentmetadata"));
    QMapIterator<QString, QString> j(m_documentMetadata);
    while (j.hasNext()) {
        j.next();
        docmetadata.setAttribute(j.key(), j.value());
    }
    // addedXml.appendChild(docmetadata);

    return sceneList;
}

bool KdenliveDoc::saveSceneList(const QString &path, const QString &scene)
{
    QLocale currentLocale; // For restoring after XML export
    qDebug() << "Current locale is " << currentLocale;
    QLocale::setDefault(QLocale::c()); // Not sure if helpful …
    QDomDocument sceneList = xmlSceneList(scene);
    if (sceneList.isNull()) {
        // Make sure we don't save if scenelist is corrupted
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1, scene list is corrupted.", path));
        return false;
    }

    // Backup current version
    backupLastSavedVersion(path);
    if (m_documentOpenStatus != CleanProject) {
        // create visible backup file and warn user
        QString baseFile = path.section(QStringLiteral(".kdenlive"), 0, 0);
        int ct = 0;
        QString backupFile = baseFile + QStringLiteral("_backup") + QString::number(ct) + QStringLiteral(".kdenlive");
        while (QFile::exists(backupFile)) {
            ct++;
            backupFile = baseFile + QStringLiteral("_backup") + QString::number(ct) + QStringLiteral(".kdenlive");
        }
        QString message;
        if (m_documentOpenStatus == UpgradedProject) {
            message =
                i18n("Your project file was upgraded to the latest Kdenlive document version.\nTo make sure you do not lose data, a backup copy called %1 "
                     "was created.",
                     backupFile);
        } else {
            message = i18n("Your project file was modified by Kdenlive.\nTo make sure you do not lose data, a backup copy called %1 was created.", backupFile);
        }

        KIO::FileCopyJob *copyjob = KIO::file_copy(QUrl::fromLocalFile(path), QUrl::fromLocalFile(backupFile));
        if (copyjob->exec()) {
            KMessageBox::information(QApplication::activeWindow(), message);
            m_documentOpenStatus = CleanProject;
        } else {
            KMessageBox::information(
                QApplication::activeWindow(),
                i18n("Your project file was upgraded to the latest Kdenlive document version, but it was not possible to create the backup copy %1.",
                     backupFile));
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: " << path;
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", path));
        return false;
    }

    const QByteArray sceneData = sceneList.toString().toUtf8();
    QLocale::setDefault(currentLocale);

    file.write(sceneData);
    if (!file.commit()) {
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", path));
        return false;
    }
    cleanupBackupFiles();
    QFileInfo info(path);
    QString fileName = QUrl::fromLocalFile(path).fileName().section(QLatin1Char('.'), 0, -2);
    fileName.append(QLatin1Char('-') + m_documentProperties.value(QStringLiteral("documentid")));
    fileName.append(info.lastModified().toString(QStringLiteral("-yyyy-MM-dd-hh-mm")));
    fileName.append(QStringLiteral(".kdenlive.png"));
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    emit saveTimelinePreview(backupFolder.absoluteFilePath(fileName));
    return true;
}

QString KdenliveDoc::projectTempFolder() const
{
    if (m_projectFolder.isEmpty()) {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    return m_projectFolder;
}

QString KdenliveDoc::projectDataFolder() const
{
    if (m_projectFolder.isEmpty()) {
        if (KdenliveSettings::customprojectfolder()) {
            return KdenliveSettings::defaultprojectfolder();
        }
        return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    }
    return m_projectFolder;
}

void KdenliveDoc::setProjectFolder(const QUrl &url)
{
    if (url == QUrl::fromLocalFile(m_projectFolder)) {
        return;
    }
    setModified(true);
    QDir dir(url.toLocalFile());
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }
    dir.mkdir(QStringLiteral("titles"));
    /*if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("You have changed the project folder. Do you want to copy the cached data from %1 to the
     * new folder %2?", m_projectFolder, url.path())) == KMessageBox::Yes) moveProjectData(url);*/
    m_projectFolder = url.toLocalFile();

    updateProjectFolderPlacesEntry();
}

void KdenliveDoc::moveProjectData(const QString & /*src*/, const QString &dest)
{
    // Move proxies

    QList<QUrl> cacheUrls;
    auto binClips = pCore->projectItemModel()->getAllClipIds();
    // First step: all clips referenced by the bin model exist and are inserted
    for (const auto &binClip : binClips) {
        auto projClip = pCore->projectItemModel()->getClipByBinID(binClip);
        if (projClip->clipType() == ClipType::Text) {
            // the image for title clip must be moved
            QUrl oldUrl = QUrl::fromLocalFile(projClip->clipUrl());
            if (!oldUrl.isEmpty()) {
                QUrl newUrl = QUrl::fromLocalFile(dest + QStringLiteral("/titles/") + oldUrl.fileName());
                KIO::Job *job = KIO::copy(oldUrl, newUrl);
                if (job->exec()) {
                    projClip->setProducerProperty(QStringLiteral("resource"), newUrl.toLocalFile());
                }
            }
            continue;
        }
        QString proxy = projClip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
        if (proxy.length() > 2 && QFile::exists(proxy)) {
            QUrl pUrl = QUrl::fromLocalFile(proxy);
            if (!cacheUrls.contains(pUrl)) {
                cacheUrls << pUrl;
            }
        }
    }
    if (!cacheUrls.isEmpty()) {
        QDir proxyDir(dest + QStringLiteral("/proxy/"));
        if (proxyDir.mkpath(QStringLiteral("."))) {
            KIO::CopyJob *job = KIO::move(cacheUrls, QUrl::fromLocalFile(proxyDir.absolutePath()));
            KJobWidgets::setWindow(job, QApplication::activeWindow());
            if (static_cast<int>(job->exec()) > 0) {
                KMessageBox::sorry(QApplication::activeWindow(), i18n("Moving proxy clips failed: %1", job->errorText()));
            }
        }
    }
}

bool KdenliveDoc::profileChanged(const QString &profile) const
{
    return pCore->getCurrentProfile() != ProfileRepository::get()->getProfile(profile);
}

Render *KdenliveDoc::renderer()
{
    return nullptr;
}

std::shared_ptr<DocUndoStack> KdenliveDoc::commandStack()
{
    return m_commandStack;
}

int KdenliveDoc::getFramePos(const QString &duration)
{
    return m_timecode.getFrameCount(duration);
}

Timecode KdenliveDoc::timecode() const
{
    return m_timecode;
}

int KdenliveDoc::width() const
{
    return pCore->getCurrentProfile()->width();
}

int KdenliveDoc::height() const
{
    return pCore->getCurrentProfile()->height();
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
    setModified(!m_commandStack->isClean());
}

void KdenliveDoc::setModified(bool mod)
{
    // fix mantis#3160: The document may have an empty URL if not saved yet, but should have a m_autosave in any case
    if ((m_autosave != nullptr) && mod && KdenliveSettings::crashrecovery()) {
        emit startAutoSave();
    }
    if (mod == m_modified) {
        return;
    }
    m_modified = mod;
    emit docModified(m_modified);
}

bool KdenliveDoc::isModified() const
{
    return m_modified;
}

const QString KdenliveDoc::description() const
{
    if (!m_url.isValid()) {
        return i18n("Untitled") + QStringLiteral("[*] / ") + pCore->getCurrentProfile()->description();
    }
    return m_url.fileName() + QStringLiteral(" [*]/ ") + pCore->getCurrentProfile()->description();
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
                    if (file.seek(file.size() - 1000000)) {
                        fileData.append(file.readAll());
                    }
                } else {
                    fileData = file.readAll();
                }
                file.close();
                fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
                if (QString::fromLatin1(fileHash.toHex()) == matchHash) {
                    return file.fileName();
                }
                qCDebug(KDENLIVE_LOG) << filesAndDirs.at(i) << "size match but not hash";
            }
        }
        ////qCDebug(KDENLIVE_LOG) << filesAndDirs.at(i) << file.size() << fileHash.toHex();
    }
    filesAndDirs = dir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
        foundFileName = searchFileRecursively(dir.absoluteFilePath(filesAndDirs.at(i)), matchSize, matchHash);
        if (!foundFileName.isEmpty()) {
            break;
        }
    }
    return foundFileName;
}


QStringList KdenliveDoc::getBinFolderClipIds(const QString &folderId) const
{
    return pCore->bin()->getBinFolderClipIds(folderId);
}

void KdenliveDoc::slotCreateTextTemplateClip(const QString &group, const QString &groupId, QUrl path)
{
    Q_UNUSED(group)
    // TODO refac: this seem to be a duplicate of ClipCreationDialog::createTitleTemplateClip. See if we can merge
    QString titlesFolder = QDir::cleanPath(m_projectFolder + QStringLiteral("/titles/"));
    if (path.isEmpty()) {
        QPointer<QFileDialog> d = new QFileDialog(QApplication::activeWindow(), i18n("Enter Template Path"), titlesFolder);
        d->setMimeTypeFilters(QStringList() << QStringLiteral("application/x-kdenlivetitle"));
        d->setFileMode(QFileDialog::ExistingFile);
        if (d->exec() == QDialog::Accepted && !d->selectedUrls().isEmpty()) {
            path = d->selectedUrls().first();
        }
        delete d;
    }

    if (path.isEmpty()) {
        return;
    }

    // TODO: rewrite with new title system (just set resource)
    QString id = ClipCreator::createTitleTemplate(path.toString(), QString(), i18n("Template title clip"), groupId, pCore->projectItemModel());
    emit selectLastAddedClip(id);
}

void KdenliveDoc::cacheImage(const QString &fileId, const QImage &img) const
{
    bool ok = false;
    QDir dir = getCacheDir(CacheThumbs, &ok);
    if (ok) {
        img.save(dir.absoluteFilePath(fileId + QStringLiteral(".png")));
    }
}

void KdenliveDoc::setDocumentProperty(const QString &name, const QString &value)
{
    if (value.isEmpty()) {
        m_documentProperties.remove(name);
        return;
    }
    m_documentProperties[name] = value;
}

const QString KdenliveDoc::getDocumentProperty(const QString &name, const QString &defaultValue) const
{
    return m_documentProperties.value(name, defaultValue);
}

QMap<QString, QString> KdenliveDoc::getRenderProperties() const
{
    QMap<QString, QString> renderProperties;
    QMapIterator<QString, QString> i(m_documentProperties);
    while (i.hasNext()) {
        i.next();
        if (i.key().startsWith(QLatin1String("render"))) {
            if (i.key() == QLatin1String("renderurl")) {
                // Check that we have a full path
                QString value = i.value();
                if (QFileInfo(value).isRelative()) {
                    value.prepend(m_documentRoot);
                }
                renderProperties.insert(i.key(), value);
            } else {
                renderProperties.insert(i.key(), i.value());
            }
        }
    }
    return renderProperties;
}

void KdenliveDoc::saveCustomEffects(const QDomNodeList &customeffects)
{
    QDomElement e;
    QStringList importedEffects;
    int maxchild = customeffects.count();
    QStringList newPaths;
    for (int i = 0; i < maxchild; ++i) {
        e = customeffects.at(i).toElement();
        const QString id = e.attribute(QStringLiteral("id"));
        if (!id.isEmpty()) {
            // Check if effect exists or save it
            if (EffectsRepository::get()->exists(id)) {
                QDomDocument doc;
                doc.appendChild(doc.importNode(e, true));
                QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects");
                path += id + QStringLiteral(".xml");
                if (!QFile::exists(path)) {
                    importedEffects << id;
                    newPaths << path;
                    QFile file(path);
                    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
                        QTextStream out(&file);
                        out << doc.toString();
                    }
                }
            }
        }
    }
    if (!importedEffects.isEmpty()) {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following effects were imported from the project:"), importedEffects);
    }
    if (!importedEffects.isEmpty()) {
        emit reloadEffects(newPaths);
    }
}

void KdenliveDoc::updateProjectFolderPlacesEntry()
{
    /*
     * For similar and more code have a look at kfileplacesmodel.cpp and the included files:
     * https://api.kde.org/frameworks/kio/html/kfileplacesmodel_8cpp_source.html
     */

    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/user-places.xbel");
    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForExternalFile(file);
    if (!bookmarkManager) {
        return;
    }
    KBookmarkGroup root = bookmarkManager->root();

    KBookmark bookmark = root.first();

    QString kdenliveName = QCoreApplication::applicationName();
    QUrl documentLocation = QUrl::fromLocalFile(m_projectFolder);

    bool exists = false;

    while (!bookmark.isNull()) {
        // UDI not empty indicates a device
        QString udi = bookmark.metaDataItem(QStringLiteral("UDI"));
        QString appName = bookmark.metaDataItem(QStringLiteral("OnlyInApp"));

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
        bookmark = root.addBookmark(i18n("Project Folder"), documentLocation, QStringLiteral("folder-favorites"));
        // Make this user selectable ?
        bookmark.setMetaDataItem(QStringLiteral("OnlyInApp"), kdenliveName);
        bookmarkManager->emitChanged(root);
    }
}

// static
double KdenliveDoc::getDisplayRatio(const QString &path)
{
    QFile file(path);
    QDomDocument doc;
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(KDENLIVE_LOG) << "ERROR, CANNOT READ: " << path;
        return 0;
    }
    if (!doc.setContent(&file)) {
        qCWarning(KDENLIVE_LOG) << "ERROR, CANNOT READ: " << path;
        file.close();
        return 0;
    }
    file.close();
    QDomNodeList list = doc.elementsByTagName(QStringLiteral("profile"));
    if (list.isEmpty()) {
        return 0;
    }
    QDomElement profile = list.at(0).toElement();
    double den = profile.attribute(QStringLiteral("display_aspect_den")).toDouble();
    if (den > 0) {
        return profile.attribute(QStringLiteral("display_aspect_num")).toDouble() / den;
    }
    return 0;
}

void KdenliveDoc::backupLastSavedVersion(const QString &path)
{
    // Ensure backup folder exists
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    QString fileName = QUrl::fromLocalFile(path).fileName().section(QLatin1Char('.'), 0, -2);
    QFileInfo info(file);
    fileName.append(QLatin1Char('-') + m_documentProperties.value(QStringLiteral("documentid")));
    fileName.append(info.lastModified().toString(QStringLiteral("-yyyy-MM-dd-hh-mm")));
    fileName.append(QStringLiteral(".kdenlive"));
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
    QDir backupFolder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/.backup"));
    QString projectFile = url().fileName().section(QLatin1Char('.'), 0, -2);
    projectFile.append(QLatin1Char('-') + m_documentProperties.value(QStringLiteral("documentid")));
    projectFile.append(QStringLiteral("-??"));
    projectFile.append(QStringLiteral("??"));
    projectFile.append(QStringLiteral("-??"));
    projectFile.append(QStringLiteral("-??"));
    projectFile.append(QStringLiteral("-??"));
    projectFile.append(QStringLiteral("-??.kdenlive"));

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
        } else if (d.secsTo(resultList.at(i).lastModified()) < 43200) {
            // files created in the day
            dayList.append(resultList.at(i).absoluteFilePath());
        } else if (d.daysTo(resultList.at(i).lastModified()) < 8) {
            // files created in the week
            weekList.append(resultList.at(i).absoluteFilePath());
        } else {
            // older files
            oldList.append(resultList.at(i).absoluteFilePath());
        }
    }
    if (hourList.count() > 20) {
        int step = hourList.count() / 10;
        for (int i = 0; i < hourList.count(); i += step) {
            // qCDebug(KDENLIVE_LOG)<<"REMOVE AT: "<<i<<", COUNT: "<<hourList.count();
            hourList.removeAt(i);
            --i;
        }
    } else {
        hourList.clear();
    }
    if (dayList.count() > 20) {
        int step = dayList.count() / 10;
        for (int i = 0; i < dayList.count(); i += step) {
            dayList.removeAt(i);
            --i;
        }
    } else {
        dayList.clear();
    }
    if (weekList.count() > 20) {
        int step = weekList.count() / 10;
        for (int i = 0; i < weekList.count(); i += step) {
            weekList.removeAt(i);
            --i;
        }
    } else {
        weekList.clear();
    }
    if (oldList.count() > 20) {
        int step = oldList.count() / 10;
        for (int i = 0; i < oldList.count(); i += step) {
            oldList.removeAt(i);
            --i;
        }
    } else {
        oldList.clear();
    }

    QString f;
    while (hourList.count() > 0) {
        f = hourList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
    }
    while (dayList.count() > 0) {
        f = dayList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
    }
    while (weekList.count() > 0) {
        f = weekList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
    }
    while (oldList.count() > 0) {
        f = oldList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
    }
}

const QMap<QString, QString> KdenliveDoc::metadata() const
{
    return m_documentMetadata;
}

void KdenliveDoc::setMetadata(const QMap<QString, QString> &meta)
{
    setModified(true);
    m_documentMetadata = meta;
}

void KdenliveDoc::slotProxyCurrentItem(bool doProxy, QList<std::shared_ptr<ProjectClip>> clipList, bool force, QUndoCommand *masterCommand)
{
    if (clipList.isEmpty()) {
        clipList = pCore->bin()->selectedClips();
    }
    bool hasParent = true;
    if (masterCommand == nullptr) {
        masterCommand = new QUndoCommand();
        if (doProxy) {
            masterCommand->setText(i18np("Add proxy clip", "Add proxy clips", clipList.count()));
        } else {
            masterCommand->setText(i18np("Remove proxy clip", "Remove proxy clips", clipList.count()));
        }
        hasParent = false;
    }

    // Make sure the proxy folder exists
    bool ok = false;
    QDir dir = getCacheDir(CacheProxy, &ok);
    if (!ok) {
        // Error
        return;
    }
    if (m_proxyExtension.isEmpty()) {
        initProxySettings();
    }
    QString extension = QLatin1Char('.') + m_proxyExtension;
    // getDocumentProperty(QStringLiteral("proxyextension"));
    /*QString params = getDocumentProperty(QStringLiteral("proxyparams"));
    if (params.contains(QStringLiteral("-s "))) {
        QString proxySize = params.section(QStringLiteral("-s "), 1).section(QStringLiteral("x"), 0, 0);
        extension.prepend(QStringLiteral("-") + proxySize);
    }*/

    // Prepare updated properties
    QMap<QString, QString> newProps;
    QMap<QString, QString> oldProps;
    if (!doProxy) {
        newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
    }

    // Parse clips
    QStringList externalProxyParams = m_documentProperties.value(QStringLiteral("externalproxyparams")).split(QLatin1Char(';'));
    for (int i = 0; i < clipList.count(); ++i) {
        const std::shared_ptr<ProjectClip> &item = clipList.at(i);
        ClipType::ProducerType t = item->clipType();
        // Only allow proxy on some clip types
        if ((t == ClipType::Video || t == ClipType::AV || t == ClipType::Unknown || t == ClipType::Image || t == ClipType::Playlist ||
             t == ClipType::SlideShow) &&
            item->isReady()) {
            if ((doProxy && !force && item->hasProxy()) ||
                (!doProxy && !item->hasProxy() && pCore->projectItemModel()->hasClip(item->AbstractProjectItem::clipId()))) {
                continue;
            }

            if (doProxy) {
                newProps.clear();
                QString path;
                if (useExternalProxy() && item->hasLimitedDuration()) {
                    if (externalProxyParams.count() >= 3) {
                        QFileInfo info(item->url());
                        QDir clipDir = info.absoluteDir();
                        if (clipDir.cd(externalProxyParams.at(0))) {
                            // Find correct file
                            QString fileName = info.fileName();
                            if (!externalProxyParams.at(1).isEmpty()) {
                                fileName.prepend(externalProxyParams.at(1));
                            }
                            if (!externalProxyParams.at(2).isEmpty()) {
                                fileName = fileName.section(QLatin1Char('.'), 0, -2);
                                fileName.append(externalProxyParams.at(2));
                            }
                            if (clipDir.exists(fileName)) {
                                path = clipDir.absoluteFilePath(fileName);
                            }
                        }
                    }
                }
                if (path.isEmpty()) {
                    path = dir.absoluteFilePath(item->hash() + (t == ClipType::Image ? QStringLiteral(".png") : extension));
                }
                newProps.insert(QStringLiteral("kdenlive:proxy"), path);
                // We need to insert empty proxy so that undo will work
                // TODO: how to handle clip properties
                // oldProps = clip->currentProperties(newProps);
                oldProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
            } else {
                if (t == ClipType::SlideShow) {
                    // Revert to picture aspect ratio
                    newProps.insert(QStringLiteral("aspect_ratio"), QStringLiteral("1"));
                }
                // Reset to original url
                newProps.insert(QStringLiteral("resource"), item->url());
            }
            new EditClipCommand(pCore->bin(), item->AbstractProjectItem::clipId(), oldProps, newProps, true, masterCommand);
        } else {
            // Cannot proxy this clip type
            pCore->bin()->doDisplayMessage(i18n("Clip type does not support proxies"), KMessageWidget::Information);
        }
    }
    if (!hasParent) {
        if (masterCommand->childCount() > 0) {
            m_commandStack->push(masterCommand);
        } else {
            delete masterCommand;
        }
    }
}

QMap<QString, QString> KdenliveDoc::documentProperties()
{
    m_documentProperties.insert(QStringLiteral("version"), QString::number(DOCUMENTVERSION));
    m_documentProperties.insert(QStringLiteral("kdenliveversion"), QStringLiteral(KDENLIVE_VERSION));
    if (!m_projectFolder.isEmpty()) {
        m_documentProperties.insert(QStringLiteral("storagefolder"),
                                    m_projectFolder + QLatin1Char('/') + m_documentProperties.value(QStringLiteral("documentid")));
    }
    m_documentProperties.insert(QStringLiteral("profile"), pCore->getCurrentProfile()->path());
    if (m_documentProperties.contains(QStringLiteral("decimalPoint"))) {
        // "kdenlive:docproperties.decimalPoint" was removed in document version 100
        m_documentProperties.remove(QStringLiteral("decimalPoint"));
    }
    return m_documentProperties;
}

void KdenliveDoc::loadDocumentGuides()
{
    QString guides = m_documentProperties.value(QStringLiteral("guides"));
    if (!guides.isEmpty()) {
        m_guideModel->importFromJson(guides, true);
    }
}

void KdenliveDoc::loadDocumentProperties()
{
    QDomNodeList list = m_document.elementsByTagName(QStringLiteral("playlist"));
    QDomElement baseElement = m_document.documentElement();
    m_documentRoot = baseElement.attribute(QStringLiteral("root"));
    if (!m_documentRoot.isEmpty()) {
        m_documentRoot = QDir::cleanPath(m_documentRoot) + QDir::separator();
    }
    if (!list.isEmpty()) {
        QDomElement pl = list.at(0).toElement();
        if (pl.isNull()) {
            return;
        }
        QDomNodeList props = pl.elementsByTagName(QStringLiteral("property"));
        QString name;
        QDomElement e;
        for (int i = 0; i < props.count(); i++) {
            e = props.at(i).toElement();
            name = e.attribute(QStringLiteral("name"));
            if (name.startsWith(QLatin1String("kdenlive:docproperties."))) {
                name = name.section(QLatin1Char('.'), 1);
                if (name == QStringLiteral("storagefolder")) {
                    // Make sure we have an absolute path
                    QString value = e.firstChild().nodeValue();
                    if (QFileInfo(value).isRelative()) {
                        value.prepend(m_documentRoot);
                    }
                    m_documentProperties.insert(name, value);
                } else {
                    m_documentProperties.insert(name, e.firstChild().nodeValue());
                }
            } else if (name.startsWith(QLatin1String("kdenlive:docmetadata."))) {
                name = name.section(QLatin1Char('.'), 1);
                m_documentMetadata.insert(name, e.firstChild().nodeValue());
            }
        }
    }
    QString path = m_documentProperties.value(QStringLiteral("storagefolder"));
    if (!path.isEmpty()) {
        QDir dir(path);
        dir.cdUp();
        m_projectFolder = dir.absolutePath();
        bool ok;
        // Ensure document storage folder is writable
        QString documentId = QDir::cleanPath(getDocumentProperty(QStringLiteral("documentid")));
        documentId.toLongLong(&ok, 10);
        if (ok) {
            if (!dir.exists(documentId)) {
                if (!dir.mkpath(documentId)) {
                    // Invalid storage folder, reset to default
                    m_projectFolder.clear();
                }
            }
        } else {
            // Something is wrong, documentid not readable
            qDebug()<<"=========\n\nCannot read document id: "<<documentId<<"\n\n==========";
        }
    }

    QString profile = m_documentProperties.value(QStringLiteral("profile"));
    bool profileFound = pCore->setCurrentProfile(profile);
    if (!profileFound) {
        // try to find matching profile from MLT profile properties
        list = m_document.elementsByTagName(QStringLiteral("profile"));
        if (!list.isEmpty()) {
            std::unique_ptr<ProfileInfo> xmlProfile(new ProfileParam(list.at(0).toElement()));
            QString profilePath = ProfileRepository::get()->findMatchingProfile(xmlProfile.get());
            // Document profile does not exist, create it as custom profile
            if (profilePath.isEmpty()) {
                profilePath = ProfileRepository::get()->saveProfile(xmlProfile.get());
            }
            profileFound = pCore->setCurrentProfile(profilePath);
        }
    }
    if (!profileFound) {
        qDebug() << "ERROR, no matching profile found";
    }
    updateProjectProfile(false);
}

void KdenliveDoc::updateProjectProfile(bool reloadProducers, bool reloadThumbs)
{
    pCore->jobManager()->slotCancelJobs();
    double fps = pCore->getCurrentFps();
    double fpsChanged = m_timecode.fps() / fps;
    m_timecode.setFormat(fps);
    if (!reloadProducers) {
        return;
    }
    emit updateFps(fpsChanged);
    pCore->bin()->reloadAllProducers(reloadThumbs);
}

void KdenliveDoc::resetProfile(bool reloadThumbs)
{
    updateProjectProfile(true, reloadThumbs);
    emit docModified(true);
}

void KdenliveDoc::slotSwitchProfile(const QString &profile_path, bool reloadThumbs)
{
    // Discard all current jobs
    pCore->jobManager()->slotCancelJobs();
    pCore->setCurrentProfile(profile_path);
    updateProjectProfile(true, reloadThumbs);
    emit docModified(true);
}

void KdenliveDoc::switchProfile(std::unique_ptr<ProfileParam> &profile, const QString &id, const QDomElement &xml)
{
    Q_UNUSED(id)
    Q_UNUSED(xml)
    // Request profile update
    // Check profile fps so that we don't end up with an fps = 30.003 which would mess things up
    QString adjustMessage;
    double fps = (double)profile->frame_rate_num() / profile->frame_rate_den();
    double fps_int;
    double fps_frac = std::modf(fps, &fps_int);
    if (fps_frac < 0.4) {
        profile->m_frame_rate_num = (int)fps_int;
        profile->m_frame_rate_den = 1;
    } else {
        // Check for 23.98, 29.97, 59.94
        if (qFuzzyCompare(fps_int, 23.0)) {
            if (qFuzzyCompare(fps, 23.98)) {
                profile->m_frame_rate_num = 24000;
                profile->m_frame_rate_den = 1001;
            }
        } else if (qFuzzyCompare(fps_int, 29.0)) {
            if (qFuzzyCompare(fps, 29.97)) {
                profile->m_frame_rate_num = 30000;
                profile->m_frame_rate_den = 1001;
            }
        } else if (qFuzzyCompare(fps_int, 59.0)) {
            if (qFuzzyCompare(fps, 59.94)) {
                profile->m_frame_rate_num = 60000;
                profile->m_frame_rate_den = 1001;
            }
        } else {
            // Unknown profile fps, warn user
            adjustMessage = i18n("\nWarning: unknown non integer fps, might cause incorrect duration display.");
        }
    }
    QString matchingProfile = ProfileRepository::get()->findMatchingProfile(profile.get());
    if (matchingProfile.isEmpty() && (profile->width() % 8 != 0)) {
        // Make sure profile width is a multiple of 8, required by some parts of mlt
        profile->adjustDimensions();
        matchingProfile = ProfileRepository::get()->findMatchingProfile(profile.get());
    }
    if (!matchingProfile.isEmpty()) {
        // We found a known matching profile, switch and inform user
        profile->m_path = matchingProfile;
        profile->m_description = ProfileRepository::get()->getProfile(matchingProfile)->description();

        if (KdenliveSettings::default_profile().isEmpty()) {
            // Default project format not yet confirmed, propose
            QString currentProfileDesc = pCore->getCurrentProfile()->description();
            KMessageBox::ButtonCode answer = KMessageBox::questionYesNoCancel(
                QApplication::activeWindow(),
                i18n("Your default project profile is %1, but your clip's profile is %2.\nDo you want to change default profile for future projects?",
                     currentProfileDesc, profile->description()),
                i18n("Change default project profile"), KGuiItem(i18n("Change default to %1", profile->description())),
                KGuiItem(i18n("Keep current default %1", currentProfileDesc)), KGuiItem(i18n("Ask me later")));

            switch (answer) {
            case KMessageBox::Yes:
                // Discard all current jobs
                pCore->jobManager()->slotCancelJobs();
                KdenliveSettings::setDefault_profile(profile->path());
                pCore->setCurrentProfile(profile->path());
                updateProjectProfile(true);
                emit docModified(true);
                return;
                break;
            case KMessageBox::No:
                return;
                break;
            default:
                break;
            }
        }

        // Build actions for the info message (switch / cancel)
        QList<QAction *> list;
        const QString profilePath = profile->path();
        QAction *ac = new QAction(QIcon::fromTheme(QStringLiteral("dialog-ok")), i18n("Switch"), this);
        connect(ac, &QAction::triggered, this, [this, profilePath]() { this->slotSwitchProfile(profilePath, true); });
        QAction *ac2 = new QAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")), i18n("Cancel"), this);
        list << ac << ac2;
        pCore->displayBinMessage(i18n("Switch to clip profile %1?", profile->descriptiveString()), KMessageWidget::Information, list, false, BinMessage::BinCategory::ProfileMessage);
    } else {
        // No known profile, ask user if he wants to use clip profile anyway
        if (qFuzzyCompare((double)profile->m_frame_rate_num / profile->m_frame_rate_den, fps)) {
            adjustMessage = i18n("\nProfile fps adjusted from original %1", QString::number(fps, 'f', 4));
        }
        if (KMessageBox::warningContinueCancel(QApplication::activeWindow(),
                                               i18n("No profile found for your clip.\nCreate and switch to new profile (%1x%2, %3fps)?%4", profile->m_width,
                                                    profile->m_height, QString::number((double)profile->m_frame_rate_num / profile->m_frame_rate_den, 'f', 2),
                                                    adjustMessage)) == KMessageBox::Continue) {
            profile->m_description = QStringLiteral("%1x%2 %3fps")
                                         .arg(profile->m_width)
                                         .arg(profile->m_height)
                                         .arg(QString::number((double)profile->m_frame_rate_num / profile->m_frame_rate_den, 'f', 2));
            QString profilePath = ProfileRepository::get()->saveProfile(profile.get());
            // Discard all current jobs
            pCore->jobManager()->slotCancelJobs();
            pCore->setCurrentProfile(profilePath);
            updateProjectProfile(true);
            emit docModified(true);
        }
    }
}

void KdenliveDoc::doAddAction(const QString &name, QAction *a, const QKeySequence &shortcut)
{
    pCore->window()->actionCollection()->addAction(name, a);
    a->setShortcut(shortcut);
    pCore->window()->actionCollection()->setDefaultShortcut(a, a->shortcut());
}

QAction *KdenliveDoc::getAction(const QString &name)
{
    return pCore->window()->actionCollection()->action(name);
}

void KdenliveDoc::previewProgress(int p)
{
    emit pCore->window()->setPreviewProgress(p);
}

void KdenliveDoc::displayMessage(const QString &text, MessageType type, int timeOut)
{
    emit pCore->window()->displayMessage(text, type, timeOut);
}

void KdenliveDoc::selectPreviewProfile()
{
    // Read preview profiles and find the best match
    if (!KdenliveSettings::previewparams().isEmpty()) {
        setDocumentProperty(QStringLiteral("previewparameters"), KdenliveSettings::previewparams());
        setDocumentProperty(QStringLiteral("previewextension"), KdenliveSettings::previewextension());
        return;
    }
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "timelinepreview");
    QMap<QString, QString> values = group.entryMap();
    if (KdenliveSettings::nvencEnabled() && values.contains(QStringLiteral("x264-nvenc"))) {
        const QString bestMatch = values.value(QStringLiteral("x264-nvenc"));
        setDocumentProperty(QStringLiteral("previewparameters"), bestMatch.section(QLatin1Char(';'), 0, 0));
        setDocumentProperty(QStringLiteral("previewextension"), bestMatch.section(QLatin1Char(';'), 1, 1));
        return;
    }
    if (KdenliveSettings::vaapiEnabled() && values.contains(QStringLiteral("x264-vaapi"))) {
        const QString bestMatch = values.value(QStringLiteral("x264-vaapi"));
        setDocumentProperty(QStringLiteral("previewparameters"), bestMatch.section(QLatin1Char(';'), 0, 0));
        setDocumentProperty(QStringLiteral("previewextension"), bestMatch.section(QLatin1Char(';'), 1, 1));
        return;
    }
    QMapIterator<QString, QString> i(values);
    QStringList matchingProfiles;
    QStringList fallBackProfiles;
    QSize pSize = pCore->getCurrentFrameDisplaySize();
    QString profileSize = QStringLiteral("%1x%2").arg(pSize.width()).arg(pSize.height());

    while (i.hasNext()) {
        i.next();
        // Check for frame rate
        QString params = i.value();
        QStringList data = i.value().split(QLatin1Char(' '));
        // Check for size mismatch
        if (params.contains(QStringLiteral("s="))) {
            QString paramSize = params.section(QStringLiteral("s="), 1).section(QLatin1Char(' '), 0, 0);
            if (paramSize != profileSize) {
                continue;
            }
        }
        bool rateFound = false;
        for (const QString &arg : qAsConst(data)) {
            if (arg.startsWith(QStringLiteral("r="))) {
                rateFound = true;
                double fps = arg.section(QLatin1Char('='), 1).toDouble();
                if (fps > 0) {
                    if (qAbs((int)(pCore->getCurrentFps() * 100) - (fps * 100)) <= 1) {
                        matchingProfiles << i.value();
                        break;
                    }
                }
            }
        }
        if (!rateFound) {
            // Profile without fps, can be used as fallBack
            fallBackProfiles << i.value();
        }
    }
    QString bestMatch;
    if (!matchingProfiles.isEmpty()) {
        bestMatch = matchingProfiles.first();
    } else if (!fallBackProfiles.isEmpty()) {
        bestMatch = fallBackProfiles.first();
    }
    if (!bestMatch.isEmpty()) {
        setDocumentProperty(QStringLiteral("previewparameters"), bestMatch.section(QLatin1Char(';'), 0, 0));
        setDocumentProperty(QStringLiteral("previewextension"), bestMatch.section(QLatin1Char(';'), 1, 1));
    } else {
        setDocumentProperty(QStringLiteral("previewparameters"), QString());
        setDocumentProperty(QStringLiteral("previewextension"), QString());
    }
}

QString KdenliveDoc::getAutoProxyProfile()
{
    if (m_proxyExtension.isEmpty() || m_proxyParams.isEmpty()) {
        initProxySettings();
    }
    return m_proxyParams;
}

void KdenliveDoc::initProxySettings()
{
    // Read preview profiles and find the best match
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "proxy");
    QString params;
    QMap<QString, QString> values = group.entryMap();
    // Select best proxy profile depending on hw encoder support
    if (KdenliveSettings::nvencEnabled() && values.contains(QStringLiteral("x264-nvenc"))) {
        params = values.value(QStringLiteral("x264-nvenc"));
    } else if (KdenliveSettings::vaapiEnabled() && values.contains(QStringLiteral("x264-vaapi"))) {
        params = values.value(QStringLiteral("x264-vaapi"));
    } else {
        params = values.value(QStringLiteral("MJPEG"));
    }
    m_proxyParams = params.section(QLatin1Char(';'), 0, 0);
    m_proxyExtension = params.section(QLatin1Char(';'), 1);
}

void KdenliveDoc::checkPreviewStack(int ix)
{
    // A command was pushed in the middle of the stack, remove all cached data from last undos
    emit removeInvalidUndo(ix);
}

void KdenliveDoc::saveMltPlaylist(const QString &fileName)
{
    Q_UNUSED(fileName)
    // TODO REFAC
    // m_render->preparePreviewRendering(fileName);
}

void KdenliveDoc::initCacheDirs()
{
    bool ok = false;
    QString kdenliveCacheDir;
    QString documentId = QDir::cleanPath(getDocumentProperty(QStringLiteral("documentid")));
    documentId.toLongLong(&ok, 10);
    if (m_projectFolder.isEmpty()) {
        kdenliveCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    } else {
        kdenliveCacheDir = m_projectFolder;
    }
    if (!ok || documentId.isEmpty() || kdenliveCacheDir.isEmpty()) {
        return;
    }
    QString basePath = kdenliveCacheDir + QLatin1Char('/') + documentId;
    QDir dir(basePath);
    dir.mkpath(QStringLiteral("."));
    dir.mkdir(QStringLiteral("preview"));
    dir.mkdir(QStringLiteral("audiothumbs"));
    dir.mkdir(QStringLiteral("videothumbs"));
    QDir cacheDir(kdenliveCacheDir);
    cacheDir.mkdir(QStringLiteral("proxy"));
}

QDir KdenliveDoc::getCacheDir(CacheType type, bool *ok) const
{
    QString basePath;
    QString kdenliveCacheDir;
    QString documentId = QDir::cleanPath(getDocumentProperty(QStringLiteral("documentid")));
    documentId.toLongLong(ok, 10);
    if (m_projectFolder.isEmpty()) {
        kdenliveCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!*ok || documentId.isEmpty() || kdenliveCacheDir.isEmpty()) {
            *ok = false;
            return QDir(kdenliveCacheDir);
        }
    } else {
        // Use specified folder to store all files
        kdenliveCacheDir = m_projectFolder;
    }
    basePath = kdenliveCacheDir + QLatin1Char('/') + documentId;
    switch (type) {
    case SystemCacheRoot:
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    case CacheRoot:
        basePath = kdenliveCacheDir;
        break;
    case CachePreview:
        basePath.append(QStringLiteral("/preview"));
        break;
    case CacheProxy:
        basePath = kdenliveCacheDir;
        basePath.append(QStringLiteral("/proxy"));
        break;
    case CacheAudio:
        basePath.append(QStringLiteral("/audiothumbs"));
        break;
    case CacheThumbs:
        basePath.append(QStringLiteral("/videothumbs"));
        break;
    default:
        break;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        *ok = false;
    }
    return dir;
}

QStringList KdenliveDoc::getProxyHashList()
{
    return pCore->bin()->getProxyHashList();
}

std::shared_ptr<MarkerListModel> KdenliveDoc::getGuideModel() const
{
    return m_guideModel;
}

void KdenliveDoc::guidesChanged()
{
    m_documentProperties[QStringLiteral("guides")] = m_guideModel->toJson();
}

void KdenliveDoc::groupsChanged(const QString &groups)
{
    m_documentProperties[QStringLiteral("groups")] = groups;
}

const QString KdenliveDoc::documentRoot() const
{
    return m_documentRoot;
}

bool KdenliveDoc::updatePreviewSettings(const QString &profile)
{
    if (profile.isEmpty()) {
        return false;
    }
    QString params = profile.section(QLatin1Char(';'), 0, 0);
    QString ext = profile.section(QLatin1Char(';'), 1, 1);
    if (params != getDocumentProperty(QStringLiteral("previewparameters")) || ext != getDocumentProperty(QStringLiteral("previewextension"))) {
        // Timeline preview params changed, delete all existing previews.
        setDocumentProperty(QStringLiteral("previewparameters"), params);
        setDocumentProperty(QStringLiteral("previewextension"), ext);
        return true;
    }
    return false;
}

QMap <QString, QString> KdenliveDoc::getProjectTags()
{
    QMap <QString, QString> tags;
    for (int i = 1 ; i< 20; i++) {
        QString current = getDocumentProperty(QString("tag%1").arg(i));
        if (current.isEmpty()) {
            break;
        } else {
            tags.insert(current.section(QLatin1Char(':'), 0, 0), current.section(QLatin1Char(':'), 1));
        }
    }
    if (tags.isEmpty()) {
        tags.insert(QStringLiteral("#ff0000"), i18n("Red"));
        tags.insert(QStringLiteral("#00ff00"), i18n("Green"));
        tags.insert(QStringLiteral("#0000ff"), i18n("Blue"));
        tags.insert(QStringLiteral("#ffff00"), i18n("Yellow"));
        tags.insert(QStringLiteral("#00ffff"), i18n("Cyan"));
    }
    return tags;
}

int KdenliveDoc::audioChannels() const
{
    return getDocumentProperty(QStringLiteral("audioChannels"), QStringLiteral("2")).toInt();
}

QString& KdenliveDoc::modifiedDecimalPoint() {
    return m_modifiedDecimalPoint;
}
