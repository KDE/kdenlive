/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "previewmanager.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "profiles/profilemodel.hpp"
#include "timeline2/view/timelinecontroller.h"

#include <KLocalizedString>
#include <QProcess>
#include <QStandardPaths>
#include <QCollator>

PreviewManager::PreviewManager(TimelineController *controller, Mlt::Tractor *tractor)
    : QObject()
    , workingPreview(-1)
    , m_controller(controller)
    , m_tractor(tractor)
    , m_previewTrack(nullptr)
    , m_overlayTrack(nullptr)
    , m_previewTrackIndex(-1)
    , m_initialized(false)
{
    m_previewGatherTimer.setSingleShot(true);
    m_previewGatherTimer.setInterval(200);
    QObject::connect(&m_previewProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &PreviewManager::processEnded);


    // Find path for Kdenlive renderer
#ifdef Q_OS_WIN
    m_renderer = QCoreApplication::applicationDirPath() + QStringLiteral("/kdenlive_render.exe");
#else
    m_renderer = QCoreApplication::applicationDirPath() + QStringLiteral("/kdenlive_render");
#endif
    if (!QFile::exists(m_renderer)) {
        m_renderer = QStandardPaths::findExecutable(QStringLiteral("kdenlive_render"));
        if (m_renderer.isEmpty()) {
            m_renderer = QStringLiteral("kdenlive_render");
        }
    }
    connect(this, &PreviewManager::abortPreview, &m_previewProcess, &QProcess::kill, Qt::DirectConnection);
    connect(&m_previewProcess, &QProcess::readyReadStandardError, this, &PreviewManager::receivedStderr);
}

PreviewManager::~PreviewManager()
{
    if (m_initialized) {
        abortRendering();
        if (m_undoDir.dirName() == QLatin1String("undo")) {
            m_undoDir.removeRecursively();
        }
        if ((pCore->currentDoc()->url().isEmpty() && m_cacheDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) ||
            m_cacheDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            if (m_cacheDir.dirName() == QLatin1String("preview")) {
                m_cacheDir.removeRecursively();
            }
        }
    }
    delete m_overlayTrack;
    delete m_previewTrack;
}

bool PreviewManager::initialize()
{
    // Make sure our document id does not contain .. tricks
    bool ok;
    KdenliveDoc *doc = pCore->currentDoc();
    QString documentId = QDir::cleanPath(doc->getDocumentProperty(QStringLiteral("documentid")));
    documentId.toLongLong(&ok, 10);
    if (!ok || documentId.isEmpty()) {
        // Something is wrong, documentId should be a number (ms since epoch), abort
        pCore->displayMessage(i18n("Wrong document ID, cannot create temporary folder"), ErrorMessage);
        return false;
    }
    m_cacheDir = doc->getCacheDir(CachePreview, &ok);
    if (!m_cacheDir.exists() || !ok) {
        pCore->displayMessage(i18n("Cannot create folder %1", m_cacheDir.absolutePath()), ErrorMessage);
        return false;
    }
    if (m_cacheDir.dirName() != QLatin1String("preview") || m_cacheDir == QDir() ||
        (!m_cacheDir.exists(QStringLiteral("undo")) && !m_cacheDir.mkdir(QStringLiteral("undo"))) || !m_cacheDir.absolutePath().contains(documentId)) {
        pCore->displayMessage(i18n("Something is wrong with cache folder %1", m_cacheDir.absolutePath()), ErrorMessage);
        return false;
    }
    if (!loadParams()) {
        pCore->displayMessage(i18n("Invalid timeline preview parameters"), ErrorMessage);
        return false;
    }
    m_undoDir = QDir(m_cacheDir.absoluteFilePath(QStringLiteral("undo")));

    // Make sure our cache dirs are inside the temporary folder
    if (!m_cacheDir.makeAbsolute() || !m_undoDir.makeAbsolute() || !m_undoDir.mkpath(QStringLiteral("."))) {
        pCore->displayMessage(i18n("Something is wrong with cache folders"), ErrorMessage);
        return false;
    }

    connect(this, &PreviewManager::cleanupOldPreviews, this, &PreviewManager::doCleanupOldPreviews);
    connect(doc, &KdenliveDoc::removeInvalidUndo, this, &PreviewManager::slotRemoveInvalidUndo, Qt::DirectConnection);
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(3000);
    connect(&m_previewTimer, &QTimer::timeout, this, &PreviewManager::startPreviewRender);
    connect(this, &PreviewManager::previewRender, this, &PreviewManager::gotPreviewRender, Qt::DirectConnection);
    connect(&m_previewGatherTimer, &QTimer::timeout, this, &PreviewManager::slotProcessDirtyChunks);
    m_initialized = true;
    return true;
}

bool PreviewManager::buildPreviewTrack()
{
    if (m_previewTrack != nullptr) {
        return false;
    }
    // Create overlay track
    qDebug() << "/// BUILDING PREVIEW TRACK\n----------------------\n----------------__";
    m_previewTrack = new Mlt::Playlist(pCore->getCurrentProfile()->profile());
    m_previewTrack->set("id", "timeline_preview");
    m_tractor->lock();
    reconnectTrack();
    m_tractor->unlock();
    return true;
}

void PreviewManager::loadChunks(QVariantList previewChunks, QVariantList dirtyChunks, const QDateTime &documentDate)
{
    if (previewChunks.isEmpty()) {
        previewChunks = m_renderedChunks;
    }
    if (dirtyChunks.isEmpty()) {
        dirtyChunks = m_dirtyChunks;
    }
    for (const auto &frame : qAsConst(previewChunks)) {
        const QString fileName = m_cacheDir.absoluteFilePath(QStringLiteral("%1.%2").arg(frame.toInt()).arg(m_extension));
        QFile file(fileName);
        if (file.exists()) {
            if (!documentDate.isNull() && QFileInfo(file).lastModified() > documentDate) {
                // Timeline preview file was created after document, invalidate
                file.remove();
                dirtyChunks << frame;
            } else {
                gotPreviewRender(frame.toInt(), fileName, 1000);
            }
        } else {
            dirtyChunks << frame;
        }
    }
    if (!previewChunks.isEmpty()) {
        emit m_controller->renderedChunksChanged();
    }
    if (!dirtyChunks.isEmpty()) {
        for (const auto &i : qAsConst(dirtyChunks)) {
            if (!m_dirtyChunks.contains(i)) {
                m_dirtyChunks << i;
            }
        }
        emit m_controller->dirtyChunksChanged();
    }
}

void PreviewManager::deletePreviewTrack()
{
    m_tractor->lock();
    disconnectTrack();
    delete m_previewTrack;
    m_previewTrack = nullptr;
    m_dirtyChunks.clear();
    m_renderedChunks.clear();
    emit m_controller->dirtyChunksChanged();
    emit m_controller->renderedChunksChanged();
    m_tractor->unlock();
}

const QDir PreviewManager::getCacheDir() const
{
    return m_cacheDir;
}

void PreviewManager::reconnectTrack()
{
    disconnectTrack();
    if (!m_previewTrack && !m_overlayTrack) {
        m_previewTrackIndex = -1;
        return;
    }
    m_previewTrackIndex = m_tractor->count();
    int increment = 0;
    if (m_previewTrack) {
        m_tractor->insert_track(*m_previewTrack, m_previewTrackIndex);
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(m_previewTrackIndex));
        tk->set("hide", 2);
        //tk->set("id", "timeline_preview");
        increment++;
    }
    if (m_overlayTrack) {
        m_tractor->insert_track(*m_overlayTrack, m_previewTrackIndex + increment);
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(m_previewTrackIndex + increment));
        tk->set("hide", 2);
        //tk->set("id", "timeline_overlay");
    }
}

void PreviewManager::disconnectTrack()
{
    if (m_previewTrackIndex > -1) {
        Mlt::Producer *prod = m_tractor->track(m_previewTrackIndex);
        if (strcmp(prod->get("id"), "timeline_preview") == 0 || strcmp(prod->get("id"), "timeline_overlay") == 0) {
            m_tractor->remove_track(m_previewTrackIndex);
        }
        delete prod;
        if (m_tractor->count() == m_previewTrackIndex + 1) {
            // overlay track still here, remove
            Mlt::Producer *trkprod = m_tractor->track(m_previewTrackIndex);
            if (strcmp(trkprod->get("id"), "timeline_overlay") == 0) {
                m_tractor->remove_track(m_previewTrackIndex);
            }
            delete trkprod;
        }
    }
    m_previewTrackIndex = -1;
}

void PreviewManager::disable()
{
    if (m_previewTrackIndex > -1) {
        if (m_previewTrack) {
            m_previewTrack->set("hide", 3);
        }
        if (m_overlayTrack) {
            m_overlayTrack->set("hide", 3);
        }
    }
}

void PreviewManager::enable()
{
    if (m_previewTrackIndex > -1) {
        if (m_previewTrack) {
            m_previewTrack->set("hide", 2);
        }
        if (m_overlayTrack) {
            m_overlayTrack->set("hide", 2);
        }
    }
}

bool PreviewManager::loadParams()
{
    KdenliveDoc *doc = pCore->currentDoc();
    m_extension = doc->getDocumentProperty(QStringLiteral("previewextension"));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
    m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif

    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        doc->selectPreviewProfile();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
        m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
        m_extension = doc->getDocumentProperty(QStringLiteral("previewextension"));
    }
    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        return false;
    }
    // Remove the r= and s= parameter (forcing framerate / frame size) as it causes rendering failure.
    // These parameters should be provided by MLT's profile
    // NOTE: this is still required for DNxHD so leave it
    /*for (int i = 0; i < m_consumerParams.count(); i++) {
        if (m_consumerParams.at(i).startsWith(QStringLiteral("r=")) || m_consumerParams.at(i).startsWith(QStringLiteral("s="))) {
            m_consumerParams.removeAt(i);
            i--;
        }
    }*/
    if (doc->getDocumentProperty(QStringLiteral("resizepreview")).toInt() != 0) {
        int resizeWidth = doc->getDocumentProperty(QStringLiteral("previewheight")).toInt();
        m_consumerParams << QStringLiteral("s=%1x%2").arg((int)(resizeWidth * pCore->getCurrentDar())).arg(resizeWidth);
    }
    m_consumerParams << QStringLiteral("an=1");
    if (KdenliveSettings::gpu_accel()) {
        m_consumerParams << QStringLiteral("glsl.=1");
    }
    return true;
}

void PreviewManager::invalidatePreviews(const QVariantList chunks)
{
    QMutexLocker lock(&m_previewMutex);
    bool timer = KdenliveSettings::autopreview();
    if (m_previewTimer.isActive()) {
        m_previewTimer.stop();
        timer = true;
    }
    KdenliveDoc *doc = pCore->currentDoc();
    int stackIx = doc->commandStack()->index();
    int stackMax = doc->commandStack()->count();
    if (stackIx == stackMax && !m_undoDir.exists(QString::number(stackIx - 1))) {
        // We just added a new command in stack, archive existing chunks
        int ix = stackIx - 1;
        m_undoDir.mkdir(QString::number(ix));
        bool foundPreviews = false;
        for (const auto &i : chunks) {
            QString current = QStringLiteral("%1.%2").arg(i.toInt()).arg(m_extension);
            if (m_cacheDir.rename(current, QStringLiteral("undo/%1/%2").arg(ix).arg(current))) {
                foundPreviews = true;
            }
        }
        if (!foundPreviews) {
            // No preview files found, remove undo folder
            m_undoDir.rmdir(QString::number(ix));
        } else {
            // new chunks archived, cleanup old ones
            emit cleanupOldPreviews();
        }
    } else {
        // Restore existing chunks, delete others
        // Check if we just undo the last stack action, then backup, otherwise delete
        bool lastUndo = false;
        if (stackIx + 1 == stackMax) {
            if (!m_undoDir.exists(QString::number(stackMax))) {
                lastUndo = true;
                bool foundPreviews = false;
                m_undoDir.mkdir(QString::number(stackMax));
                for (const auto &i : chunks) {
                    QString current = QStringLiteral("%1.%2").arg(i.toInt()).arg(m_extension);
                    if (m_cacheDir.rename(current, QStringLiteral("undo/%1/%2").arg(stackMax).arg(current))) {
                        foundPreviews = true;
                    }
                }
                if (!foundPreviews) {
                    m_undoDir.rmdir(QString::number(stackMax));
                }
            }
        }
        bool moveFile = true;
        QDir tmpDir = m_undoDir;
        if (!tmpDir.cd(QString::number(stackIx))) {
            moveFile = false;
        }
        QVariantList foundChunks;
        for (const auto &i : chunks) {
            QString cacheFileName = QStringLiteral("%1.%2").arg(i.toInt()).arg(m_extension);
            if (!lastUndo) {
                m_cacheDir.remove(cacheFileName);
            }
            if (moveFile) {
                if (QFile::copy(tmpDir.absoluteFilePath(cacheFileName), m_cacheDir.absoluteFilePath(cacheFileName))) {
                    foundChunks << i;
                    m_dirtyChunks.removeAll(i);
                    m_renderedChunks << i;
                } else {
                    qDebug() << "// ERROR PROCESSE CHUNK: " << i << ", " << cacheFileName;
                }
            }
        }
        if (!foundChunks.isEmpty()) {
            std::sort(foundChunks.begin(), foundChunks.end());
            emit m_controller->dirtyChunksChanged();
            emit m_controller->renderedChunksChanged();
            reloadChunks(foundChunks);
        }
    }
    doc->setModified(true);
    if (timer) {
        m_previewTimer.start();
    }
}

void PreviewManager::doCleanupOldPreviews()
{
    if (m_undoDir.dirName() != QLatin1String("undo")) {
        return;
    }
    QStringList dirs = m_undoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // Use QCollator to do a natural sorting so that 10 is after 2
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(dirs.begin(), dirs.end(), [&collator](const QString &file1, const QString &file2) { return collator.compare(file1, file2) < 0; });
    bool ok;
    while (dirs.count() > 5) {
        QDir tmp = m_undoDir;
        QString dirName = dirs.takeFirst();
        dirName.toInt(&ok);
        if (ok && tmp.cd(dirName)) {
            tmp.removeRecursively();
        }
    }
}

void PreviewManager::clearPreviewRange(bool resetZones)
{
    m_previewGatherTimer.stop();
    abortRendering();
    m_tractor->lock();
    bool hasPreview = m_previewTrack != nullptr;
    for (const auto &ix : qAsConst(m_renderedChunks)) {
        m_cacheDir.remove(QStringLiteral("%1.%2").arg(ix.toInt()).arg(m_extension));
        if (!m_dirtyChunks.contains(ix)) {
            m_dirtyChunks << ix;
        }
        if (!hasPreview) {
            continue;
        }
        int trackIx = m_previewTrack->get_clip_index_at(ix.toInt());
        if (!m_previewTrack->is_blank(trackIx)) {
            Mlt::Producer *prod = m_previewTrack->replace_with_blank(trackIx);
            delete prod;
        }
    }
    if (hasPreview) {
        m_previewTrack->consolidate_blanks();
    }
    m_tractor->unlock();
    m_renderedChunks.clear();
    // Reload preview params
    loadParams();
    if (resetZones) {
        m_dirtyChunks.clear();
    }
    emit m_controller->renderedChunksChanged();
    emit m_controller->dirtyChunksChanged();
}

void PreviewManager::addPreviewRange(const QPoint zone, bool add)
{
    int chunkSize = KdenliveSettings::timelinechunks();
    int startChunk = zone.x() / chunkSize;
    int endChunk = rintl(zone.y() / chunkSize);
    QList<int> toRemove;
    qDebug() << " // / RESUQEST CHUNKS; " << startChunk << " = " << endChunk;
    for (int i = startChunk; i <= endChunk; i++) {
        int frame = i * chunkSize;
        if (add) {
            if (!m_renderedChunks.contains(frame) && !m_dirtyChunks.contains(frame)) {
                m_dirtyChunks << frame;
            }
        } else {
            if (m_renderedChunks.contains(frame)) {
                toRemove << frame;
                m_renderedChunks.removeAll(frame);
            } else {
                m_dirtyChunks.removeAll(frame);
            }
        }
    }
    if (add) {
        qDebug() << "CHUNKS CHANGED: " << m_dirtyChunks;
        emit m_controller->dirtyChunksChanged();
        if (m_previewProcess.state() == QProcess::NotRunning && KdenliveSettings::autopreview()) {
            m_previewTimer.start();
        }
    } else {
        // Remove processed chunks
        bool isRendering = m_previewProcess.state() != QProcess::NotRunning;
        m_previewGatherTimer.stop();
        abortRendering();
        m_tractor->lock();
        bool hasPreview = m_previewTrack != nullptr;
        for (int ix : qAsConst(toRemove)) {
            m_cacheDir.remove(QStringLiteral("%1.%2").arg(ix).arg(m_extension));
            if (!hasPreview) {
                continue;
            }
            int trackIx = m_previewTrack->get_clip_index_at(ix);
            if (!m_previewTrack->is_blank(trackIx)) {
                Mlt::Producer *prod = m_previewTrack->replace_with_blank(trackIx);
                delete prod;
            }
        }
        if (hasPreview) {
            m_previewTrack->consolidate_blanks();
        }
        emit m_controller->renderedChunksChanged();
        emit m_controller->dirtyChunksChanged();
        m_tractor->unlock();
        if (isRendering || KdenliveSettings::autopreview()) {
            m_previewTimer.start();
        }
    }
}

void PreviewManager::abortRendering()
{
    if (m_previewProcess.state() == QProcess::NotRunning) {
        return;
    }
    qDebug() << "/// ABORTING RENDEIGN 1\nRRRRRRRRRR";
    emit abortPreview();
    m_previewProcess.waitForFinished();
    if (m_previewProcess.state() != QProcess::NotRunning) {
        m_previewProcess.kill();
        m_previewProcess.waitForFinished();
    }
    // Re-init time estimation
    emit previewRender(-1, QString(), 1000);
}

void PreviewManager::startPreviewRender()
{
    QMutexLocker lock(&m_previewMutex);
    if (m_renderedChunks.isEmpty() && m_dirtyChunks.isEmpty()) {
        m_controller->addPreviewRange(true);
    }
    if (!m_dirtyChunks.isEmpty()) {
        // Abort any rendering
        abortRendering();
        m_waitingThumbs.clear();
        // clear log
        m_errorLog.clear();
        const QString sceneList = m_cacheDir.absoluteFilePath(QStringLiteral("preview.mlt"));
        pCore->getMonitor(Kdenlive::ProjectMonitor)->sceneList(m_cacheDir.absolutePath(), sceneList);
        pCore->currentDoc()->saveMltPlaylist(sceneList);
        m_previewTimer.stop();
        doPreviewRender(sceneList);
    }
}

void PreviewManager::receivedStderr()
{
    QStringList resultList = QString::fromLocal8Bit(m_previewProcess.readAllStandardError()).split(QLatin1Char('\n'));
    for (auto &result : resultList) {
        qDebug() << "GOT PROCESS RESULT: " << result;
        if (result.startsWith(QLatin1String("START:"))) {
            workingPreview = result.section(QLatin1String("START:"), 1).simplified().toInt();
            qDebug() << "// GOT START INFO: " << workingPreview;
            emit m_controller->workingPreviewChanged();
        } else if (result.startsWith(QLatin1String("DONE:"))) {
            int chunk = result.section(QLatin1String("DONE:"), 1).simplified().toInt();
            m_processedChunks++;
            QString fileName = QStringLiteral("%1.%2").arg(chunk).arg(m_extension);
            qDebug() << "---------------\nJOB PROGRRESS: " << m_chunksToRender << ", " << m_processedChunks << " = "
                     << (100 * m_processedChunks / m_chunksToRender);
            emit previewRender(chunk, m_cacheDir.absoluteFilePath(fileName), 1000 * m_processedChunks / m_chunksToRender);
        } else {
            m_errorLog.append(result);
        }
    }
}

void PreviewManager::doPreviewRender(const QString &scene)
{
    // initialize progress bar
    std::sort(m_dirtyChunks.begin(), m_dirtyChunks.end());
    if (m_dirtyChunks.isEmpty()) {
        return;
    }
    Q_ASSERT(m_previewProcess.state() == QProcess::NotRunning);

    QStringList chunks;
    for (QVariant &frame : m_dirtyChunks) {
        chunks << frame.toString();
    }
    m_chunksToRender = m_dirtyChunks.count();
    m_processedChunks = 0;
    int chunkSize = KdenliveSettings::timelinechunks();
    QStringList args{KdenliveSettings::rendererpath(),
                     scene,
                     m_cacheDir.absolutePath(),
                     QStringLiteral("-split"),
                     chunks.join(QLatin1Char(',')),
                     QString::number(chunkSize - 1),
                     pCore->getCurrentProfilePath(),
                     m_extension,
                     m_consumerParams.join(QLatin1Char(' '))};
    qDebug() << " -  - -STARTING PREVIEW JOBS: " << args;
    pCore->currentDoc()->previewProgress(0);
    m_previewProcess.start(m_renderer, args);
    if (m_previewProcess.waitForStarted()) {
        qDebug() << " -  - -STARTING PREVIEW JOBS . . . STARTED";
    }
}

void PreviewManager::processEnded(int, QProcess::ExitStatus status)
{
    qDebug() << "// PROCESS IS FINISHED!!!";
    const QString sceneList = m_cacheDir.absoluteFilePath(QStringLiteral("preview.mlt"));
    QFile::remove(sceneList);
    if (status == QProcess::QProcess::CrashExit) {
        qDebug() << "// PROCESS CRASHED!!!!!!";
        pCore->currentDoc()->previewProgress(-1);
        if (workingPreview >= 0) {
            const QString fileName = QStringLiteral("%1.%2").arg(workingPreview).arg(m_extension);
            if (m_cacheDir.exists(fileName)) {
                m_cacheDir.remove(fileName);
            }
        }
    } else {
        pCore->currentDoc()->previewProgress(1000);
    }
    workingPreview = -1;
    emit m_controller->workingPreviewChanged();
}

void PreviewManager::slotProcessDirtyChunks()
{
    if (m_dirtyChunks.isEmpty()) {
        return;
    }
    invalidatePreviews(m_dirtyChunks);
    if (KdenliveSettings::autopreview()) {
        m_previewTimer.start();
    }
}

void PreviewManager::slotRemoveInvalidUndo(int ix)
{
    QMutexLocker lock(&m_previewMutex);
    if (m_undoDir.dirName() != QLatin1String("undo")) {
        // Make sure we delete correct folder
        return;
    }
    QStringList dirs = m_undoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool ok;
    for (const QString &dir : qAsConst(dirs)) {
        if (dir.toInt(&ok) >= ix && ok) {
            QDir tmp = m_undoDir;
            if (tmp.cd(dir)) {
                tmp.removeRecursively();
            }
        }
    }
}

void PreviewManager::invalidatePreview(int startFrame, int endFrame)
{
    if (m_previewTrack == nullptr) {
        return;
    }
    int chunkSize = KdenliveSettings::timelinechunks();
    int start = startFrame - startFrame % chunkSize;
    int end = endFrame - endFrame % chunkSize;

    std::sort(m_renderedChunks.begin(), m_renderedChunks.end());
    m_previewGatherTimer.stop();
    abortRendering();
    m_tractor->lock();
    bool chunksChanged = false;
    for (int i = start; i <= end; i += chunkSize) {
        if (m_renderedChunks.contains(i)) {
            int ix = m_previewTrack->get_clip_index_at(i);
            if (m_previewTrack->is_blank(ix)) {
                continue;
            }
            Mlt::Producer *prod = m_previewTrack->replace_with_blank(ix);
            delete prod;
            QVariant val(i);
            m_renderedChunks.removeAll(val);
            if (!m_dirtyChunks.contains(val)) {
                m_dirtyChunks << val;
                chunksChanged = true;
            }
        }
    }
    m_tractor->unlock();
    if (chunksChanged) {
        m_previewTrack->consolidate_blanks();
        emit m_controller->renderedChunksChanged();
        emit m_controller->dirtyChunksChanged();
    }
    m_previewGatherTimer.start();
}

void PreviewManager::reloadChunks(const QVariantList chunks)
{
    if (m_previewTrack == nullptr || chunks.isEmpty()) {
        return;
    }
    m_tractor->lock();
    for (const auto &ix : chunks) {
        if (m_previewTrack->is_blank_at(ix.toInt())) {
            QString fileName = m_cacheDir.absoluteFilePath(QStringLiteral("%1.%2").arg(ix.toInt()).arg(m_extension));
            fileName.prepend(QStringLiteral("avformat:"));
            Mlt::Producer prod(pCore->getCurrentProfile()->profile(), fileName.toUtf8().constData());
            if (prod.is_valid()) {
                // m_ruler->updatePreview(ix, true);
                prod.set("mlt_service", "avformat-novalidate");
                prod.set("mute_on_pause", 1);
                m_previewTrack->insert_at(ix.toInt(), &prod, 1);
            }
        }
    }
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
}

void PreviewManager::gotPreviewRender(int frame, const QString &file, int progress)
{
    if (m_previewTrack == nullptr) {
        return;
    }
    if (frame < 0) {
        pCore->currentDoc()->previewProgress(1000);
        return;
    }
    if (file.isEmpty() || progress < 0) {
        pCore->currentDoc()->previewProgress(progress);
        if (progress < 0) {
            pCore->displayMessage(i18n("Preview rendering failed, check your parameters. %1Show details...%2",
                                       QString("<a href=\"" + QString::fromLatin1(QUrl::toPercentEncoding(file)) + QStringLiteral("\">")),
                                       QStringLiteral("</a>")),
                                  MltError);
        }
        return;
    }
    if (m_previewTrack->is_blank_at(frame)) {
        Mlt::Producer prod(pCore->getCurrentProfile()->profile(), QString("avformat:%1").arg(file).toUtf8().constData());
        if (prod.is_valid()) {
            m_dirtyChunks.removeAll(frame);
            m_renderedChunks << frame;
            emit m_controller->renderedChunksChanged();
            prod.set("mlt_service", "avformat-novalidate");
            prod.set("mute_on_pause", 1);
            qDebug() << "|||| PLUGGING PREVIEW CHUNK AT: " << frame;
            m_tractor->lock();
            m_previewTrack->insert_at(frame, &prod, 1);
            m_previewTrack->consolidate_blanks();
            m_tractor->unlock();
            pCore->currentDoc()->previewProgress(progress);
            pCore->currentDoc()->setModified(true);
        } else {
            qCDebug(KDENLIVE_LOG) << "* * * INVALID PROD: " << file;
            corruptedChunk(frame, file);
        }
    } else {
        qCDebug(KDENLIVE_LOG) << "* * * NON EMPTY PROD: " << frame;
    }
}

void PreviewManager::corruptedChunk(int frame, const QString &fileName)
{
    emit abortPreview();
    m_previewProcess.waitForFinished();
    if (workingPreview >= 0) {
        workingPreview = -1;
        emit m_controller->workingPreviewChanged();
    }
    emit previewRender(0, m_errorLog, -1);
    m_cacheDir.remove(fileName);
    if (!m_dirtyChunks.contains(frame)) {
        m_dirtyChunks << frame;
        std::sort(m_dirtyChunks.begin(), m_dirtyChunks.end());
    }
}

int PreviewManager::setOverlayTrack(Mlt::Playlist *overlay)
{
    m_overlayTrack = overlay;
    m_overlayTrack->set("id", "timeline_overlay");
    reconnectTrack();
    return m_previewTrackIndex;
}

void PreviewManager::removeOverlayTrack()
{
    delete m_overlayTrack;
    m_overlayTrack = nullptr;
    reconnectTrack();
}

QPair<QStringList, QStringList> PreviewManager::previewChunks() const
{
    QStringList renderedChunks;
    QStringList dirtyChunks;
    for (const QVariant &frame : m_renderedChunks) {
        renderedChunks << frame.toString();
    }
    for (const QVariant &frame : m_dirtyChunks) {
        dirtyChunks << frame.toString();
    }
    return {renderedChunks, dirtyChunks};
}

bool PreviewManager::hasOverlayTrack() const
{
    return m_overlayTrack != nullptr;
}

bool PreviewManager::hasPreviewTrack() const
{
    return m_previewTrack != nullptr;
}

int PreviewManager::addedTracks() const
{
    if (m_previewTrack) {
        if (m_overlayTrack) {
            return 2;
        }
        return 1;
    } else if (m_overlayTrack) {
        return 1;
    }
    return -1;
}
