/***************************************************************************
                         kthumb.h  -  description
                            -------------------
   begin                : Fri Nov 22 2002
   copyright            : (C) 2002 by Jason Wood
   email                : jasonwood@blueyonder.co.uk
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KTHUMB_H
#define KTHUMB_H

#include <QPixmap>
#include <QFile>
#include <QDomElement>
#include <QFuture>

#include <KUrl>
#include <kdeversion.h>

#include <mlt++/Mlt.h>


/**KRender encapsulates the client side of the interface to a renderer.
From Kdenlive's point of view, you treat the KRender object as the
renderer, and simply use it as if it was local. Calls are asyncrhonous -
you send a call out, and then receive the return value through the
relevant signal that get's emitted once the call completes.
  *@author Jason Wood
  */


namespace Mlt
{
class Miracle;
class Consumer;
class Producer;
class Frame;
class Profile;
};

class ClipManager;

typedef QMap <int, QMap <int, QByteArray> > audioByteArray;

class KThumb: public QObject
{
Q_OBJECT public:


    KThumb(ClipManager *clipManager, KUrl url, const QString &id, const QString &hash, QObject * parent = 0);
    ~KThumb();
    void setProducer(Mlt::Producer *producer);
    bool hasProducer() const;
    void clearProducer();
    void updateThumbUrl(const QString &hash);
    void extractImage(QList <int> frames);
    QImage extractImage(int frame, int width, int height);
#if KDE_IS_VERSION(4,5,0)
    /** @brief Request thumbnails for the frame range. */
    void queryIntraThumbs(QList <int> missingFrames);
    /** @brief Query cached thumbnail. */
    QImage findCachedThumb(const QString &path);
#endif
    void getThumb(int frame);
    void getGenericThumb(int frame, int height, int type);

public slots:
    void updateClipUrl(KUrl url, const QString &hash);
    static QPixmap getImage(KUrl url, int width, int height);
//    static QPixmap getImage(QDomElement xml, int frame, int width, int height);
    /* void getImage(KUrl url, int frame, int width, int height);
     void getThumbs(KUrl url, int startframe, int endframe, int width, int height);*/
    void slotCreateAudioThumbs();
    static QPixmap getImage(KUrl url, int frame, int width, int height);
    static QImage getFrame(Mlt::Producer *producer, int framepos, int frameWidth, int displayWidth, int height);
    static QImage getFrame(Mlt::Frame *frame, int frameWidth, int displayWidth, int height);
    /** @brief Calculates image variance, useful to know if a thumbnail is interesting. 
     *  @return an integer between 0 and 100. 0 means no variance, eg. black image while bigger values mean contrasted image
     * */
    static uint imageVariance(QImage image);

private slots:
#if KDE_IS_VERSION(4,5,0)
    /** @brief Fetch all requested frames. */ 
    void slotGetIntraThumbs();
#endif

private:
    KUrl m_url;
    QString m_thumbFile;
    double m_dar;
    double m_ratio;
    Mlt::Producer *m_producer;
    ClipManager *m_clipManager;
    QString m_id;
    /** @brief Controls the intra frames thumbnails process (cached thumbnails). */
    QFuture<void> m_intra;
    /** @brief List of frame numbers from which we want to extract thumbnails. */
    QList <int> m_intraFramesQueue;
    QMutex m_mutex;
    QImage getProducerFrame(int framepos, int frameWidth, int displayWidth, int height);

signals:
    void thumbReady(int, QImage);
    void mainThumbReady(const QString &, QPixmap);
    void audioThumbReady(const audioByteArray&);
    /** @brief We have finished caching all requested thumbs. */
    void thumbsCached();
};

#endif
