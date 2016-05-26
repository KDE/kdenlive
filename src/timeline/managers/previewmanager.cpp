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
#include "../customruler.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"

#include <QtConcurrent>
#include <QStandardPaths>
#include <QProcess>

PreviewManager::PreviewManager(KdenliveDoc *doc, CustomRuler *ruler, Mlt::Tractor *tractor) : QObject()
    , m_doc(doc)
    , m_ruler(ruler)
    , m_tractor(tractor)
    , m_previewTrack(NULL)
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
        /*if (m_undoDir.dirName() == QLatin1String("undo"))
            m_undoDir.removeRecursively();
        if (m_doc->url().isEmpty() || m_cacheDir.entryList(QDir::Files).count() == 0) {
            if (m_cacheDir.dirName() == m_doc->getDocumentProperty(QStringLiteral("documentid")))
                m_cacheDir.removeRecursively();
        }*/
    }
    delete m_previewTrack;
}

bool PreviewManager::initialize()
{
    QString documentId = m_doc->getDocumentProperty(QStringLiteral("documentid"));
    m_initialized = true;
    if (documentId.isEmpty() || documentId.toLong() == 0) {
        // Something is wrong, documentId should be a number (ms since epoch), abort
        return false;
    }
    QString cacheDir = m_doc->getDocumentProperty(QStringLiteral("cachedir"));
    if (!cacheDir.isEmpty() && QFile::exists(cacheDir)) {
        m_cacheDir = QDir(cacheDir);
    } else {
        m_cacheDir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
        m_cacheDir.mkdir(documentId);
        if (!m_cacheDir.cd(documentId)) {
            return false;
        }
    }
    if (m_cacheDir.dirName() != documentId || (!m_cacheDir.exists("undo") && !m_cacheDir.mkdir("undo"))) {
        // TODO: cannot create undo folder, abort
        return false;
    }
    if (!loadParams()) {
        return false;
    }
    m_doc->setDocumentProperty(QStringLiteral("cachedir"), m_cacheDir.absolutePath());
    m_undoDir = QDir(m_cacheDir.absoluteFilePath("undo"));
    connect(this, &PreviewManager::cleanupOldPreviews, this, &PreviewManager::doCleanupOldPreviews);
    connect(m_doc, &KdenliveDoc::removeInvalidUndo, this, &PreviewManager::slotRemoveInvalidUndo, Qt::DirectConnection);
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
    if (m_previewTrack)
        return false;
    // Create overlay track
    m_previewTrack = new Mlt::Playlist(*m_tractor->profile());
    int trackIndex = m_tractor->count();
    m_tractor->lock();
    m_tractor->insert_track(*m_previewTrack, trackIndex);
    Mlt::Producer *tk = m_tractor->track(trackIndex);
    tk->set("hide", 2);
    delete tk;
    m_tractor->unlock();
    return true;
}

const QDir PreviewManager::getCacheDir() const
{
    return m_cacheDir;
}

void PreviewManager::reconnectTrack()
{
    if (m_previewTrack) {
        m_tractor->insert_track(*m_previewTrack, m_tractor->count());
    }
}

void PreviewManager::disconnectTrack()
{
    if (m_previewTrack) {
        m_tractor->remove_track(m_tractor->count() - 1);
    }
}

bool PreviewManager::loadParams()
{
    m_extension= m_doc->getDocumentProperty(QStringLiteral("previewextension"));
    m_consumerParams = m_doc->getDocumentProperty(QStringLiteral("previewparameters")).split(" ");

    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        m_doc->selectPreviewProfile();
        m_consumerParams = m_doc->getDocumentProperty(QStringLiteral("previewparameters")).split(" ");
        m_extension= m_doc->getDocumentProperty(QStringLiteral("previewextension"));
    }
    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        return false;
    }
    m_consumerParams << "an=1";
    return true;
}

void PreviewManager::invalidatePreviews(QList <int> chunks)
{
    // We are not at the bottom of undo stack, chunks have already been archived previously
    QMutexLocker lock(&m_previewMutex);
    bool timer = false;
    if (m_previewTimer.isActive()) {
        m_previewTimer.stop();
        timer = true;
    }
    int stackIx = m_doc->commandStack()->index();
    int stackMax = m_doc->commandStack()->count();
    if (stackIx == stackMax && !m_undoDir.exists(QString::number(stackIx - 1))) {
        // We just added a new command in stack, archive existing chunks
        int ix = stackIx - 1;
        m_undoDir.mkdir(QString::number(ix));
        bool foundPreviews = false;
        foreach(int i, chunks) {
            QString current = QString("%1.%2").arg(i).arg(m_extension);
            if (m_cacheDir.rename(current, QString("undo/%1/%2").arg(ix).arg(current))) {
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
                foreach(int i, chunks) {
                    QString current = QString("%1.%2").arg(i).arg(m_extension);
                    if (m_cacheDir.rename(current, QString("undo/%1/%2").arg(stackMax).arg(current))) {
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
        QList <int> foundChunks;
        foreach(int i, chunks) {
            QString cacheFileName = QString("%1.%2").arg(i).arg(m_extension);
            if (!lastUndo) {
                m_cacheDir.remove(cacheFileName);
            }
            if (moveFile) {
                if (QFile::copy(tmpDir.absoluteFilePath(cacheFileName), m_cacheDir.absoluteFilePath(cacheFileName))) {
                    foundChunks << i;
                }
            }
        }
        qSort(foundChunks);
        reloadChunks(foundChunks);
    }
    m_doc->setModified(true);
    if (timer)
        m_previewTimer.start();
}

void PreviewManager::doCleanupOldPreviews()
{
    if (m_undoDir.dirName() != QLatin1String("undo"))
        return;
    QStringList dirs = m_undoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    qSort(dirs);
    /*
    while (dirs.count() > 5) {
        QString dir = dirs.takeFirst();
        QDir tmp = m_undoDir;
        if (tmp.cd(dir)) {
            tmp.removeRecursively();
        }
    }*/
}

void PreviewManager::addPreviewRange(bool add)
{
    QPoint p = m_doc->zone();
    int chunkSize = KdenliveSettings::timelinechunks();
    int startChunk = p.x() / chunkSize;
    int endChunk = rintl(p.y() / chunkSize);
    QList <int> frames;
    for (int i = startChunk; i <= endChunk; i++) {
        frames << i * chunkSize;
    }
    QList <int> toProcess = m_ruler->addChunks(frames, add);
    if (toProcess.isEmpty())
        return;
    if (add) {
        if (m_previewThread.isRunning()) {
            // just add required frames to current rendering job
            m_waitingThumbs << toProcess;
        } else if (KdenliveSettings::autopreview())
            m_previewTimer.start();
    } else {
        // Remove processed chunks
        foreach(int ix, toProcess) {
            m_cacheDir.remove(QString("%1.%2").arg(ix).arg(m_extension));
        }
    }
}

void PreviewManager::abortRendering()
{
    if (!m_previewThread.isRunning())
        return;
    m_abortPreview = true;
    emit abortPreview();
    m_previewThread.waitForFinished();
    // Re-init time estimation
    emit previewRender(0, QString(), 0);
}

void PreviewManager::startPreviewRender()
{
    if (!m_ruler->hasPreviewRange()) {
        addPreviewRange(true);
    }
    QList <int> chunks = m_ruler->getDirtyChunks();
    if (!chunks.isEmpty()) {
        // Abort any rendering
        abortRendering();
        m_waitingThumbs.clear();
        const QString sceneList = m_cacheDir.absoluteFilePath(QStringLiteral("preview.mlt"));
        m_doc->saveMltPlaylist(sceneList);
        m_waitingThumbs = chunks;
        m_previewThread = QtConcurrent::run(this, &PreviewManager::doPreviewRender, sceneList);
    }
}

void PreviewManager::doPreviewRender(QString scene)
{
    int progress;
    int chunkSize = KdenliveSettings::timelinechunks();
    // initialize progress bar
    emit previewRender(0, QString(), 0);
    int ct = 0;
    qSort(m_waitingThumbs);
    while (!m_waitingThumbs.isEmpty()) {
        int i = m_waitingThumbs.takeFirst();
        ct++;
        QString fileName = QString("%1.%2").arg(i).arg(m_extension);
        if (m_waitingThumbs.isEmpty()) {
            progress = 1000;
        } else {
            progress = (double) (ct) / (ct + m_waitingThumbs.count()) * 1000;
        }
        if (m_cacheDir.exists(fileName)) {
            // This chunk already exists
            emit previewRender(i, m_cacheDir.absoluteFilePath(fileName), progress);
            continue;
        }
        // Build rendering process
        QStringList args;
        args << scene;
        args << "in=" + QString::number(i);
        args << "out=" + QString::number(i + chunkSize - 1);
        args << "-consumer" << "avformat:" + m_cacheDir.absoluteFilePath(fileName);
        args << m_consumerParams;
        QProcess previewProcess;
        connect(this, SIGNAL(abortPreview()), &previewProcess, SLOT(kill()), Qt::DirectConnection);
        previewProcess.start(KdenliveSettings::rendererpath(), args);
        if (previewProcess.waitForStarted()) {
            previewProcess.waitForFinished(-1);
            if (previewProcess.exitStatus() != QProcess::NormalExit || previewProcess.exitCode() != 0) {
                // Something went wrong
                if (m_abortPreview) {
                    emit previewRender(0, QString(), 1000);
                } else {
                    qDebug()<<"+++++++++\n++ ERROR  ++\n++++++";
                    emit previewRender(i, QString(), -1);
                }
                QFile::remove(m_cacheDir.absoluteFilePath(fileName));
                break;
            } else {
                emit previewRender(i, m_cacheDir.absoluteFilePath(fileName), progress);
            }
        } else {
            emit previewRender(i, QString(), -1);
            break;
        }
    }
    QFile::remove(scene);
    m_abortPreview = false;
}

void PreviewManager::slotProcessDirtyChunks()
{
    QList <int> chunks = m_ruler->getDirtyChunks();
    if (chunks.isEmpty())
        return;
    invalidatePreviews(chunks);
    m_ruler->updatePreviewDisplay(chunks.first(), chunks.last());
    if (KdenliveSettings::autopreview())
        m_previewTimer.start();
}

void PreviewManager::slotRemoveInvalidUndo(int ix)
{
    QMutexLocker lock(&m_previewMutex);
    if (m_undoDir.dirName() != QLatin1String("undo")) {
        // Make sure we delete correct folder
        return;
    }
    QStringList dirs = m_undoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    qSort(dirs);
    /*
    foreach(const QString dir, dirs) {
        if (dir.toInt() >= ix) {
            QDir tmp = m_undoDir;
            if (tmp.cd(dir)) {
                tmp.removeRecursively();
            }
        }
    }*/
}

void PreviewManager::invalidatePreview(int startFrame, int endFrame)
{
    int chunkSize = KdenliveSettings::timelinechunks();
    int start = startFrame / chunkSize;
    int end = lrintf(endFrame / chunkSize);
    start *= chunkSize;
    end *= chunkSize;
    if (!m_ruler->isUnderPreview(start, end)) {
        return;
    }
    m_previewGatherTimer.stop();
    abortPreview();
    m_tractor->lock();
    for (int i = start; i <=end; i+= chunkSize) {
        if (m_ruler->updatePreview(i, false)) {
            int ix = m_previewTrack->get_clip_index_at(i);
            if (m_previewTrack->is_blank(ix))
                continue;
            Mlt::Producer *prod = m_previewTrack->replace_with_blank(ix);
            delete prod;
        }
    }
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
    m_previewGatherTimer.start();
}

void PreviewManager::reloadChunks(QList <int> chunks)
{
    m_tractor->lock();
    foreach(int ix, chunks) {
        if (m_previewTrack->is_blank_at(ix)) {
            const QString fileName = m_cacheDir.absoluteFilePath(QString("%1.%2").arg(ix).arg(m_extension));
            Mlt::Producer prod(*m_tractor->profile(), 0, fileName.toUtf8().constData());
            if (prod.is_valid()) {
                m_ruler->updatePreview(ix, true);
                prod.set("mlt_service", "avformat-novalidate");
                m_previewTrack->insert_at(ix, &prod, 1);
            }
        }
    }
    m_ruler->updatePreviewDisplay(chunks.first(), chunks.last());
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
}

void PreviewManager::gotPreviewRender(int frame, const QString &file, int progress)
{
    if (file.isEmpty()) {
        m_doc->previewProgress(progress);
        return;
    }
    m_tractor->lock();
    if (m_previewTrack->is_blank_at(frame)) {
        Mlt::Producer prod(*m_tractor->profile(), 0, file.toUtf8().constData());
        if (prod.is_valid()) {
            m_ruler->updatePreview(frame, true, true);
            prod.set("mlt_service", "avformat-novalidate");
            m_previewTrack->insert_at(frame, &prod, 1);
        }
    }
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
    m_doc->previewProgress(progress);
    m_doc->setModified(true);
}
