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

#ifndef KRENDER_H
#define KRENDER_H

#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <qptrlist.h>
#include <qvaluestack.h>

#include <kurl.h>

#ifdef ENABLE_FIREWIRE
#include <libiec61883/iec61883.h>
#endif

#include "gentime.h"
#include "docclipref.h"
#include "effectdesc.h"
#include "effectparamdescfactory.h"

/**KRender encapsulates the client side of the interface to a renderer.
From Kdenlive's point of view, you treat the KRender object as the
renderer, and simply use it as if it was local. Calls are asyncrhonous -
you send a call out, and then recieve the return value through the
relevant signal that get's emitted once the call completes.
  *@author Jason Wood
  */

class KRender;
class AVFormatDescCodecList;
class EffectParamDesc;
class QPixmap;

namespace Mlt {
    class Consumer;
    class Playlist;
    class Tractor;
    class Frame;
    class Producer;
    class Filter;
};


struct StackValue {
    QString element;
	/** A function pointer to the relevant method that should parse tagOpen events */
     bool(KRender::*funcStartElement) (const QString & localName,
	const QString & qName, const QXmlAttributes & atts);
	/** A function pointer to the relevant method that should parse tagClose events */
     bool(KRender::*funcEndElement) (const QString & localName,
	const QString & qName);
};

class KRender:public QObject {
  Q_OBJECT public:

    enum FailStates { OK = 0,
	APP_NOEXIST
    };

     KRender(const QString & rendererName, QWidget *parent = 0, const char *name = 0);
    ~KRender();

	/** Wraps the VEML command of the same name; requests that the renderer
	should create a video window. If show is true, then the window should be
	displayed, otherwise it should be hidden. KRender will emit the signal
	replyCreateVideoXWindow() once the renderer has replied. */
    void createVideoXWindow(WId winid, WId externalMonitor);
	/** Seeks the renderer clip to the given time. */
    void seek(GenTime time);
    
    QPixmap getVideoThumbnail(KURL url, int frame, int width, int height);
    QPixmap getImageThumbnail(KURL url, int width, int height);

	/** Return thumbnail for color clip */
    void getImage(int id, QString color, int width, int height);

    QPixmap frameThumbnail(Mlt::Frame *frame, int width, int height, bool border = false);
    
    /** Return thumbnail for image clip */
    void getImage(KURL url, int width, int height);

	/** Requests a particular frame from the given file. 
	 * 
	 * The pixmap will be returned by emitting the replyGetImage() signal.
	 * */
    void getImage(KURL url, int frame, int width, int height);

	/** Wraps the VEML command of the same name. Sets the current scene list to
	be list. */
    void setSceneList(QDomDocument list, bool resetPosition = true);

	/** Wraps the VEML command of the same name. Tells the renderer to
	play the current scene at the speed specified, relative to normal
	playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
	backwards. Does not specify start/stop times for playback.*/
    void play(double speed);
	/** stop playing */
    void stop(const GenTime & startTime);
    void setVolume(double volume);

    QPixmap extractFrame(int frame_position, int width, int height);
	/** Wraps the VEML command of the same name. Tells the renderer to
	play the current scene at the speed specified, relative to normal
	playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
	backwards. Specifes the start/stop times for playback.*/
    void play(double speed, const GenTime & startTime);
    void play(double speed, const GenTime & startTime,
	const GenTime & stopTime);
	/** Wraps the VEML command of the same name - render the currently
	specified sceneList - set with setSceneList() - to the document
	name specified. */
    void render(const KURL & url);

	/** Returns the description of this renderer */
    QString description();

	/** Returns the name of the renderer. */
    const QString & rendererName() const;

	/** Returns the speed at which the renderer is currently playing, 0.0 if the renderer is
	not playing anything. */
    double playSpeed();

	/** Returns the current seek position of the renderer. */
    const GenTime & seekPosition() const;

    void emitFrameNumber(double position);
    void emitConsumerStopped();

    /** Gives the aspect ratio of the consumer */
    double consumerRatio() const;

    /** Gives the aspect ratio of the consumer */
    void askForRefresh();
    
    /** Save current producer frame as image */
    void exportCurrentFrame(KURL url, bool notify);

    /** Turn on or off on screen display */
    void refreshDisplay();
    /** returns the current scenelist */
    QDomDocument sceneList() const;
    int resetRendererProfile(char * profile);
    bool isBlocked;
 
  private:			// Private attributes & methods
	/** The name of this renderer - useful to identify the renderes by what they do - e.g. background rendering, workspace monitor, etc... */
     QString m_name;
     Mlt::Consumer * m_mltConsumer;
     Mlt::Producer * m_mltProducer;
     Mlt::Producer *m_mltTextProducer;
     Mlt::Filter *m_osdInfo;
     double m_framePosition;
     double m_fps;
     uint m_monitorId;
     bool m_isStopped;
     bool m_generateScenelist;

	/** Holds the path to on screen display profile */
     QString m_osdProfile;
     
     QTimer *refreshTimer;
     QTimer *osdTimer;
     KURL m_exportedFile;
     int exportDuration, firstExportFrame, lastExportFrame;

	/** Holds the scenelist to be sent, if pending. */
    QDomDocument m_sceneList;

	/** A human-readable description of this renderer. */
    QString m_description;
    int m_winid;
    int m_externalwinid;
	/** The actually seek command, private so people can't avoid the buffering of multiple seek commands. */
    void sendSeekCommand(GenTime time);

	/** Sets the description of this renderer to desc. */
    void setDescription(const QString & description);
    void openMlt();
    void closeMlt();
    
#ifdef ENABLE_FIREWIRE
    void dv_transmit( raw1394handle_t handle, FILE *f, int channel);
#endif


    private slots:		// Private slots
	/** refresh monitor display */
        void refresh();
	void slotOsdTimeout();
	void restartConsumer();

     signals:			// Signals
	/** This signal is emitted once a reply to createVideoXWidow() has been recieved. */
    void replyCreateVideoXWindow(WId);
	/** emitted when the renderer recieves a reply to a getFileProperties request. */
    void replyGetFileProperties(const QMap < QString, QString > &, const QMap < QString, QString > &);

	/** emitted when the renderer recieves a reply to a getImage request. */
    void replyGetImage(const KURL &, int, const QPixmap &, int, int);
    void replyGetImage(int, const QPixmap &, int, int);

	/** Emitted when the renderer stops, either playing or rendering. */
    void stopped();
	/** Emitted when the renderer starts playing. */
    void playing(double);
	/** Emitted when the renderer is rendering. */
    void rendering(const GenTime &);
	/** Emitted when rendering has finished */
    void renderFinished();
	/** Emitted when the current seek position has been changed by the renderer. */
//    void positionChanged(const GenTime &);
	/** No descriptions */
    void effectListChanged(const QPtrList < EffectDesc > &);
	/** Emitted when an error occurs within this renderer. */
    void error(const QString &, const QString &);

    
    public slots:		// Public slots
	/** Start Consumer */
    void start();
	/** Stop Consumer */
    void stop();
    void clear();
    int getLength();
	/** If the file is readable by mlt, return true, otherwise false */
    bool isValid(KURL url);
    
    /** Create a new producer to show the title clip on top of the current monitor view */
    void setTitlePreview(QString tmpFileName);
    
    /** Restore the original producer after a title preview */
    void restoreProducer();
    
    	/** Wraps the VEML command of the same name. Requests the file properties
    for the specified url from the renderer. Upon return, the result will be emitted
    via replyGetFileProperties(). */
    void getFileProperties(KURL url, uint framenb = 0);
    
    void exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime);
    static char *decodedString(QString str);

};

#endif
