/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PRODUCERQUEUE_H
#define PRODUCERQUEUE_H

#include "definitions.h"

#include <QMutex>
#include <QFuture>

class ClipController;
class BinController;

namespace Mlt
{
class Producer;
}

/**)
 * @class ProducerQueue
 * @brief A class building MLT producers / Clipcontrollers from url / xml info
 * @author Jean-Baptiste Mardelle
 */

class ProducerQueue : public QObject
{

    Q_OBJECT

public:
    explicit ProducerQueue(BinController *controller);
    ~ProducerQueue();

    /** @brief Force processing of clip with selected id. */
    void forceProcessing(const QString &id);
    /** @brief Are we currently processing clip with selected id. */
    bool isProcessing(const QString &id);
    /** @brief Make sure to close running threads before closing document */
    void abortOperations();

private:
    QMutex m_infoMutex;
    QList<requestClipInfo> m_requestList;
    /** @brief The ids of the clips that are currently being loaded for info query */
    QStringList m_processingClipId;
    QFuture <void> m_infoThread;
    BinController *m_binController;
    ClipType getTypeForService(const QString &id, const QString &path) const;
    /** @brief Pass xml values to an MLT producer at build time */
    void processProducerProperties(Mlt::Producer *prod, const QDomElement &xml);

public slots:
    /** @brief Requests the file properties for the specified URL (will be put in a queue list)
    @param xml The xml parameters for the clip
    @param clipId The clip Id string
    @param imageHeight The height (in pixels) of the returned thumbnail (height of a treewidgetitem in projectlist)
    @param replaceProducer If true, the MLT producer will be recreated */
    void getFileProperties(const QDomElement &xml, const QString &clipId, int imageHeight, bool replaceProducer = true);

    /** @brief Processing of this clip is over, producer was set on clip, remove from list. */
    void slotProcessingDone(const QString &id);

private slots:
    /** @brief Process the clip info requests (in a separate thread). */
    void processFileProperties();
    /** @brief A clip with multiple video streams was found, ask what to do. */
    void slotMultiStreamProducerFound(const QString &path, const QList<int> &audio_list, const QList<int> &video_list, stringMap data);

signals:
    /** @brief The renderer received a reply to a getFileProperties request. */
    void gotFileProperties(requestClipInfo, ClipController *);

    /** @brief The renderer received a reply to a getImage request. */
    void replyGetImage(const QString &, const QImage &, bool fromFile = false);
    /** @brief A proxy clip is missing, ask for creation. */
    void requestProxy(const QString &);
    void infoProcessingFinished();
    /** @brief First clip does not match profect profile, switch. */
    void switchProfile(MltVideoProfile profile, const QString &id, const QDomElement &xml);
    /** @brief The clip is not valid, should be removed from project. */
    void removeInvalidClip(const QString &, bool replaceProducer, const QString &message = QString());
    /** @brief The proxy is not valid, should be deleted.
     *  @param id The original clip's id
     *  @param durationError Should be set to true if the proxy failed because it has not same length as original clip
     */
    void removeInvalidProxy(const QString &id, bool durationError);
    /** @brief A multiple stream clip was found. */
    void multiStreamFound(const QString &, QList<int>, QList<int>, stringMap data);
    void addClip(const QString &, const QMap<QString, QString> &);
    /** @brief A clip changed whithout need to replace produder (for example clip), just ask refresh. */
    void refreshTimelineProducer(const QString);
};

#endif
