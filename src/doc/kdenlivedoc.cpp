/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kdenlivedoc.h"
#include "bin/bin.h"
#include "bin/bincommands.h"
#include "bin/binplaylist.hpp"
#include "bin/clipcreator.hpp"
#include "bin/mediabrowser.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markersortmodel.h"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "documentchecker.h"
#include "documentvalidator.h"
#include "docundostack.hpp"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "mltcontroller/clipcontroller.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "timeline2/model/builders/meltBuilder.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "titler/titlewidget.h"
#include "transitions/transitionsrepository.hpp"
#include <config-kdenlive.h>

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KBookmark>
#include <KBookmarkManager>
#include <KIO/CopyJob>
#include <KIO/FileCopyJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>

#include "kdenlive_debug.h"
#include <QCryptographicHash>
#include <QDomImplementation>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUndoGroup>
#include <QUndoStack>
#include <memory>
#include <mlt++/Mlt.h>

#include <audio/audioInfo.h>
#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

// The document version is the Kdenlive project file version. Only increment this on major releases if
// the file format changes and requires manual processing in the document validator.
// Increasing the document version means that older Kdenlive versions won't be able to open the project files
const double DOCUMENTVERSION = 1.1;

// The index for all timeline objects
int KdenliveDoc::next_id = 0;

// create a new blank document
KdenliveDoc::KdenliveDoc(QString projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QMap<QString, QString> &properties,
                         const QMap<QString, QString> &metadata, const std::pair<int, int> &tracks, int audioChannels, MainWindow *parent)
    : QObject(parent)
    , m_autosave(nullptr)
    , m_uuid(QUuid::createUuid())
    , m_clipsCount(0)
    , m_commandStack(std::make_shared<DocUndoStack>(undoGroup))
    , m_modified(false)
    , m_documentOpenStatus(CleanProject)
    , m_url(QUrl())
    , m_projectFolder(std::move(projectFolder))
{
    next_id = 0;
    if (parent) {
        connect(this, &KdenliveDoc::updateCompositionMode, parent, &MainWindow::slotUpdateCompositeAction);
    }
    connect(m_commandStack.get(), &QUndoStack::indexChanged, this, &KdenliveDoc::slotModified);
    connect(m_commandStack.get(), &DocUndoStack::invalidate, this, &KdenliveDoc::checkPreviewStack, Qt::DirectConnection);
    // connect(m_commandStack, SIGNAL(cleanChanged(bool)), this, SLOT(setModified(bool)));
    pCore->taskManager.unBlock();
    initializeProperties(true, tracks, audioChannels);

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
    pCore->setCurrentProfile(profileName);
    m_document = createEmptyDocument(tracks.first, tracks.second);
    updateProjectProfile(false);
    updateProjectFolderPlacesEntry();
    initCacheDirs();
}

KdenliveDoc::KdenliveDoc(const QUrl &url, QDomDocument &newDom, QString projectFolder, QUndoGroup *undoGroup, MainWindow *parent)
    : QObject(parent)
    , m_autosave(nullptr)
    , m_uuid(QUuid::createUuid())
    , m_document(newDom)
    , m_clipsCount(0)
    , m_commandStack(std::make_shared<DocUndoStack>(undoGroup))
    , m_modified(false)
    , m_documentOpenStatus(CleanProject)
    , m_url(url)
    , m_projectFolder(std::move(projectFolder))
{
    next_id = 0;
    if (parent) {
        connect(this, &KdenliveDoc::updateCompositionMode, parent, &MainWindow::slotUpdateCompositeAction);
    }
    connect(m_commandStack.get(), &QUndoStack::indexChanged, this, &KdenliveDoc::slotModified);
    connect(m_commandStack.get(), &DocUndoStack::invalidate, this, &KdenliveDoc::checkPreviewStack, Qt::DirectConnection);
    pCore->taskManager.unBlock();
    initializeProperties(false);
    updateClipsCount();
}

KdenliveDoc::KdenliveDoc(std::shared_ptr<DocUndoStack> undoStack, std::pair<int, int> tracks, MainWindow *parent)
    : QObject(parent)
    , m_autosave(nullptr)
    , m_uuid(QUuid::createUuid())
    , m_clipsCount(0)
    , m_modified(false)
    , m_documentOpenStatus(CleanProject)
{
    next_id = 0;
    m_commandStack = undoStack;
    m_document = createEmptyDocument(tracks.second, tracks.first);
    initializeProperties(true, tracks, 2);
    loadDocumentProperties();
    pCore->taskManager.unBlock();
}

DocOpenResult KdenliveDoc::Open(const QUrl &url, const QString &projectFolder, QUndoGroup *undoGroup,
    bool recoverCorruption, MainWindow *parent)
{

    DocOpenResult result = DocOpenResult{};

    if (url.isEmpty() || !url.isValid()) {
        result.setError(i18n("Invalid file path"));
        return result;
    }

    qCDebug(KDENLIVE_LOG) << "// opening file " << url.toLocalFile();

    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.setError(i18n("Cannot open file %1", url.toLocalFile()));
        return result;
    }

    QDomDocument domDoc {};
    int line;
    int col;
    QString domErrorMessage;
    if (recoverCorruption) {
        // this seems to also drop valid non-BMP Unicode characters, so only do
        // it if the file is unreadable otherwise
        QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidChars);
        result.setModified(true);
    }
    bool success = domDoc.setContent(&file, false, &domErrorMessage, &line, &col);

    if (!success) {
        if (recoverCorruption) {
            // Try to recover broken file produced by Kdenlive 0.9.4
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
                success = domDoc.setContent(playlist, false, &domErrorMessage, &line, &col);
                correction++;
            }
            if (!success) {
                result.setError(i18n("Could not recover corrupted file."));
                return result;
            } else {
                qCDebug(KDENLIVE_LOG) << "Corrupted document read successfully.";
                result.setModified(true);
            }
        } else {
            result.setError(i18n("Cannot open file %1:\n%2 (line %3, col %4)",
                url.toLocalFile(), domErrorMessage, line, col));
            return result;
        }
    }
    file.close();


    qCDebug(KDENLIVE_LOG) << "// validating project file";
    DocumentValidator validator(domDoc, url);
    success = validator.isProject();
    if (!success) {
        // It is not a project file
        result.setError(i18n("File %1 is not a Kdenlive project file", url.toLocalFile()));
        return result;
    }

    auto validationResult = validator.validate(DOCUMENTVERSION);
    success = validationResult.first;
    if (!success) {
        result.setError(i18n("File %1 is not a valid Kdenlive project file.", url.toLocalFile()));
        return result;
    }

    if (!validationResult.second.isEmpty()) {
        qDebug() << "DECIMAL POINT has changed to . (was " << validationResult.second << "previously)";
        result.setModified(true);
    }

    if (!KdenliveSettings::gpu_accel()) {
        success = validator.checkMovit();
    }
    if (!success) {
        result.setError(i18n("GPU acceleration is turned off in Kdenlive settings, but is required for this project's Movit filters."));
        return result;
    }

    DocumentChecker d(url, domDoc);

    if (d.hasErrorInProject()) {
        if (pCore->window() == nullptr) {
            qInfo() << "DocumentChecker found some problems in the project:";
            for (const auto &item : d.resourceItems()) {
                qInfo() << item;
                if (item.status == DocumentChecker::MissingStatus::Missing) {
                    success = false;
                }
            }
        } else {
            success = d.resolveProblemsWithGUI();
        }
    }

    if (!success) {
        // Loading aborted
        result.setAborted();
        return result;
    }

    // create KdenliveDoc object
    auto doc = std::unique_ptr<KdenliveDoc>(new KdenliveDoc(url, domDoc, projectFolder, undoGroup, parent));
    if (!validationResult.second.isEmpty()) {
        doc->m_modifiedDecimalPoint = validationResult.second;
        //doc->setModifiedDecimalPoint(validationResult.second);
    }
    doc->loadDocumentProperties();
    if (!doc->m_projectFolder.isEmpty()) {
        // Ask to create the project directory if it does not exist
        QDir folder(doc->m_projectFolder);
        if (!folder.mkpath(QStringLiteral("."))) {
            // Project folder is not writable
            doc->m_projectFolder = doc->m_url.toString(QUrl::RemoveFilename | QUrl::RemoveScheme);
            folder.setPath(doc->m_projectFolder);
            if (folder.exists()) {
                KMessageBox::error(
                    parent,
                    i18n("The project directory %1, could not be created.\nPlease make sure you have the required permissions.\nDefaulting to system folders",
                         doc->m_projectFolder));
            } else {
                KMessageBox::information(parent, i18n("Document project folder is invalid, using system default folders"));
            }
            doc->m_projectFolder.clear();
        }
    }
    doc->initCacheDirs();

    if (doc->m_document.documentElement().hasAttribute(QStringLiteral("upgraded"))) {
        doc->m_documentOpenStatus = UpgradedProject;
        result.setUpgraded(true);
    } else if (doc->m_document.documentElement().hasAttribute(QStringLiteral("modified")) || validator.isModified()) {
        doc->m_documentOpenStatus = ModifiedProject;
        result.setModified(true);
        doc->setModified(true);
    }

    if (result.wasModified() || result.wasUpgraded()) {
        doc->requestBackup();
    }
    result.setDocument(std::move(doc));

    return result;
}

KdenliveDoc::~KdenliveDoc()
{
    if (m_url.isEmpty()) {
        // Document was never saved, delete cache folder
        QString documentId = QDir::cleanPath(m_documentProperties.value(QStringLiteral("documentid")));
        bool ok = false;
        documentId.toLongLong(&ok, 10);
        if (ok && !documentId.isEmpty()) {
            QDir baseCache = getCacheDir(CacheBase, &ok);
            if (baseCache.dirName() == documentId && baseCache.entryList(QDir::Files).isEmpty()) {
                baseCache.removeRecursively();
            }
        }
    }
    // qCDebug(KDENLIVE_LOG) << "// DEL CLP MAN";
    disconnect(this, &KdenliveDoc::docModified, pCore->window(), &MainWindow::slotUpdateDocumentState);
    m_commandStack->clear();
    m_timelines.clear();
    // qCDebug(KDENLIVE_LOG) << "// DEL CLP MAN done";
    if (m_autosave) {
        if (!m_autosave->fileName().isEmpty()) {
            m_autosave->remove();
        }
        delete m_autosave;
    }
}

void KdenliveDoc::initializeProperties(bool newDocument, std::pair<int, int> tracks, int audioChannels)
{
    // init default document properties
    m_documentProperties[QStringLiteral("enableproxy")] = QString::number(int(KdenliveSettings::enableproxy()));
    m_documentProperties[QStringLiteral("proxyparams")] = KdenliveSettings::proxyparams();
    m_documentProperties[QStringLiteral("proxyextension")] = KdenliveSettings::proxyextension();
    m_documentProperties[QStringLiteral("previewparameters")] = KdenliveSettings::previewparams();
    m_documentProperties[QStringLiteral("previewextension")] = KdenliveSettings::previewextension();
    m_documentProperties[QStringLiteral("externalproxyparams")] = KdenliveSettings::externalProxyProfile();
    m_documentProperties[QStringLiteral("enableexternalproxy")] = QString::number(int(KdenliveSettings::externalproxy()));
    m_documentProperties[QStringLiteral("generateproxy")] = QString::number(int(KdenliveSettings::generateproxy()));
    m_documentProperties[QStringLiteral("proxyminsize")] = QString::number(KdenliveSettings::proxyminsize());
    m_documentProperties[QStringLiteral("generateimageproxy")] = QString::number(int(KdenliveSettings::generateimageproxy()));
    m_documentProperties[QStringLiteral("proxyimageminsize")] = QString::number(KdenliveSettings::proxyimageminsize());
    m_documentProperties[QStringLiteral("proxyimagesize")] = QString::number(KdenliveSettings::proxyimagesize());
    m_documentProperties[QStringLiteral("proxyresize")] = QString::number(KdenliveSettings::proxyscale());
    m_documentProperties[QStringLiteral("enableTimelineZone")] = QLatin1Char('0');
    m_documentProperties[QStringLiteral("seekOffset")] = QString::number(TimelineModel::seekDuration);
    m_documentProperties[QStringLiteral("audioChannels")] = QString::number(audioChannels);
    m_documentProperties[QStringLiteral("uuid")] = m_uuid.toString();
    if (newDocument) {
        QMap<QString, QString> sequenceProperties;
        // video tracks are after audio tracks, and the UI shows them from highest position to lowest position
        sequenceProperties[QStringLiteral("videoTarget")] = QString::number(tracks.second);
        sequenceProperties[QStringLiteral("audioTarget")] = QString::number(tracks.second - 1);
        // If there is at least one video track, set activeTrack to be the first
        // video track (which comes after the audio tracks). Otherwise, set the
        // activeTrack to be the last audio track (the top-most audio track in the
        // UI).
        const int activeTrack = tracks.first > 0 ? tracks.second : tracks.second - 1;
        sequenceProperties[QStringLiteral("activeTrack")] = QString::number(activeTrack);
        sequenceProperties[QStringLiteral("documentuuid")] = m_uuid.toString();
        m_sequenceProperties.insert(m_uuid, sequenceProperties);
        // For existing documents, don't define guidesCategories, so that we can use the getDefaultGuideCategories() for backwards compatibility
        m_documentProperties[QStringLiteral("guidesCategories")] = MarkerListModel::categoriesListToJSon(KdenliveSettings::guidesCategories());
    }
}

const QStringList KdenliveDoc::guidesCategories()
{
    QStringList categories = getGuideModel(activeUuid)->guideCategoriesToStringList(m_documentProperties.value(QStringLiteral("guidesCategories")));
    if (categories.isEmpty()) {
        const QStringList defaultCategories = getDefaultGuideCategories();
        m_documentProperties[QStringLiteral("guidesCategories")] = MarkerListModel::categoriesListToJSon(defaultCategories);
        return defaultCategories;
    }
    return categories;
}

void KdenliveDoc::updateGuideCategories(const QStringList &categories, const QMap<int, int> remapCategories)
{
    const QStringList currentCategories =
        getGuideModel(activeUuid)->guideCategoriesToStringList(m_documentProperties.value(QStringLiteral("guidesCategories")));
    // Check if a guide category was removed
    QList<int> currentIndexes;
    QList<int> updatedIndexes;
    for (auto &cat : currentCategories) {
        currentIndexes << cat.section(QLatin1Char(':'), -2, -2).toInt();
    }
    for (auto &cat : categories) {
        updatedIndexes << cat.section(QLatin1Char(':'), -2, -2).toInt();
    }
    for (auto &i : updatedIndexes) {
        currentIndexes.removeAll(i);
    }
    if (!currentIndexes.isEmpty()) {
        // A marker category was removed, delete all Bin clip markers using it
        pCore->bin()->removeMarkerCategories(currentIndexes, remapCategories);
    }
    getGuideModel(activeUuid)->loadCategoriesWithUndo(categories, currentCategories, remapCategories);
}

void KdenliveDoc::saveGuideCategories()
{
    const QString categories = getGuideModel(activeUuid)->categoriesToJSon();
    m_documentProperties[QStringLiteral("guidesCategories")] = categories;
}

int KdenliveDoc::updateClipsCount()
{
    m_clipsCount = m_document.elementsByTagName(QLatin1String("entry")).size();
    return m_clipsCount;
}

int KdenliveDoc::clipsCount() const
{
    return m_clipsCount;
}

const QByteArray KdenliveDoc::getAndClearProjectXml()
{
    // Profile has already been set, dont overwrite it
    m_document.documentElement().removeChild(m_document.documentElement().firstChildElement(QLatin1String("profile")));
    const QByteArray result = m_document.toString().toUtf8();
    // We don't need the xml data anymore, throw away
    m_document.clear();
    return result;
}

QDomDocument KdenliveDoc::createEmptyDocument(int videotracks, int audiotracks, bool disableProfile)
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
    return createEmptyDocument(tracks, disableProfile);
}

QDomDocument KdenliveDoc::createEmptyDocument(const QList<TrackInfo> &tracks, bool disableProfile)
{
    // Creating new document
    QDomDocument doc;
    std::unique_ptr<Mlt::Profile> docProfile(new Mlt::Profile(pCore->getCurrentProfilePath().toUtf8().constData()));
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer xmlConsumer(*docProfile.get(), "xml:kdenlive_playlist");
    if (disableProfile) {
        xmlConsumer.set("no_profile", 1);
    }
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("store", "kdenlive");
    Mlt::Tractor tractor(*docProfile.get());
    Mlt::Producer bk(*docProfile.get(), "color:black");
    bk.set("mlt_image_format", "rgba");
    tractor.insert_track(bk, 0);
    for (int i = 0; i < tracks.count(); ++i) {
        Mlt::Tractor track(*docProfile.get());
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
        Mlt::Playlist playlist1(*docProfile.get());
        Mlt::Playlist playlist2(*docProfile.get());
        track.insert_track(playlist1, 0);
        track.insert_track(playlist2, 1);
        tractor.insert_track(track, i + 1);
    }
    QScopedPointer<Mlt::Field> field(tractor.field());
    QString compositeService = TransitionsRepository::get()->getCompositingTransition();
    if (!compositeService.isEmpty()) {
        for (int i = 0; i <= tracks.count(); i++) {
            if (i > 0 && tracks.at(i - 1).type == AudioTrack) {
                Mlt::Transition tr(*docProfile.get(), "mix");
                tr.set("a_track", 0);
                tr.set("b_track", i);
                tr.set("always_active", 1);
                tr.set("sum", 1);
                tr.set("accepts_blanks", 1);
                tr.set("internal_added", 237);
                field->plant_transition(tr, 0, i);
            }
            if (i > 0 && tracks.at(i - 1).type == VideoTrack) {
                Mlt::Transition tr(*docProfile.get(), compositeService.toUtf8().constData());
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
           (width < 0 || width > m_documentProperties.value(QStringLiteral("proxyminsize")).toInt());
}

bool KdenliveDoc::autoGenerateImageProxy(int width) const
{
    return (m_documentProperties.value(QStringLiteral("generateimageproxy")).toInt() != 0) &&
           (width < 0 || width > m_documentProperties.value(QStringLiteral("proxyimageminsize")).toInt());
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
        }
        m_autosave->flush();
    }
}

void KdenliveDoc::setZoom(const QUuid &uuid, int horizontal, int vertical)
{
    setSequenceProperty(uuid, QStringLiteral("zoom"), horizontal);
    if (vertical > -1) {
        setSequenceProperty(uuid, QStringLiteral("verticalzoom"), vertical);
    }
}

void KdenliveDoc::importSequenceProperties(const QUuid uuid, const QStringList properties)
{
    for (const auto &prop : properties) {
        if (m_documentProperties.contains(prop)) {
            setSequenceProperty(uuid, prop, m_documentProperties.value(prop));
        }
    }
    for (const auto &prop : properties) {
        m_documentProperties.remove(prop);
    }
}

QPoint KdenliveDoc::zoom(const QUuid &uuid) const
{
    return QPoint(getSequenceProperty(uuid, QStringLiteral("zoom"), QStringLiteral("8")).toInt(),
                  getSequenceProperty(uuid, QStringLiteral("verticalzoom")).toInt());
}

void KdenliveDoc::setZone(const QUuid &uuid, int start, int end)
{
    setSequenceProperty(uuid, QStringLiteral("zonein"), start);
    setSequenceProperty(uuid, QStringLiteral("zoneout"), end);
}

QPoint KdenliveDoc::zone(const QUuid &uuid) const
{
    return QPoint(getSequenceProperty(uuid, QStringLiteral("zonein")).toInt(), getSequenceProperty(uuid, QStringLiteral("zoneout")).toInt());
}

QPair<int, int> KdenliveDoc::targetTracks(const QUuid &uuid) const
{
    return {getSequenceProperty(uuid, QStringLiteral("videoTarget")).toInt(), getSequenceProperty(uuid, QStringLiteral("audioTarget")).toInt()};
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
        qDebug() << " = = = =  = =  CORRUPTED DOC\n" << scene;
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

bool KdenliveDoc::saveSceneList(const QString &path, const QString &scene, bool saveOverExistingFile)
{
    QDomDocument sceneList = xmlSceneList(scene);
    if (sceneList.isNull()) {
        // Make sure we don't save if scenelist is corrupted
        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1, scene list is corrupted.", path));
        return false;
    }

    // Backup current version
    backupLastSavedVersion(path);
    if (m_documentOpenStatus != CleanProject && saveOverExistingFile) {
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
    Q_EMIT saveTimelinePreview(backupFolder.absoluteFilePath(fileName));
    return true;
}

QString KdenliveDoc::projectTempFolder() const
{
    if (m_projectFolder.isEmpty()) {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    return m_projectFolder;
}

QString KdenliveDoc::projectDataFolder(const QString &newPath) const
{
    if (KdenliveSettings::videotodefaultfolder() == 2 && !KdenliveSettings::videofolder().isEmpty()) {
        return KdenliveSettings::videofolder();
    }
    if (!newPath.isEmpty() && (KdenliveSettings::videotodefaultfolder() == 1 || m_sameProjectFolder)) {
        // If the project is being moved, and we use the location of the project file, return the new path
        return newPath;
    }
    if (m_projectFolder.isEmpty()) {
        // Project has not been saved yet
        if (KdenliveSettings::customprojectfolder()) {
            return KdenliveSettings::defaultprojectfolder();
        }
        return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    }
    if (KdenliveSettings::videotodefaultfolder() == 1 || m_sameProjectFolder) {
        // Always render to project folder
        if (KdenliveSettings::customprojectfolder() && !m_sameProjectFolder) {
            return KdenliveSettings::defaultprojectfolder();
        }
        return QFileInfo(m_url.toLocalFile()).absolutePath();
    }
    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
}

QString KdenliveDoc::projectCaptureFolder() const
{
    if (KdenliveSettings::capturetoprojectfolder() == 2 && !KdenliveSettings::capturefolder().isEmpty()) {
        return KdenliveSettings::capturefolder();
    }
    if (KdenliveSettings::capturetoprojectfolder() == 1 || m_sameProjectFolder || KdenliveSettings::capturetoprojectfolder() == 3) {
        QString projectFolder = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

        if (m_projectFolder.isEmpty()) {
            // Project has not been saved yet
            if (KdenliveSettings::customprojectfolder()) {
                projectFolder = KdenliveSettings::defaultprojectfolder();
            }
        } else if (KdenliveSettings::customprojectfolder() && !m_sameProjectFolder) {
            projectFolder = KdenliveSettings::defaultprojectfolder();
        } else {
            projectFolder = QFileInfo(m_url.toLocalFile()).absolutePath();
        }

        if (KdenliveSettings::capturetoprojectfolder() == 3 && !KdenliveSettings::captureprojectsubfolder().isEmpty()) {
            // Wherever the project file is, we want a subfolder
            projectFolder += QDir::separator() + KdenliveSettings::captureprojectsubfolder();
        }

        return projectFolder;
    }

    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
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

const QList<QUrl> KdenliveDoc::getProjectData(bool *ok)
{
    // Move proxies
    QList<QUrl> cacheUrls;
    auto binClips = pCore->projectItemModel()->getAllClipIds();
    QDir proxyFolder = getCacheDir(CacheProxy, ok);
    if (!*ok) {
        qWarning() << "Cannot write to cache folder: " << proxyFolder.absolutePath();
        return cacheUrls;
    }
    // First step: all clips referenced by the bin model exist and are inserted
    for (const auto &binClip : binClips) {
        auto projClip = pCore->projectItemModel()->getClipByBinID(binClip);
        QString proxy = projClip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
        QFileInfo p(proxy);
        if (proxy.length() > 2 && p.exists() && p.absoluteDir() == proxyFolder) {
            // Only move proxy clips that are inside our own proxy folder, not external ones.
            QUrl pUrl = QUrl::fromLocalFile(proxy);
            if (!cacheUrls.contains(pUrl)) {
                cacheUrls << pUrl;
            }
        }
    }
    *ok = true;
    return cacheUrls;
}

void KdenliveDoc::slotMoveFinished(KJob *job)
{
    if (job->error() != 0) {
        KMessageBox::error(pCore->window(), i18n("Error moving project folder: %1", job->errorText()));
    }
}

bool KdenliveDoc::profileChanged(const QString &profile) const
{
    return !(*pCore->getCurrentProfile().get() == *ProfileRepository::get()->getProfile(profile).get());
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

QStringList KdenliveDoc::getAllSubtitlesPath(bool final)
{
    QStringList result;
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    while (j.hasNext()) {
        j.next();
        if (j.value()->hasSubtitleModel()) {
            QMap<std::pair<int, QString>, QString> allSubFiles = j.value()->getSubtitleModel()->getSubtitlesList();
            QMapIterator<std::pair<int, QString>, QString> k(allSubFiles);
            while (k.hasNext()) {
                k.next();
                result << subTitlePath(j.value()->uuid(), k.key().first, final);
            }
        }
    }
    return result;
}

void KdenliveDoc::prepareRenderAssets(const QDir &destFolder)
{
    // Copy current subtitles to assets render folder
    updateWorkFilesBeforeSave(destFolder.absoluteFilePath(m_url.fileName()), true);
}

void KdenliveDoc::restoreRenderAssets()
{
    // Copy current subtitles to assets render folder
    updateWorkFilesAfterSave();
}

void KdenliveDoc::updateWorkFilesBeforeSave(const QString &newUrl, bool onRender)
{
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    bool checkOverwrite = QUrl::fromLocalFile(newUrl) != m_url;
    while (j.hasNext()) {
        j.next();
        if (j.value()->hasSubtitleModel()) {
            // Calculate the new path for each subtitle in this timeline
            QString basePath = newUrl;
            if (j.value()->uuid() != m_uuid) {
                basePath.append(j.value()->uuid().toString());
            }
            QMap<std::pair<int, QString>, QString> allSubs = j.value()->getSubtitleModel()->getSubtitlesList();
            QMapIterator<std::pair<int, QString>, QString> i(allSubs);
            while (i.hasNext()) {
                i.next();
                QString finalName = basePath;
                if (i.key().first > 0) {
                    finalName.append(QStringLiteral("-%1").arg(i.key().first));
                }
                QFileInfo info(finalName);
                QString subPath = info.dir().absoluteFilePath(QString("%1.srt").arg(info.fileName()));
                j.value()->getSubtitleModel()->copySubtitle(subPath, i.key().first, checkOverwrite, true);
            }
        }
    }
    QDir sequenceFolder;
    if (onRender) {
        sequenceFolder = QFileInfo(newUrl).dir();
        sequenceFolder.mkpath(QFileInfo(newUrl).baseName());
        sequenceFolder.cd(QFileInfo(newUrl).baseName());
    } else {
        bool ok;
        sequenceFolder = getCacheDir(CacheSequence, &ok);
        if (!ok) {
            // Warning, could not access project folder...
            qWarning() << "Cannot write to cache folder: " << sequenceFolder.absolutePath();
        }
    }
    pCore->bin()->moveTimeWarpToFolder(sequenceFolder, true);
}

void KdenliveDoc::updateWorkFilesAfterSave()
{
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    while (j.hasNext()) {
        j.next();
        if (j.value()->hasSubtitleModel()) {
            int ix = getSequenceProperty(j.value()->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"), QStringLiteral("0")).toInt();
            j.value()->getSubtitleModel()->restoreTmpFile(ix);
        }
    }

    bool ok;
    QDir sequenceFolder = getCacheDir(CacheTmpWorkFiles, &ok);
    pCore->bin()->moveTimeWarpToFolder(sequenceFolder, false);
}

void KdenliveDoc::slotModified()
{
    setModified(!m_commandStack->isClean());
}

void KdenliveDoc::setModified(bool mod)
{
    // fix mantis#3160: The document may have an empty URL if not saved yet, but should have a m_autosave in any case
    if ((m_autosave != nullptr) && mod && KdenliveSettings::crashrecovery()) {
        Q_EMIT startAutoSave();
    }
    // TODO: this is not working in case of undo/redo
    m_sequenceThumbsNeedsRefresh.insert(pCore->currentTimelineId());

    if (mod == m_modified) {
        return;
    }
    m_modified = mod;
    Q_EMIT docModified(m_modified);
}

bool KdenliveDoc::sequenceThumbRequiresRefresh(const QUuid &uuid) const
{
    return m_sequenceThumbsNeedsRefresh.contains(uuid);
}

void KdenliveDoc::setSequenceThumbRequiresUpdate(const QUuid &uuid)
{
    m_sequenceThumbsNeedsRefresh.insert(uuid);
}

void KdenliveDoc::sequenceThumbUpdated(const QUuid &uuid)
{
    m_sequenceThumbsNeedsRefresh.remove(uuid);
}

bool KdenliveDoc::isModified() const
{
    return m_modified;
}

void KdenliveDoc::requestBackup()
{
    m_document.documentElement().setAttribute(QStringLiteral("modified"), 1);
}

const QString KdenliveDoc::description(const QString suffix) const
{
    QString fullName = suffix;
    if (!fullName.isEmpty()) {
        fullName.append(QLatin1Char(':'));
    }
    if (!m_url.isValid()) {
        fullName.append(i18n("Untitled"));
    } else {
        fullName.append(QFileInfo(m_url.toLocalFile()).completeBaseName());
    }
    fullName.append(QStringLiteral(" [*]/ ") + pCore->getCurrentProfile()->description());
    return fullName;
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
        QPointer<QFileDialog> d = new QFileDialog(QApplication::activeWindow(), i18nc("@title:window", "Enter Template Path"), titlesFolder);
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
    Q_EMIT selectLastAddedClip(id);
}

void KdenliveDoc::cacheImage(const QString &fileId, const QImage &img) const
{
    bool ok;
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

bool KdenliveDoc::hasDocumentProperty(const QString &name) const
{
    return m_documentProperties.contains(name);
}

void KdenliveDoc::setSequenceProperty(const QUuid &uuid, const QString &name, const QString &value)
{
    if (m_sequenceProperties.contains(uuid)) {
        if (value.isEmpty()) {
            m_sequenceProperties[uuid].remove(name);
        } else {
            m_sequenceProperties[uuid].insert(name, value);
        }
    } else if (!value.isEmpty()) {
        QMap<QString, QString> sequenceMap;
        sequenceMap.insert(name, value);
        m_sequenceProperties.insert(uuid, sequenceMap);
    }
}

void KdenliveDoc::setSequenceProperty(const QUuid &uuid, const QString &name, int value)
{
    setSequenceProperty(uuid, name, QString::number(value));
}

const QString KdenliveDoc::getSequenceProperty(const QUuid &uuid, const QString &name, const QString defaultValue) const
{
    if (m_sequenceProperties.contains(uuid)) {
        const QMap<QString, QString> sequenceMap = m_sequenceProperties.value(uuid);
        const QString result = sequenceMap.value(name, defaultValue);
        return result;
    }
    return defaultValue;
}

const QStringList KdenliveDoc::getSequenceNames() const
{
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> m(m_timelines);
    QStringList sequenceNames;
    while (m.hasNext()) {
        m.next();
        sequenceNames << QString(m.value()->tractor()->get("kdenlive:clipname"));
    }
    return sequenceNames;
}

bool KdenliveDoc::hasSequenceProperty(const QUuid &uuid, const QString &name) const
{
    if (m_sequenceProperties.contains(uuid)) {
        if (m_sequenceProperties.value(uuid).contains(name)) {
            return true;
        }
    }
    return false;
}

void KdenliveDoc::clearSequenceProperty(const QUuid &uuid, const QString &name)
{
    if (m_sequenceProperties.contains(uuid)) {
        m_sequenceProperties[uuid].remove(name);
    }
}

const QMap<QString, QString> KdenliveDoc::getSequenceProperties(const QUuid &uuid) const
{
    if (m_sequenceProperties.contains(uuid)) {
        QMap<QString, QString> seqProps = m_sequenceProperties.value(uuid);
        if (pCore->window()) {
            // Include timeline controller properties (zone, position)
            pCore->window()->getSequenceProperties(uuid, seqProps);
        }
        return seqProps;
    }
    return QMap<QString, QString>();
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                        out.setCodec("UTF-8");
#endif
                        out << doc.toString();
                    } else {
                        KMessageBox::error(QApplication::activeWindow(), i18n("Cannot write to file %1", file.fileName()));
                    }
                }
            }
        }
    }
    if (!importedEffects.isEmpty()) {
        KMessageBox::informationList(QApplication::activeWindow(), i18n("The following effects were imported from the project:"), importedEffects);
    }
    if (!importedEffects.isEmpty()) {
        Q_EMIT reloadEffects(newPaths);
    }
}

void KdenliveDoc::updateProjectFolderPlacesEntry()
{
    /*
     * For similar and more code have a look at kfileplacesmodel.cpp and the included files:
     * https://api.kde.org/frameworks/kio/html/kfileplacesmodel_8cpp_source.html
     */

    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/user-places.xbel");
#if QT_VERSION_MAJOR < 6
    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForExternalFile(file);
#else
    std::unique_ptr<KBookmarkManager> bookmarkManager = std::make_unique<KBookmarkManager>(file);
#endif
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
    QDomDocument doc;
    if (!Xml::docContentFromFile(doc, path, false)) {
        return 0;
    }
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
        // backup subitle file in case we have one
        // TODO: this only backups one subtitle file, and the saved one, not the tmp worked on file
        QString subpath(path + QStringLiteral(".srt"));
        QString subbackupFile(backupFile + QStringLiteral(".srt"));
        if (QFile(subpath).exists()) {
            QFile::remove(subbackupFile);
            if (!QFile::copy(subpath, subbackupFile)) {
                KMessageBox::information(QApplication::activeWindow(), i18n("Cannot create backup copy:\n%1", subbackupFile));
            }
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
        QFile::remove(f + QStringLiteral(".srt"));
    }
    while (dayList.count() > 0) {
        f = dayList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
        QFile::remove(f + QStringLiteral(".srt"));
    }
    while (weekList.count() > 0) {
        f = weekList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
        QFile::remove(f + QStringLiteral(".srt"));
    }
    while (oldList.count() > 0) {
        f = oldList.takeFirst();
        QFile::remove(f);
        QFile::remove(f + QStringLiteral(".png"));
        QFile::remove(f + QStringLiteral(".srt"));
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

QMap<QString, QString> KdenliveDoc::proxyClipsById(const QStringList &ids, bool proxy, const QMap<QString, QString> &proxyPath)
{
    QMap<QString, QString> existingProxies;
    for (auto &id : ids) {
        auto clip = pCore->projectItemModel()->getClipByBinID(id);
        QMap<QString, QString> newProps;
        if (!proxy) {
            newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
            existingProxies.insert(id, clip->getProducerProperty(QStringLiteral("kdenlive:proxy")));
        } else if (proxyPath.contains(id)) {
            newProps.insert(QStringLiteral("kdenlive:proxy"), proxyPath.value(id));
        }
        clip->setProperties(newProps);
    }
    return existingProxies;
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
    bool ok;
    QDir dir = getCacheDir(CacheProxy, &ok);
    if (!ok) {
        // Error
        qDebug() << "::::: CANNOT GET CACHE DIR!!!!";
        return;
    }
    QString extension = getDocumentProperty(QStringLiteral("proxyextension"));
    if (extension.isEmpty()) {
        if (m_proxyExtension.isEmpty()) {
            initProxySettings();
        }
        extension = m_proxyExtension;
    }
    extension.prepend(QLatin1Char('.'));

    // Prepare updated properties
    QMap<QString, QString> newProps;
    QMap<QString, QString> oldProps;
    if (!doProxy) {
        newProps.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
    }

    // Parse clips
    for (int i = 0; i < clipList.count(); ++i) {
        const std::shared_ptr<ProjectClip> &item = clipList.at(i);
        ClipType::ProducerType t = item->clipType();
        // Only allow proxy on some clip types
        if ((t == ClipType::Video || t == ClipType::AV || t == ClipType::Unknown || t == ClipType::Image || t == ClipType::Playlist ||
             t == ClipType::SlideShow) &&
            item->statusReady()) {
            // Check for MP3 with cover art
            if (t == ClipType::AV && item->codec(false) == QLatin1String("mjpeg")) {
                QString frame_rate = item->videoCodecProperty(QStringLiteral("frame_rate"));
                if (frame_rate.isEmpty()) {
                    frame_rate = item->getProducerProperty(QLatin1String("meta.media.frame_rate_num"));
                }
                if (frame_rate == QLatin1String("90000")) {
                    pCore->bin()->doDisplayMessage(i18n("Clip type does not support proxies"), KMessageWidget::Information);
                    continue;
                }
            }
            if ((doProxy && !force && item->hasProxy()) ||
                (!doProxy && !item->hasProxy() && pCore->projectItemModel()->hasClip(item->AbstractProjectItem::clipId()))) {
                continue;
            }

            if (doProxy) {
                newProps.clear();
                QString path;
                if (useExternalProxy() && item->hasLimitedDuration()) {
                    if (item->hasProducerProperty(QStringLiteral("kdenlive:camcorderproxy"))) {
                        const QString camProxy = item->getProducerProperty(QStringLiteral("kdenlive:camcorderproxy"));
                        extension = QFileInfo(camProxy).suffix();
                        extension.prepend(QLatin1Char('.'));
                    } else {
                        path = item->getProxyFromOriginal(item->url());
                        if (!path.isEmpty()) {
                            // Check if source and proxy have the same audio streams count
                            int sourceAudioStreams = item->audioStreamsCount();
                            std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(pCore->getProjectProfile(), "avformat", path.toUtf8().constData()));
                            prod->set("video_index", -1);
                            prod->probe();
                            auto info = std::make_unique<AudioInfo>(prod);
                            int proxyAudioStreams = info->size();
                            prod.reset();
                            if (proxyAudioStreams != sourceAudioStreams) {
                                // Build a proxy with correct audio streams
                                newProps.insert(QStringLiteral("kdenlive:camcorderproxy"), path);
                                extension = QFileInfo(path).suffix();
                                extension.prepend(QLatin1Char('.'));
                                path.clear();
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
            if (doProxy) {
                pCore->bin()->doDisplayMessage(i18n("Clip type does not support proxies"), KMessageWidget::Information);
            }
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

double KdenliveDoc::getDocumentVersion() const
{
    return DOCUMENTVERSION;
}

QMap<QString, QString> KdenliveDoc::documentProperties(bool saveHash)
{
    m_documentProperties.insert(QStringLiteral("version"), QString::number(DOCUMENTVERSION));
    m_documentProperties.insert(QStringLiteral("kdenliveversion"), QStringLiteral(KDENLIVE_VERSION));
    if (!m_projectFolder.isEmpty()) {
        QDir folder(m_projectFolder);
        m_documentProperties.insert(QStringLiteral("storagefolder"), folder.absoluteFilePath(m_documentProperties.value(QStringLiteral("documentid"))));
    }
    m_documentProperties.insert(QStringLiteral("profile"), pCore->getCurrentProfile()->path());
    if (m_documentProperties.contains(QStringLiteral("decimalPoint"))) {
        // "kdenlive:docproperties.decimalPoint" was removed in document version 100
        m_documentProperties.remove(QStringLiteral("decimalPoint"));
    }
    if (pCore->mediaBrowser()) {
        m_documentProperties.insert(QStringLiteral("browserurl"), pCore->mediaBrowser()->url().toLocalFile());
    }
    m_documentProperties.insert(QStringLiteral("binsort"), QString::number(KdenliveSettings::binSorting()));
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    while (j.hasNext()) {
        j.next();
        j.value()->passSequenceProperties(getSequenceProperties(j.key()));
        if (saveHash) {
            j.value()->tractor()->set("kdenlive:sequenceproperties.timelineHash", j.value()->timelineHash().toHex().constData());
        }
    }
    return m_documentProperties;
}

void KdenliveDoc::loadDocumentGuides(const QUuid &uuid, std::shared_ptr<TimelineItemModel> model)
{
    const QString guides = getSequenceProperty(uuid, QStringLiteral("guides"));
    if (!guides.isEmpty()) {
        model->getGuideModel()->importFromJson(guides, true, false);
        clearSequenceProperty(uuid, QStringLiteral("guides"));
    }
}

void KdenliveDoc::loadDocumentProperties()
{
    QDomNodeList list = m_document.elementsByTagName(QStringLiteral("playlist"));
    QDomElement baseElement = m_document.documentElement();
    m_documentRoot = baseElement.attribute(QStringLiteral("root"));
    if (!m_documentRoot.isEmpty()) {
        m_documentRoot = QDir::cleanPath(m_documentRoot) + QLatin1Char('/');
    }
    QDomElement pl;
    for (int i = 0; i < list.count(); i++) {
        pl = list.at(i).toElement();
        const QString id = pl.attribute(QStringLiteral("id"));
        if (id == QLatin1String("main_bin") || id == QLatin1String("main bin")) {
            break;
        }
        pl = QDomElement();
    }
    if (pl.isNull()) {
        qDebug() << "==== DOCUMENT PLAYLIST NOT FOUND!!!!!";
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
                if (name == QLatin1String("uuid")) {
                    m_uuid = QUuid(e.firstChild().nodeValue());
                } else if (name == QLatin1String("timelines")) {
                    qDebug() << "=======\n\nFOUND EXTRA TIMELINES:\n\n" << e.firstChild().nodeValue() << "\n\n=========";
                }
            }
        } else if (name.startsWith(QLatin1String("kdenlive:docmetadata."))) {
            name = name.section(QLatin1Char('.'), 1);
            m_documentMetadata.insert(name, e.firstChild().nodeValue());
        }
    }
    QString path = m_documentProperties.value(QStringLiteral("storagefolder"));
    if (!path.isEmpty()) {
        QDir dir(path);
        dir.cdUp();
        m_projectFolder = dir.absolutePath();
        bool ok = false;
        // Ensure document storage folder is writable
        QString documentId = QDir::cleanPath(m_documentProperties.value(QStringLiteral("documentid")));
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
            qDebug() << "=========\n\nCannot read document id: " << documentId << "\n\n==========";
        }
    }

    QString profile = m_documentProperties.value(QStringLiteral("profile"));
    bool profileFound = pCore->setCurrentProfile(profile);
    if (!profileFound) {
        // try to find matching profile from MLT profile properties
        list = m_document.elementsByTagName(QStringLiteral("profile"));
        if (!list.isEmpty()) {
            std::unique_ptr<ProfileInfo> xmlProfile(new ProfileParam(list.at(0).toElement()));
            // Check for valid fps
            if (!xmlProfile->hasValidFps()) {
                qWarning() << "######################################\n#  ERROR, non standard fps detected  #\n######################################";
                KMessageBox::error(
                    pCore->window(),
                    ki18nc("@label:textbox", "The project uses a non standard framerate (%1), this will result in misplaced clips and frame offset.")
                        .subs((double(xmlProfile->frame_rate_num()) / xmlProfile->frame_rate_den()), 0, 'f', 2)
                        .toString());
            }
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
    pCore->taskManager.slotCancelJobs(false, {AbstractTask::PROXYJOB, AbstractTask::AUDIOTHUMBJOB, AbstractTask::TRANSCODEJOB});
    double fps = pCore->getCurrentFps();
    double fpsChanged = m_timecode.fps() / fps;
    m_timecode.setFormat(fps);
    if (!reloadProducers) {
        return;
    }
    Q_EMIT updateFps(fpsChanged);
    pCore->bin()->reloadAllProducers(reloadThumbs);
}

void KdenliveDoc::resetProfile(bool reloadThumbs)
{
    updateProjectProfile(true, reloadThumbs);
    Q_EMIT docModified(true);
}

void KdenliveDoc::slotSwitchProfile(const QString &profile_path, bool reloadThumbs)
{
    // Discard all current jobs except proxy and audio thumbs
    pCore->taskManager.slotCancelJobs(false, {AbstractTask::PROXYJOB, AbstractTask::AUDIOTHUMBJOB, AbstractTask::TRANSCODEJOB});
    pCore->setCurrentProfile(profile_path);
    updateProjectProfile(true, reloadThumbs);
    // In case we only have one clip in timeline,
    Q_EMIT docModified(true);
}

void KdenliveDoc::switchProfile(ProfileParam *pf, const QString &clipName)
{
    // Request profile update
    // Check profile fps so that we don't end up with an fps = 30.003 which would mess things up
    QString adjustMessage;
    std::unique_ptr<ProfileParam> profile(pf);
    double fps = double(profile->frame_rate_num()) / profile->frame_rate_den();
    double fps_int;
    double fps_frac = std::modf(fps, &fps_int);
    if (fps_frac < 0.4) {
        profile->m_frame_rate_num = int(fps_int);
        profile->m_frame_rate_den = 1;
    } else {
        // Check for 23.98, 29.97, 59.94
        bool fpsFixed = false;
        if (qFuzzyCompare(fps_int, 23.0)) {
            if (qFuzzyCompare(fps, 23.98) || fps_frac > 0.94) {
                profile->m_frame_rate_num = 24000;
                profile->m_frame_rate_den = 1001;
                fpsFixed = true;
            }
        } else if (qFuzzyCompare(fps_int, 29.0)) {
            if (qFuzzyCompare(fps, 29.97) || fps_frac > 0.94) {
                profile->m_frame_rate_num = 30000;
                profile->m_frame_rate_den = 1001;
                fpsFixed = true;
            }
        } else if (qFuzzyCompare(fps_int, 59.0)) {
            if (qFuzzyCompare(fps, 59.94) || fps_frac > 0.9) {
                profile->m_frame_rate_num = 60000;
                profile->m_frame_rate_den = 1001;
                fpsFixed = true;
            }
        }
        if (!fpsFixed) {
            // Unknown profile fps, warn user
            profile->m_frame_rate_num = qRound(fps);
            profile->m_frame_rate_den = 1;
            adjustMessage = i18n("Warning: non standard fps, adjusting to closest integer. ");
        }
    }
    QString matchingProfile = ProfileRepository::get()->findMatchingProfile(profile.get());
    if (matchingProfile.isEmpty() && (profile->width() % 2 != 0)) {
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
            KMessageBox::ButtonCode answer = KMessageBox::questionTwoActionsCancel(
                QApplication::activeWindow(),
                i18n("Your default project profile is %1, but your clip's profile (%2) is %3.\nDo you want to change default profile for future projects?",
                     currentProfileDesc, clipName, profile->description()),
                i18n("Change default project profile"), KGuiItem(i18n("Change default to %1", profile->description())),
                KGuiItem(i18n("Keep current default %1", currentProfileDesc)), KGuiItem(i18n("Ask me later")));

            switch (answer) {
            case KMessageBox::PrimaryAction:
                // Discard all current jobs
                pCore->taskManager.slotCancelJobs(false, {AbstractTask::PROXYJOB, AbstractTask::AUDIOTHUMBJOB, AbstractTask::TRANSCODEJOB});
                KdenliveSettings::setDefault_profile(profile->path());
                pCore->setCurrentProfile(profile->path());
                updateProjectProfile(true, true);
                Q_EMIT docModified(true);
                return;
            case KMessageBox::SecondaryAction:
                return;
            default:
                break;
            }
        }

        // Build actions for the info message (switch / cancel)
        const QString profilePath = profile->path();
        QAction *ac = new QAction(QIcon::fromTheme(QStringLiteral("dialog-ok")), i18n("Switch"), this);
        connect(ac, &QAction::triggered, this, [this, profilePath]() { this->slotSwitchProfile(profilePath, true); });
        QAction *ac2 = new QAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")), i18n("Cancel"), this);
        QList<QAction *> list = {ac, ac2};
        adjustMessage.append(i18n("Switch to clip (%1) profile %2?", clipName, profile->descriptiveString()));
        pCore->displayBinMessage(adjustMessage, KMessageWidget::Information, list, false, BinMessage::BinCategory::ProfileMessage);
    } else {
        // No known profile, ask user if he wants to use clip profile anyway
        if (qFuzzyCompare(double(profile->m_frame_rate_num) / profile->m_frame_rate_den, fps)) {
            adjustMessage.append(i18n("\nProfile fps adjusted from original %1", QString::number(fps, 'f', 4)));
        } else if (!adjustMessage.isEmpty()) {
            adjustMessage.prepend(QLatin1Char('\n'));
        }
        if (KMessageBox::warningContinueCancel(pCore->window(), i18n("No profile found for your clip %1.\nCreate and switch to new profile (%2x%3, %4fps)?%5",
                                                                     clipName, profile->m_width, profile->m_height,
                                                                     QString::number(double(profile->m_frame_rate_num) / profile->m_frame_rate_den, 'f', 2),
                                                                     adjustMessage)) == KMessageBox::Continue) {
            profile->m_description = QStringLiteral("%1x%2 %3fps")
                                         .arg(profile->m_width)
                                         .arg(profile->m_height)
                                         .arg(QString::number(double(profile->m_frame_rate_num) / profile->m_frame_rate_den, 'f', 2));
            QString profilePath = ProfileRepository::get()->saveProfile(profile.get());
            // Discard all current jobs
            pCore->taskManager.slotCancelJobs(false, {AbstractTask::PROXYJOB, AbstractTask::AUDIOTHUMBJOB, AbstractTask::TRANSCODEJOB});
            pCore->setCurrentProfile(profilePath);
            updateProjectProfile(true, true);
            Q_EMIT docModified(true);
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
    if (pCore->window()) {
        Q_EMIT pCore->window()->setPreviewProgress(p);
    }
}

void KdenliveDoc::displayMessage(const QString &text, MessageType type, int timeOut)
{
    Q_EMIT pCore->window()->displayMessage(text, type, timeOut);
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
    if (!KdenliveSettings::supportedHWCodecs().isEmpty()) {
        QString codecFormat = QStringLiteral("x264-");
        codecFormat.append(KdenliveSettings::supportedHWCodecs().first().section(QLatin1Char('_'), 1));
        if (values.contains(codecFormat)) {
            const QString bestMatch = values.value(codecFormat);
            setDocumentProperty(QStringLiteral("previewparameters"), bestMatch.section(QLatin1Char(';'), 0, 0));
            setDocumentProperty(QStringLiteral("previewextension"), bestMatch.section(QLatin1Char(';'), 1, 1));
            return;
        }
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
                    if (qAbs(int(pCore->getCurrentFps() * 100) - (fps * 100)) <= 1) {
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
    if (!KdenliveSettings::supportedHWCodecs().isEmpty()) {
        QString codecFormat = QStringLiteral("x264-");
        codecFormat.append(KdenliveSettings::supportedHWCodecs().first().section(QLatin1Char('_'), 1));
        if (values.contains(codecFormat)) {
            params = values.value(codecFormat);
        }
    }
    if (params.isEmpty()) {
        params = values.value(QStringLiteral("MJPEG"));
    }
    m_proxyParams = params.section(QLatin1Char(';'), 0, 0);
    m_proxyExtension = params.section(QLatin1Char(';'), 1);
}

void KdenliveDoc::checkPreviewStack(int ix)
{
    // A command was pushed in the middle of the stack, remove all cached data from last undos
    Q_EMIT removeInvalidUndo(ix);
}

void KdenliveDoc::initCacheDirs()
{
    bool ok = false;
    QString kdenliveCacheDir;
    QString documentId = QDir::cleanPath(m_documentProperties.value(QStringLiteral("documentid")));
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

const QDir KdenliveDoc::getCacheDir(CacheType type, bool *ok, const QUuid uuid) const
{
    QString basePath;
    QString kdenliveCacheDir;
    QString documentId = QDir::cleanPath(m_documentProperties.value(QStringLiteral("documentid")));
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
    basePath = kdenliveCacheDir + QLatin1Char('/') + documentId; // CacheBase
    switch (type) {
    case SystemCacheRoot:
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    case CacheRoot:
        basePath = kdenliveCacheDir;
        break;
    case CachePreview:
        basePath.append(QStringLiteral("/preview"));
        if (!uuid.isNull() && uuid != m_uuid) {
            basePath.append(QStringLiteral("/%1").arg(QString(QCryptographicHash::hash(uuid.toByteArray(), QCryptographicHash::Md5).toHex())));
        }
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
    case CacheTmpWorkFiles:
        basePath.append(QStringLiteral("/workfiles"));
        break;
    case CacheSequence:
        basePath.append(QStringLiteral("/sequences"));
        break;
    default:
        break;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
        if (!dir.exists()) {
            *ok = false;
        }
    }
    return dir;
}

QStringList KdenliveDoc::getProxyHashList()
{
    return pCore->bin()->getProxyHashList();
}

std::shared_ptr<TimelineItemModel> KdenliveDoc::getTimeline(const QUuid &uuid, bool allowEmpty)
{
    if (m_timelines.contains(uuid)) {
        return m_timelines.value(uuid);
    }
    if (!allowEmpty) {
        qDebug() << "REQUESTING UNKNOWN TIMELINE: " << uuid;
        Q_ASSERT(false);
    }
    return nullptr;
}

QList<QUuid> KdenliveDoc::getTimelinesUuids() const
{
    return m_timelines.keys();
}

QStringList KdenliveDoc::getTimelinesIds()
{
    QStringList ids;
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    while (j.hasNext()) {
        j.next();
        ids << QString(j.value()->tractor()->get("id"));
    }
    return ids;
}

void KdenliveDoc::addTimeline(const QUuid &uuid, std::shared_ptr<TimelineItemModel> model, bool force)
{
    if (force && m_timelines.find(uuid) != m_timelines.end()) {
        std::shared_ptr<TimelineItemModel> previousModel = m_timelines.take(uuid);
        previousModel.reset();
    }
    if (m_timelines.find(uuid) != m_timelines.end()) {
        qDebug() << "::::: TIMELINE " << uuid << " already inserted in project";
        if (m_timelines.value(uuid) != model) {
            qDebug() << "::::: TIMELINE INCONSISTENCY";
            Q_ASSERT(false);
        }
        return;
    }
    if (m_timelines.isEmpty()) {
        activeUuid = uuid;
    }
    m_timelines.insert(uuid, model);
}

bool KdenliveDoc::checkConsistency()
{
    if (m_timelines.isEmpty()) {
        qDebug() << "==== CONSISTENCY CHECK FAILED; NO TIMELINE";
        return false;
    }
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    while (j.hasNext()) {
        j.next();
        if (!j.value()->checkConsistency()) {
            return false;
        }
    }
    return true;
}

void KdenliveDoc::loadSequenceGroupsAndGuides(const QUuid &uuid)
{
    Q_ASSERT(m_timelines.find(uuid) != m_timelines.end());
    std::shared_ptr<TimelineItemModel> model = m_timelines.value(uuid);
    // Load groups
    const QString groupsData = getSequenceProperty(uuid, QStringLiteral("groups"));
    if (!groupsData.isEmpty()) {
        model->loadGroups(groupsData);
        clearSequenceProperty(uuid, QStringLiteral("groups"));
    }
    // Load guides
    model->getGuideModel()->loadCategories(guidesCategories(), false);
    model->updateFieldOrderFilter(pCore->getCurrentProfile());
    loadDocumentGuides(uuid, model);
    connect(model.get(), &TimelineModel::saveGuideCategories, this, &KdenliveDoc::saveGuideCategories);
}

void KdenliveDoc::closeTimeline(const QUuid uuid, bool onDeletion)
{
    Q_ASSERT(m_timelines.find(uuid) != m_timelines.end());
    // Sync all sequence properties
    if (onDeletion) {
        auto model = m_timelines.take(uuid);
        model->prepareClose(!closing);
        model.reset();
    } else {
        auto model = m_timelines.value(uuid);
        if (!closing) {
            setSequenceProperty(uuid, QStringLiteral("groups"), model->groupsData());
            model->passSequenceProperties(getSequenceProperties(uuid));
        }
        model->isClosed = true;
    }
    // Clear all sequence properties
    m_sequenceProperties.remove(uuid);
}

void KdenliveDoc::storeGroups(const QUuid &uuid)
{
    Q_ASSERT(m_timelines.find(uuid) != m_timelines.end());
    setSequenceProperty(uuid, QStringLiteral("groups"), m_timelines.value(uuid)->groupsData());
    m_timelines.value(uuid)->passSequenceProperties(getSequenceProperties(uuid));
}

void KdenliveDoc::checkUsage(const QUuid &uuid)
{
    Q_ASSERT(m_timelines.find(uuid) != m_timelines.end());
    qDebug() << "===== CHECKING USAGE FOR: " << uuid << " = " << m_timelines.value(uuid).use_count();
}

std::shared_ptr<MarkerSortModel> KdenliveDoc::getFilteredGuideModel(const QUuid uuid)
{
    Q_ASSERT(m_timelines.find(uuid) != m_timelines.end());
    return m_timelines.value(uuid)->getFilteredGuideModel();
}

std::shared_ptr<MarkerListModel> KdenliveDoc::getGuideModel(const QUuid uuid) const
{
    Q_ASSERT(m_timelines.find(uuid) != m_timelines.end());
    return m_timelines.value(uuid)->getGuideModel();
}

int KdenliveDoc::openedTimelineCount() const
{
    return m_timelines.size();
}

const QStringList KdenliveDoc::getSecondaryTimelines() const
{
    QString timelines = getDocumentProperty(QStringLiteral("timelines"));
    if (timelines.isEmpty()) {
        return QStringList();
    }
    return getDocumentProperty(QStringLiteral("timelines")).split(QLatin1Char(';'));
}

const QString KdenliveDoc::projectName() const
{
    if (!m_url.isValid()) {
        return i18n("Untitled");
    }
    return m_url.fileName();
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

QMap<int, QStringList> KdenliveDoc::getProjectTags() const
{
    QMap<int, QStringList> tags;
    int ix = 1;
    for (int i = 1; i < 50; i++) {
        QString current = getDocumentProperty(QString("tag%1").arg(i));
        if (current.isEmpty()) {
            break;
        }
        tags.insert(ix, {QString::number(ix), current.section(QLatin1Char(':'), 0, 0), current.section(QLatin1Char(':'), 1)});
        ix++;
    }
    if (tags.isEmpty()) {
        tags.insert(1, {QStringLiteral("1"), QStringLiteral("#ff0000"), i18n("Red")});
        tags.insert(2, {QStringLiteral("2"), QStringLiteral("#00ff00"), i18n("Green")});
        tags.insert(3, {QStringLiteral("3"), QStringLiteral("#0000ff"), i18n("Blue")});
        tags.insert(4, {QStringLiteral("4"), QStringLiteral("#ffff00"), i18n("Yellow")});
        tags.insert(5, {QStringLiteral("5"), QStringLiteral("#00ffff"), i18n("Cyan")});
    }
    return tags;
}

int KdenliveDoc::audioChannels() const
{
    return getDocumentProperty(QStringLiteral("audioChannels"), QStringLiteral("2")).toInt();
}

QString &KdenliveDoc::modifiedDecimalPoint()
{
    return m_modifiedDecimalPoint;
}

const QString KdenliveDoc::subTitlePath(const QUuid &uuid, int ix, bool final)
{
    QString documentId = QDir::cleanPath(m_documentProperties.value(QStringLiteral("documentid")));
    QString path = (m_url.isValid() && final) ? m_url.fileName() : documentId;
    if (uuid != m_uuid) {
        path.append(uuid.toString());
    }
    if (ix > 0) {
        path.append(QStringLiteral("-%1").arg(ix));
    }
    if (m_url.isValid() && final) {
        return QFileInfo(m_url.toLocalFile()).dir().absoluteFilePath(QString("%1.srt").arg(path));
    } else {
        return QDir::temp().absoluteFilePath(QString("%1-%2.srt").arg(path, pCore->sessionId));
    }
}

void KdenliveDoc::duplicateSequenceProperty(const QUuid &destUuid, const QUuid &srcUuid, const QString &subsData)
{
    QJsonArray list;
    QMap<std::pair<int, QString>, QString> currentSubs = JSonToSubtitleList(subsData);
    QMapIterator<std::pair<int, QString>, QString> s(currentSubs);
    while (s.hasNext()) {
        s.next();
        const QString newSubPath = pCore->currentDoc()->subTitlePath(destUuid, s.key().first, false);
        QJsonObject currentSubtitle;
        currentSubtitle.insert(QLatin1String("name"), QJsonValue(s.key().second));
        currentSubtitle.insert(QLatin1String("id"), QJsonValue(s.key().first));
        currentSubtitle.insert(QLatin1String("file"), QJsonValue(newSubPath));
        QString srcSub = s.value();
        if (QFileInfo(srcSub).isRelative()) {
            srcSub.prepend(m_documentRoot);
        }
        QFile src(srcSub);
        if (!src.exists()) {
            srcSub = pCore->currentDoc()->subTitlePath(srcUuid, s.key().first, true);
            if (QFileInfo(srcSub).isRelative()) {
                srcSub.prepend(m_documentRoot);
            }
            src.setFileName(srcSub);
        }
        if (!src.exists()) {
            qDebug() << "SUBTITLE SRC FILE: " << srcSub << " DOES NOT EXIST";
        } else {
            src.copy(newSubPath);
        }
        list.push_back(currentSubtitle);
    }
    QJsonDocument json(list);
    const QString subsJson = QString::fromUtf8(json.toJson());
    setSequenceProperty(destUuid, QStringLiteral("subtitlesList"), subsJson);
}

QMap<std::pair<int, QString>, QString> KdenliveDoc::multiSubtitlePath(const QUuid &uuid)
{
    const QString data = getSequenceProperty(uuid, QStringLiteral("subtitlesList"));
    return KdenliveDoc::JSonToSubtitleList(data);
}

QMap<std::pair<int, QString>, QString> KdenliveDoc::JSonToSubtitleList(const QString &data)
{
    QMap<std::pair<int, QString>, QString> results;
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return results;
    }
    auto list = json.array();
    for (const auto &entry : qAsConst(list)) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid subtitle data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("name")) || !entryObj.contains(QLatin1String("file"))) {
            qDebug() << "Warning : Skipping invalid subtitle data (does not have a name or file)";
            continue;
        }
        const QString subName = entryObj[QLatin1String("name")].toString();
        int subId = entryObj[QLatin1String("id")].toInt();
        QString subUrl = entryObj[QLatin1String("file")].toString();
        if (QFileInfo(subUrl).isRelative()) {
            subUrl.prepend(m_documentRoot);
        }
        results.insert({subId, subName}, subUrl);
    }
    return results;
}

bool KdenliveDoc::hasSubtitles() const
{
    QMapIterator<QUuid, std::shared_ptr<TimelineItemModel>> j(m_timelines);
    while (j.hasNext()) {
        j.next();
        if (j.value()->hasSubtitleModel()) {
            return true;
        }
    }
    return false;
}

void KdenliveDoc::generateRenderSubtitleFile(const QUuid &uuid, int in, int out, const QString &subtitleFile)
{
    if (m_timelines.contains(uuid)) {
        m_timelines.value(uuid)->getSubtitleModel()->subtitleFileFromZone(in, out, subtitleFile);
    }
}

// static
void KdenliveDoc::useOriginals(QDomDocument &doc)
{
    QString root = doc.documentElement().attribute(QStringLiteral("root"));
    if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
        root.append(QLatin1Char('/'));
    }

    // replace proxy clips with originals
    QMap<QString, QString> proxies = pCore->projectItemModel()->getProxies(root);
    QDomNodeList producers = doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList chains = doc.elementsByTagName(QStringLiteral("chain"));
    processProxyNodes(producers, root, proxies);
    processProxyNodes(chains, root, proxies);
}

// static
void KdenliveDoc::disableSubtitles(QDomDocument &doc)
{
    QDomNodeList filters = doc.elementsByTagName(QStringLiteral("filter"));
    for (int i = 0; i < filters.length(); ++i) {
        auto filter = filters.at(i).toElement();
        if (Xml::getXmlProperty(filter, QStringLiteral("mlt_service")) == QLatin1String("avfilter.subtitles")) {
            Xml::setXmlProperty(filter, QStringLiteral("disable"), QStringLiteral("1"));
        }
    }
}

void KdenliveDoc::makeBackgroundTrackTransparent(QDomDocument &doc)
{
    QDomNodeList prods = doc.elementsByTagName(QStringLiteral("producer"));
    // Switch all black track producers to transparent
    for (int i = 0; i < prods.length(); ++i) {
        auto prod = prods.at(i).toElement();
        if (Xml::getXmlProperty(prod, QStringLiteral("kdenlive:playlistid")) == QStringLiteral("black_track")) {
            Xml::setXmlProperty(prod, QStringLiteral("resource"), QStringLiteral("0"));
        }
    }
}

void KdenliveDoc::setAutoclosePlaylists(QDomDocument &doc, const QString &mainSequenceUuid)
{
    // We should only set the autoclose attribute on the main sequence playlists.
    // Otherwise if a sequence is reused several times, its playback will be broken
    QDomNodeList playlists = doc.elementsByTagName(QStringLiteral("playlist"));
    QDomNodeList tractors = doc.elementsByTagName(QStringLiteral("tractor"));
    QStringList matches;
    for (int i = 0; i < tractors.length(); ++i) {
        if (tractors.at(i).toElement().attribute(QStringLiteral("id")) == mainSequenceUuid) {
            // We found the main sequence tractor, list its tracks
            QDomNodeList tracks = tractors.at(i).toElement().elementsByTagName(QStringLiteral("track"));
            for (int j = 0; j < tracks.length(); ++j) {
                matches << tracks.at(j).toElement().attribute(QStringLiteral("producer"));
            }
            break;
        }
    }
    for (int i = 0; i < playlists.length(); ++i) {
        auto playlist = playlists.at(i).toElement();
        if (matches.contains(playlist.attribute(QStringLiteral("id")))) {
            playlist.setAttribute(QStringLiteral("autoclose"), 1);
        }
    }
}

void KdenliveDoc::processProxyNodes(QDomNodeList producers, const QString &root, const QMap<QString, QString> &proxies)
{

    QString producerResource;
    QString producerService;
    QString originalProducerService;
    QString suffix;
    QString prefix;
    for (int n = 0; n < producers.length(); ++n) {
        QDomElement e = producers.item(n).toElement();
        producerResource = Xml::getXmlProperty(e, QStringLiteral("resource"));
        producerService = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        originalProducerService = Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.mlt_service"));
        if (producerResource.isEmpty() || producerService == QLatin1String("color")) {
            continue;
        }
        if (producerService == QLatin1String("timewarp")) {
            // slowmotion producer
            prefix = producerResource.section(QLatin1Char(':'), 0, 0) + QLatin1Char(':');
            producerResource = producerResource.section(QLatin1Char(':'), 1);
        } else {
            prefix.clear();
        }
        if (producerService == QLatin1String("framebuffer")) {
            // slowmotion producer
            suffix = QLatin1Char('?') + producerResource.section(QLatin1Char('?'), 1);
            producerResource = producerResource.section(QLatin1Char('?'), 0, 0);
        } else {
            suffix.clear();
        }
        if (!producerResource.isEmpty()) {
            if (QFileInfo(producerResource).isRelative()) {
                producerResource.prepend(root);
            }
            if (proxies.contains(producerResource)) {
                if (!originalProducerService.isEmpty() && originalProducerService != producerService) {
                    // Proxy clips can sometimes use a different mlt service, for example playlists (xml) will use avformat. Fix
                    Xml::setXmlProperty(e, QStringLiteral("mlt_service"), originalProducerService);
                }
                QString replacementResource = proxies.value(producerResource);
                Xml::setXmlProperty(e, QStringLiteral("resource"), prefix + replacementResource + suffix);
                if (producerService == QLatin1String("timewarp")) {
                    Xml::setXmlProperty(e, QStringLiteral("warp_resource"), replacementResource);
                }
                // We need to delete the "aspect_ratio" property because proxy clips
                // sometimes have different ratio than original clips
                Xml::removeXmlProperty(e, QStringLiteral("aspect_ratio"));
                Xml::removeMetaProperties(e);
            }
        }
    }
}

void KdenliveDoc::cleanupTimelinePreview(const QDateTime &documentDate)
{
    if (m_url.isEmpty()) {
        // Document was never saved, nothing to do
        return;
    }
    bool ok;
    QDir cacheDir = getCacheDir(CachePreview, &ok);
    if (cacheDir.exists() && cacheDir.dirName() == QLatin1String("preview") && ok) {
        QFileInfoList chunksList = cacheDir.entryInfoList(QDir::Files, QDir::Time);
        for (auto &chunkFile : chunksList) {
            if (chunkFile.lastModified() > documentDate) {
                // This chunk is invalid
                QString chunkName = chunkFile.fileName().section(QLatin1Char('.'), 0, 0);
                bool ok;
                chunkName.toInt(&ok);
                if (!ok) {
                    // This is not one of our chunks
                    continue;
                }
                // Physically remove chunk file
                cacheDir.remove(chunkFile.fileName());
            } else {
                // Done
                break;
            }
        }
        // Check secondary timelines preview folders
        QFileInfoList dirsList = cacheDir.entryInfoList(QDir::AllDirs, QDir::Time);
        for (auto &dir : dirsList) {
            QDir sourceDir(dir.absolutePath());
            if (!sourceDir.absolutePath().contains(QLatin1String("preview"))) {
                continue;
            }
            QFileInfoList chunksList = sourceDir.entryInfoList(QDir::Files, QDir::Time);
            for (auto &chunkFile : chunksList) {
                if (chunkFile.lastModified() > documentDate) {
                    // This chunk is invalid
                    QString chunkName = chunkFile.fileName().section(QLatin1Char('.'), 0, 0);
                    bool ok;
                    chunkName.toInt(&ok);
                    if (!ok) {
                        // This is not one of our chunks
                        continue;
                    }
                    // Physically remove chunk file
                    sourceDir.remove(chunkFile.fileName());
                } else {
                    // Done
                    break;
                }
            }
        }
    }
}

// static
const QStringList KdenliveDoc::getDefaultGuideCategories()
{
    // Don't change this or it will break compatibility for projects created with Kdenlive < 22.12
    QStringList colors = {QLatin1String("#9b59b6"), QLatin1String("#3daee9"), QLatin1String("#1abc9c"), QLatin1String("#1cdc9a"), QLatin1String("#c9ce3b"),
                          QLatin1String("#fdbc4b"), QLatin1String("#f39c1f"), QLatin1String("#f47750"), QLatin1String("#da4453")};
    QStringList guidesCategories;
    for (int i = 0; i < 9; i++) {
        guidesCategories << QString("%1 %2:%3:%4").arg(i18n("Category")).arg(QString::number(i + 1)).arg(QString::number(i)).arg(colors.at(i));
    }
    return guidesCategories;
}

const QUuid KdenliveDoc::uuid() const
{
    return m_uuid;
}

void KdenliveDoc::loadSequenceProperties(const QUuid &uuid, Mlt::Properties sequenceProps)
{
    QMap<QString, QString> sequenceProperties = m_sequenceProperties.take(uuid);
    for (int i = 0; i < sequenceProps.count(); i++) {
        sequenceProperties.insert(qstrdup(sequenceProps.get_name(i)), qstrdup(sequenceProps.get(i)));
    }
    m_sequenceProperties.insert(uuid, sequenceProperties);
}
