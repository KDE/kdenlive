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

#ifndef PREVIEWMANAGER_H
#define PREVIEWMANAGER_H

#include "definitions.h"

#include <QDir>
#include <QFuture>
#include <QMutex>
#include <QProcess>
#include <QTimer>

class TimelineController;

namespace Mlt {
class Tractor;
class Playlist;
class Producer;
} // namespace Mlt

/**
 * @namespace PreviewManager
 * @brief Handles timeline preview.
 * This manager creates an additional video track on top of the current timeline and renders
 * chunks (small video files of 25 frames) that are added on this track when rendered.
 * This allow us to get a preview with a smooth playback of our project.
 * Only the preview zone is rendered. Once defined, a preview zone shows as a red line below
 * the timeline ruler. As chunks are rendered, the zone turns to green.
 */

class PreviewManager : public QObject
{
    Q_OBJECT

public:
    friend class TimelineController;

    explicit PreviewManager(TimelineController *controller, Mlt::Tractor *tractor);
    ~PreviewManager() override;
    /** @brief: initialize base variables, return false if error. */
    bool initialize();
    /** @brief: a timeline operation caused changes to frames between startFrame and endFrame. */
    void invalidatePreview(int startFrame, int endFrame);
    /** @brief: after a small  delay (some operations trigger several invalidatePreview calls), take care of these invalidated chunks. */
    void invalidatePreviews(const QVariantList chunks);
    /** @brief: user adds current timeline zone to the preview zone. */
    void addPreviewRange(const QPoint zone, bool add);
    /** @brief: Remove all existing previews. */
    void clearPreviewRange(bool resetZones);
    /** @brief: stops current rendering process. */
    void abortRendering();
    /** @brief: rendering parameters have changed, reload them. */
    bool loadParams();
    /** @brief: Create the preview track if not existing. */
    bool buildPreviewTrack();
    /** @brief: Delete the preview track. */
    void deletePreviewTrack();
    /** @brief: Whenever we save or render our project, we remove the preview track so it is not saved. */
    void reconnectTrack();
    /** @brief: After project save or render, re-add our preview track. */
    void disconnectTrack();
    /** @brief: Returns directory currently used to store the preview files. */
    const QDir getCacheDir() const;
    /** @brief: Load existing ruler chunks. */
    void loadChunks(QVariantList previewChunks, QVariantList dirtyChunks, const QDateTime &documentDate);
    int setOverlayTrack(Mlt::Playlist *overlay);
    /** @brief Remove the effect compare overlay track */
    void removeOverlayTrack();
    /** @brief The current preview chunk being processed, -1 if none */
    int workingPreview;
    /** @brief Returns the list of existing chunks */
    QPair<QStringList, QStringList> previewChunks() const;
    bool hasOverlayTrack() const;
    bool hasPreviewTrack() const;
    int addedTracks() const;

private:
    TimelineController *m_controller;
    Mlt::Tractor *m_tractor;
    Mlt::Playlist *m_previewTrack;
    Mlt::Playlist *m_overlayTrack;
    int m_previewTrackIndex;
    /** @brief: The kdenlive renderer app. */
    QString m_renderer;
    /** @brief: The kdenlive timeline preview process. */
    QProcess m_previewProcess;
    /** @brief: The directory used to store the preview files. */
    QDir m_cacheDir;
    /** @brief: The directory used to store undo history of preview files (child of m_cacheDir). */
    QDir m_undoDir;
    QMutex m_previewMutex;
    QStringList m_consumerParams;
    QString m_extension;
    /** @brief: Timer used to autostart preview rendering. */
    QTimer m_previewTimer;
    /** @brief: Since some timeline operations generate several invalidate calls, use a timer to get them all. */
    QTimer m_previewGatherTimer;
    bool m_initialized;
    QList<int> m_waitingThumbs;
    QFuture<void> m_previewThread;
    /** @brief: The count of chunks to process - to calculate job progress */
    int m_chunksToRender;
    /** @brief: The count of already processed chunks - to calculate job progress */
    int m_processedChunks;
    /** @brief: The render process output, useful in case of failure */
    QString m_errorLog;
    /** @brief: After an undo/redo, if we have preview history, use it. */
    void reloadChunks(const QVariantList chunks);
    /** @brief: A chunk failed to render, abort. */
    void corruptedChunk(int workingPreview, const QString &fileName);
    /** @brief: Re-enable timeline preview track. */
    void enable();
    /** @brief: Temporarily disable timeline preview track. */
    void disable();

private slots:
    /** @brief: To avoid filling the hard drive, remove preview undo history after 5 steps. */
    void doCleanupOldPreviews();
    /** @brief: Start the real rendering process. */
    void doPreviewRender(const QString &scene); // std::shared_ptr<Mlt::Producer> sourceProd);
    /** @brief: If user does an undo, then makes a new timeline operation, delete undo history of more recent stack . */
    void slotRemoveInvalidUndo(int ix);
    /** @brief: When the timer collecting invalid zones is done, process. */
    void slotProcessDirtyChunks();
    /** @brief: Process preview rendering output. */
    void receivedStderr();
    void processEnded(int, QProcess::ExitStatus status);

public slots:
    /** @brief: Prepare and start rendering. */
    void startPreviewRender();
    /** @brief: A chunk has been created, notify ruler. */
    void gotPreviewRender(int frame, const QString &file, int progress);

protected:
    QVariantList m_renderedChunks;
    QVariantList m_dirtyChunks;

signals:
    void abortPreview();
    void cleanupOldPreviews();
    void previewRender(int frame, const QString &file, int progress);
};

#endif
