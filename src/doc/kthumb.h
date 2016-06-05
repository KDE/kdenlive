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
#include <QDomElement>
#include <QFuture>

#include <QUrl>

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
class Producer;
class Frame;
}

class ClipManager;

typedef QMap <int, QMap <int, QByteArray> > audioByteArray;

class KThumb: public QObject
{
   Q_OBJECT 
public:

    explicit KThumb(ClipManager *clipManager, const QUrl &url, const QString &id, const QString &hash, QObject * parent = 0);
    ~KThumb();
    void setProducer(Mlt::Producer *producer);
    bool hasProducer() const;
    void clearProducer();
    void extractImage(const QList<int> &frames);
    QImage extractImage(int frame, int width, int height);
    /** @brief Request thumbnails for the frame range. */
    void queryIntraThumbs(const QSet <int> &missingFrames);
    void getThumb(int frame);

public:
    static QPixmap getImage(const QUrl &url, int width, int height = -1);
    static QPixmap getImage(const QUrl &url, int frame, int width, int height = -1);
    static QImage getFrame(Mlt::Producer *producer, int framepos, int displayWidth, int height);
    static QImage getFrame(Mlt::Frame *frame, int width, int height);
    /** @brief Calculates image variance, useful to know if a thumbnail is interesting. 
     *  @return an integer between 0 and 100. 0 means no variance, eg. black image while bigger values mean contrasted image
     * */
    static uint imageVariance(const QImage &image);

private slots:
    /** @brief Fetch all requested frames. */ 
    void slotGetIntraThumbs();

private:
    QUrl m_url;
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
    QImage getProducerFrame(int framepos, int displayWidth, int height);

signals:
    void thumbReady(int, const QImage&);
    void mainThumbReady(const QString &, const QPixmap&);
    void audioThumbReady(const audioByteArray&);
    /** @brief We have finished caching all requested thumbs. */
    void thumbsCached();
};

#endif
