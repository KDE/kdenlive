/***************************************************************************
                         krender.h  -  description
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

#include <qobject.h>
#include <qpixmap.h>
#include <qfile.h>
#include <qthread.h>

#include <kurl.h>


/**KRender encapsulates the client side of the interface to a renderer.
From Kdenlive's point of view, you treat the KRender object as the
renderer, and simply use it as if it was local. Calls are asyncrhonous -
you send a call out, and then recieve the return value through the
relevant signal that get's emitted once the call completes.
  *@author Jason Wood
  */


namespace Mlt {
    class Miracle;
    class Consumer;
    class Producer;
};


    class MyThread : public QThread {

    public:
        virtual void run();
	void init(KURL url, QString target, double frame, double frameLength, int frequency, int channels, int arrayWidth);
	bool isWorking();
	bool stop_me;

    private:
	QFile f;
	KURL m_url;
	double m_frame;
	double m_frameLength;
	int m_frequency;
	int m_channels;
	int m_arrayWidth;
	bool m_isWorking;
    };


class KThumb:public QObject {
  Q_OBJECT public:


     KThumb(QObject * parent = 0, const char *name = 0);
    ~KThumb();

public slots:
	void getImage(KURL url, int frame, int width, int height);
	void stopAudioThumbs();
	void removeAudioThumb();
	void getAudioThumbs(KURL url, int channel, double frame, double frameLength, int arrayWidth);

private:
	MyThread thumbProducer;
	KURL m_url;
	QString m_thumbFile;

signals:
	void thumbReady(int frame, QPixmap pm);
	void audioThumbReady(QMap <int, QMap <int, QByteArray> >);
};

#endif
