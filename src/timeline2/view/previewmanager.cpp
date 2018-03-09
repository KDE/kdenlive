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
#include "timeline2/view/timelinecontroller.h"

#include <KLocalizedString>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent>

PreviewManager::PreviewManager(TimelineController *controller, Mlt::Tractor *tractor)
    : QObject()
    , workingPreview(-1)
    , m_controller(controller)
    , m_tractor(tractor)
    , m_previewTrack(nullptr)
    , m_overlayTrack(nullptr)
    , m_previewTrackIndex(-1)
    , m_initialized(false)
    , m_abortPreview(false)
{
    m_previewGatherTimer.setSingleShot(true);
    m_previewGatherTimer.setInterval(200);
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
    connect(this, &PreviewManager::previewRender, this, &PreviewManager::gotPreviewRender);
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
    m_previewTrack = new Mlt::Playlist(*m_tractor->profile());
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
    for (const auto &frame : previewChunks) {
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
        m_controller->renderedChunksChanged();
    }
    if (!dirtyChunks.isEmpty()) {
        for (const auto &i : dirtyChunks) {
            if (!m_dirtyChunks.contains(i)) {
                m_dirtyChunks << i;
            }
        }
        m_controller->dirtyChunksChanged();
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
    m_controller->dirtyChunksChanged();
    m_controller->renderedChunksChanged();
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
        tk->set("id", "timeline_preview");
        increment++;
    }
    if (m_overlayTrack) {
        m_tractor->insert_track(*m_overlayTrack, m_previewTrackIndex + increment);
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(m_previewTrackIndex + increment));
        tk->set("hide", 2);
        tk->set("id", "timeline_overlay");
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
    qDebug() << "// DISCONNECTING PREV TK.............";
    m_previewTrackIndex = -1;
}

bool PreviewManager::loadParams()
{
    KdenliveDoc *doc = pCore->currentDoc();
    m_extension = doc->getDocumentProperty(QStringLiteral("previewextension"));
    m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), QString::SkipEmptyParts);

    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        doc->selectPreviewProfile();
        m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), QString::SkipEmptyParts);
        m_extension = doc->getDocumentProperty(QStringLiteral("previewextension"));
    }
    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        return false;
    }
    // Remove the r= and s= parameter (forcing framerate / frame size) as it causes rendering failure.
    // These parameters should be provided by MLT's profile
    for (int i = 0; i < m_consumerParams.count(); i++) {
        if (m_consumerParams.at(i).startsWith(QStringLiteral("r=")) || m_consumerParams.at(i).startsWith(QStringLiteral("s="))) {
            m_consumerParams.removeAt(i);
            i--;
        }
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
    bool timer = false;
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
        if (stackIx == stackMax - 1) {
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
            qSort(foundChunks);
            m_controller->dirtyChunksChanged();
            m_controller->renderedChunksChanged();
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

void PreviewManager::clearPreviewRange()
{
    m_previewGatherTimer.stop();
    abortPreview();
    m_tractor->lock();
    bool hasPreview = m_previewTrack != nullptr;
    for (const auto &ix : m_renderedChunks) {
        m_cacheDir.remove(QStringLiteral("%1.%2").arg(ix.toInt()).arg(m_extension));
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
    m_controller->renderedChunksChanged();
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
            if (!m_renderedChunks.contains(frame)) {
                m_dirtyChunks << frame;
            }
        } else {
            if (m_renderedChunks.contains(frame)) {
                toRemove << frame;
            } else {
                m_dirtyChunks.removeAll(frame);
            }
        }
    }
    if (add) {
        qDebug() << "CHUNKS CHANGED: " << m_dirtyChunks;
        m_controller->dirtyChunksChanged();
        if (!m_previewThread.isRunning() && KdenliveSettings::autopreview()) {
            m_previewTimer.start();
        }
    } else {
        // Remove processed chunks
        bool isRendering = m_previewThread.isRunning();
        m_previewGatherTimer.stop();
        abortPreview();
        m_tractor->lock();
        bool hasPreview = m_previewTrack != nullptr;
        for (int ix : toRemove) {
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
        m_tractor->unlock();
        if (isRendering || KdenliveSettings::autopreview()) {
            m_previewTimer.start();
        }
    }
}

void PreviewManager::abortRendering()
{
    if (!m_previewThread.isRunning()) {
        return;
    }
    m_abortPreview = true;
    emit abortPreview();
    m_previewThread.waitForFinished();
    // Re-init time estimation
    emit previewRender(0, QString(), 0);
}

void PreviewManager::startPreviewRender()
{
    if (m_renderedChunks.isEmpty() && m_dirtyChunks.isEmpty()) {
        m_controller->addPreviewRange(true);
    }
    if (!m_dirtyChunks.isEmpty()) {
        // Abort any rendering
        abortRendering();
        m_waitingThumbs.clear();
        const QString sceneList = QStringLiteral("xml:") + m_cacheDir.absoluteFilePath(QStringLiteral("preview.mlt"));
        pCore->getMonitor(Kdenlive::ProjectMonitor)->sceneList(m_cacheDir.absolutePath(), sceneList);
        // pCore->currentDoc()->saveMltPlaylist(sceneList);
        m_previewThread = QtConcurrent::run(this, &PreviewManager::doPreviewRender, sceneList);
    }
}

void PreviewManager::doPreviewRender(const QString &scene)
{
    int progress;
    int chunkSize = KdenliveSettings::timelinechunks();
    // initialize progress bar
    emit previewRender(0, QString(), 0);
    int ct = 0;
    qSort(m_dirtyChunks);
    while (!m_dirtyChunks.isEmpty()) {
        workingPreview = m_dirtyChunks.takeFirst().toInt();
        m_controller->workingPreviewChanged();
        ct++;
        QString fileName = QStringLiteral("%1.%2").arg(workingPreview).arg(m_extension);
        if (m_dirtyChunks.isEmpty()) {
            progress = 1000;
        } else {
            progress = (double)(ct) / (ct + m_dirtyChunks.count()) * 1000;
        }
        if (m_cacheDir.exists(fileName)) {
            // This chunk already exists
            emit previewRender(workingPreview, m_cacheDir.absoluteFilePath(fileName), progress);
            continue;
        }
        // Build rendering process
        QStringList args;
        args << scene;
        args << QStringLiteral("in=") + QString::number(workingPreview);
        args << QStringLiteral("out=") + QString::number(workingPreview + chunkSize - 1);
        args << QStringLiteral("-consumer") << QStringLiteral("avformat:") + m_cacheDir.absoluteFilePath(fileName);
        args << m_consumerParams;
        QProcess previewProcess;
        connect(this, &PreviewManager::abortPreview, &previewProcess, &QProcess::kill, Qt::DirectConnection);
        previewProcess.start(KdenliveSettings::rendererpath(), args);
        if (previewProcess.waitForStarted()) {
            previewProcess.waitForFinished(-1);
            if (previewProcess.exitStatus() != QProcess::NormalExit || previewProcess.exitCode() != 0) {
                // Something went wrong
                if (m_abortPreview) {
                    emit previewRender(0, QString(), 1000);
                } else {
                    emit previewRender(workingPreview, previewProcess.readAllStandardError(), -1);
                }
                // working chunk failed, re-add it to list
                m_dirtyChunks << workingPreview;
                qSort(m_dirtyChunks);
                QFile::remove(m_cacheDir.absoluteFilePath(fileName));
                break;
            } else {
                emit previewRender(workingPreview, m_cacheDir.absoluteFilePath(fileName), progress);
            }
        } else {
            emit previewRender(workingPreview, QString(), -1);
            break;
        }
    }
    // QFile::remove(scene);
    workingPreview = -1;
    m_controller->workingPreviewChanged();
    m_abortPreview = false;
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
    for (const QString &dir : dirs) {
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
    int chunkSize = KdenliveSettings::timelinechunks();
    int start = startFrame - startFrame % chunkSize;
    int end = endFrame - endFrame % chunkSize + chunkSize;

    qSort(m_renderedChunks);
    m_previewGatherTimer.stop();
    abortPreview();
    m_tractor->lock();
    bool hasPreview = m_previewTrack != nullptr;
    bool chunksChanged = false;
    for (int i = start; i <= end; i += chunkSize) {
        if (m_renderedChunks.contains(i) && hasPreview) {
            int ix = m_previewTrack->get_clip_index_at(i);
            if (m_previewTrack->is_blank(ix)) {
                continue;
            }
            Mlt::Producer *prod = m_previewTrack->replace_with_blank(ix);
            delete prod;
            m_renderedChunks.removeAll(QVariant(i));
            m_dirtyChunks << QVariant(i);
            chunksChanged = true;
        }
    }
    if (chunksChanged) {
        m_previewTrack->consolidate_blanks();
        m_controller->renderedChunksChanged();
        m_controller->dirtyChunksChanged();
    }
    m_tractor->unlock();
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
            const QString fileName = m_cacheDir.absoluteFilePath(QStringLiteral("%1.%2").arg(ix.toInt()).arg(m_extension));
            Mlt::Producer prod(*m_tractor->profile(), nullptr, fileName.toUtf8().constData());
            if (prod.is_valid()) {
                // m_ruler->updatePreview(ix, true);
                prod.set("mlt_service", "avformat-novalidate");
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
    m_tractor->lock();
    if (m_previewTrack->is_blank_at(frame)) {
        Mlt::Producer prod(*m_tractor->profile(), file.toUtf8().constData());
        if (prod.is_valid()) {
            m_renderedChunks << frame;
            m_controller->renderedChunksChanged();
            // m_ruler->updatePreview(frame, true, true);
            prod.set("mlt_service", "avformat-novalidate");
            m_previewTrack->insert_at(frame, &prod, 1);
        } else {
            qCDebug(KDENLIVE_LOG) << "* * * INVALID PROD: " << file;
        }
    } else {
        qCDebug(KDENLIVE_LOG) << "* * * NON EMPTY PROD: " << frame;
    }
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
    pCore->currentDoc()->previewProgress(progress);
    pCore->currentDoc()->setModified(true);
}

int PreviewManager::setOverlayTrack(Mlt::Playlist *overlay)
{
    m_overlayTrack = overlay;
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
