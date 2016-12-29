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
#include <QMutex>
#include <QTimer>
#include <QFuture>

class KdenliveDoc;
class CustomRuler;

namespace Mlt
{
class Tractor;
class Playlist;
}

/**
 * @namespace PreviewManager
 * @brief Handles timeline preview.
 * This manager creates an additional video track on top of the current timeline and renders
 * chunks (small video files of 25 frames) that are added on this track when rendrerd.
 * This allow us to get a preview with a smooth playback of our project.
 * Only the preview zone is rendered. Once defined, a preview zone shows as a red line below
 * the timeline ruler. As chunks are rendered, the zone turns to green.
 */

class PreviewManager : public QObject
{
    Q_OBJECT

public:
    explicit PreviewManager(KdenliveDoc *doc, CustomRuler *ruler, Mlt::Tractor *tractor);
    virtual ~PreviewManager();
    /** @brief: initialize base variables, return false if error. */
    bool initialize();
    /** @brief: a timeline operation caused changes to frames between startFrame and endFrame. */
    void invalidatePreview(int startFrame, int endFrame);
    /** @brief: after a small  delay (some operations trigger several invalidatePreview calls), take care of these invalidated chunks. */
    void invalidatePreviews(const QList<int> &chunks);
    /** @brief: user adds current timeline zone to the preview zone. */
    void addPreviewRange(bool add);
    /** @brief: Remove all existing previews. */
    void clearPreviewRange();
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
    void loadChunks(const QStringList &previewChunks, QStringList dirtyChunks, const QDateTime &documentDate);

private:
    KdenliveDoc *m_doc;
    CustomRuler *m_ruler;
    Mlt::Tractor *m_tractor;
    Mlt::Playlist *m_previewTrack;
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
    bool m_abortPreview;
    QList<int> m_waitingThumbs;
    QFuture <void> m_previewThread;
    /** @brief: After an undo/redo, if we have preview history, use it. */
    void reloadChunks(const QList<int> &chunks);

private slots:
    /** @brief: To avoid filling the hard drive, remove preview undo history after 5 steps. */
    void doCleanupOldPreviews();
    /** @brief: Start the real rendering process. */
    void doPreviewRender(const QString &scene);
    /** @brief: If user does an undo, then makes a new timeline operation, delete undo history of more recent stack . */
    void slotRemoveInvalidUndo(int ix);
    /** @brief: When the timer collecting invalid zones is done, process. */
    void slotProcessDirtyChunks();

public slots:
    /** @brief: Prepare and start rendering. */
    void startPreviewRender();
    /** @brief: A chunk has been created, notify ruler. */
    void gotPreviewRender(int frame, const QString &file, int progress);

signals:
    void abortPreview();
    void cleanupOldPreviews();
    void previewRender(int frame, const QString &file, int progress);
};

#endif

