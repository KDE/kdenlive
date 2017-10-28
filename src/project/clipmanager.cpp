/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "clipmanager.h"
#include "bin/bin.h"
#include "bin/bincommands.h"
#include "core.h"
#include "dialogs/slideshowclip.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "mltcontroller/bincontroller.h"
#include "mltcontroller/clipcontroller.h"
#include "project/projectmanager.h"
#include "renderer.h"
#include "titler/titledocument.h"

#include <mlt++/Mlt.h>

#include <KIO/JobUiDelegate>
#include <KIO/MkdirJob>
#include <KJobWidgets/KJobWidgets>
#include <klocalizedstring.h>

#include <QApplication>
#include <QCheckBox>
#include <QGraphicsItemGroup>
#include <QtConcurrent>

ClipManager::ClipManager(KdenliveDoc *doc)
    : QObject()
    , m_audioThumbsQueue()
    , m_doc(doc)
    , m_abortThumb(false)
    , m_closing(false)
    , m_abortAudioThumb(false)
{
}

ClipManager::~ClipManager()
{
    m_closing = true;
    m_abortThumb = true;
    m_abortAudioThumb = true;
    m_thumbsThread.waitForFinished();
    m_audioThumbsThread.waitForFinished();
    m_thumbsMutex.lock();
    m_requestedThumbs.clear();
    m_audioThumbsQueue.clear();
    m_thumbsMutex.unlock();
}

void ClipManager::clear()
{
    m_abortThumb = true;
    m_abortAudioThumb = true;
    m_thumbsThread.waitForFinished();
    m_audioThumbsThread.waitForFinished();
    m_thumbsMutex.lock();
    m_requestedThumbs.clear();
    m_audioThumbsQueue.clear();
    m_thumbsMutex.unlock();
    m_abortThumb = false;
    m_abortAudioThumb = false;
    m_folderList.clear();
    m_modifiedClips.clear();
}

void ClipManager::slotRequestThumbs(const QString &id, const QList<int> &frames)
{
    m_thumbsMutex.lock();
    for (int frame : frames) {
        m_requestedThumbs.insertMulti(id, frame);
    }
    m_thumbsMutex.unlock();
    if (!m_thumbsThread.isRunning() && !m_abortThumb) {
        m_thumbsThread = QtConcurrent::run(this, &ClipManager::slotGetThumbs);
    }
}

void ClipManager::stopThumbs(const QString &id)
{
    if (m_closing || (m_requestedThumbs.isEmpty() && m_processingThumbId != id && m_audioThumbsQueue.isEmpty() && m_processingAudioThumbId != id)) {
        return;
    }
    // Abort video thumbs for this clip
    m_abortThumb = true;
    m_thumbsThread.waitForFinished();
    m_thumbsMutex.lock();
    m_requestedThumbs.remove(id);
    m_audioThumbsQueue.removeAll(id);
    m_thumbsMutex.unlock();
    m_abortThumb = false;

    // Abort audio thumbs for this clip
    if (m_processingAudioThumbId == id) {
        m_abortAudioThumb = true;
        m_audioThumbsThread.waitForFinished();
        m_abortAudioThumb = false;
    }

    if (!m_thumbsThread.isRunning() && !m_requestedThumbs.isEmpty()) {
        m_thumbsThread = QtConcurrent::run(this, &ClipManager::slotGetThumbs);
    }
}

void ClipManager::slotGetThumbs()
{
    QMap<QString, int>::const_iterator i;

    while (!m_requestedThumbs.isEmpty() && !m_abortThumb) {
        m_thumbsMutex.lock();
        i = m_requestedThumbs.constBegin();
        m_processingThumbId = i.key();
        QList<int> values = m_requestedThumbs.values(m_processingThumbId);
        m_requestedThumbs.remove(m_processingThumbId);
        // TODO int thumbType = 0; // 0 = timeline thumb, 1 = project clip zone thumb, 2 = clip properties thumb
        if (m_processingThumbId.startsWith(QLatin1String("?"))) {
            // if id starts with ?, it means the request comes from a clip property widget
            // TODO thumbType = 2;
            m_processingThumbId.remove(0, 1);
        }
        if (m_processingThumbId.startsWith(QLatin1String("#"))) {
            // if id starts with #, it means the request comes from project tree
            // TODO thumbType = 1;
            m_processingThumbId.remove(0, 1);
        }
        m_thumbsMutex.unlock();
        qSort(values);
        // TODO
        /*
        DocClipBase *clip = getClipById(m_processingThumbId);
        if (!clip) continue;
        max = m_requestedThumbs.size() + values.count();
        // keep in sync with declaration un projectitem.cpp and subprojectitem.cpp
        int minHeight = qMax(38, QFontMetrics(QApplication::font()).lineSpacing() * 2);
        while (!values.isEmpty() && clip->thumbProducer() && !m_abortThumb) {
            int pos = values.takeFirst();
            switch (thumbType) {
            case 1:
                clip->thumbProducer()->getGenericThumb(pos, minHeight, thumbType);
                break;
            case 2:
                clip->thumbProducer()->getGenericThumb(pos, 180, thumbType);
                break;
            default:
                clip->thumbProducer()->getThumb(pos);
            }
            done++;
            if (max > 3) emit displayMessage(i18n("Loading thumbnails"), 100 * done / max);
        }
        */
    }
    m_processingThumbId.clear();
    emit displayMessage(QString(), -1);
}

void ClipManager::slotAddCopiedClip(KIO::Job *, const QUrl &, const QUrl &dst)
{
    pCore->bin()->droppedUrls(QList<QUrl>() << dst);
}

int ClipManager::lastClipId() const
{
    return pCore->bin()->lastClipId();
}

/*
void ClipManager::slotClipMissing(const QString &path)
{
    qCDebug(KDENLIVE_LOG) << "// CLIP: " << path << " WAS MISSING";
    //TODO
    const QList<DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != nullptr) emit missingClip(clip->getId());
    }
}

void ClipManager::slotClipAvailable(const QString &path)
{
    qCDebug(KDENLIVE_LOG) << "// CLIP: " << path << " WAS ADDED";
    //TODO
    const QList<DocClipBase *> list = getClipByResource(path);
    for (int i = 0; i < list.count(); ++i) {
        DocClipBase *clip = list.at(i);
        if (clip != nullptr) emit availableClip(clip->getId());
    }
}
*/

void ClipManager::projectTreeThumbReady(const QString &id, int frame, const QImage &img, int type)
{
    switch (type) {
    case 2:
        emit gotClipPropertyThumbnail(id, img);
        break;
    default:
        emit thumbReady(id, frame, img);
    }
}
