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

#include <qobject.h>
#include <qsocket.h>
#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <qthread.h>
#include <qptrlist.h>
#include <qvaluestack.h>

#include <kprocess.h>
#include <kurl.h>

#ifdef ENABLE_FIREWIRE
#include <libiec61883/iec61883.h>
#endif

#include "gentime.h"
#include "avfileformatdesc.h"
#include "avformatdesccodec.h"
#include "docclipref.h"
#include "effectdesc.h"
#include "effectdescriptionlist.h"
#include "effectparamdescfactory.h"
#include "initeffects.h"
#include "kdenlive.h"

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
    //class Miracle;
    class Consumer;
    class Producer;
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

extern int m_refCount;

class KRender:public QObject {
  Q_OBJECT public:

    enum FailStates { OK = 0,
	APP_NOEXIST
    };

     KRender(const QString & rendererName, Gui::KdenliveApp *parent = 0, const char *name = 0);
    ~KRender();

	/** Wraps the VEML command of the same name; requests that the renderer
	should create a video window. If show is true, then the window should be
	displayed, otherwise it should be hidden. KRender will emit the signal
	replyCreateVideoXWindow() once the renderer has replied. */
    void createVideoXWindow(bool show, WId winid);
	/** Seeks the renderer clip to the given time. */
    void seek(GenTime time);
    
    void getImage(KURL url, int frame, QPixmap * image);

	/** Return thumbnail for color clip */
    void getImage(int id, QString color, int width, int height);
    
    /** Return thumbnail for text clip */
    void getImage(int id, QString txt, uint size, int width, int height);
    
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
	/** Returns a list of all available file formats in this renderer. */
    QPtrList < AVFileFormatDesc > &fileFormats();
	/** Returns the effect list. */
    const EffectDescriptionList & effectList() const;
	/** Returns the renderer version. */
    QString version();
	/** Returns the description of this renderer */
    QString description();

	/** Returns the name of the renderer. */
    const QString & rendererName() const;

	/** Returns the speed at which the renderer is currently playing, 0.0 if the renderer is
	not playing anything. */
    double playSpeed();

	/** Returns the current seek position of the renderer. */
    const GenTime & seekPosition() const;

    void emitFrameNumber(const GenTime & time, int eventType);
    void emitFileFrameNumber(const GenTime & time, int eventType);
    void emitConsumerStopped();
    void emitFileConsumerStopped();
    
    void getImage(DocClipRef * clip);
    
    /** render timeline to a file */
    void exportTimeline(const QString &url, const QString &format, GenTime exportStart, GenTime exportEnd, QStringList params);
    void stopExport();
    
    /** Gives the aspect ratio of the consumer */
    double consumerRatio();
    
    /** Gives the aspect ratio of the consumer */
    void askForRefresh();
    
    /** Save current producer frame as image */
    void exportCurrentFrame(KURL url);
    

  protected:			// Protected methods
	/** Recieves timer events */
     virtual void timerEvent(QTimerEvent * event);
     
     
  private:			// Private attributes & methods
     //Mlt::Miracle * m_mltMiracle;
     Mlt::Consumer * m_mltConsumer;
     Mlt::Producer * m_mltProducer;
     Mlt::Consumer *m_fileRenderer;
     Mlt::Producer * m_mltFileProducer;
     Gui::KdenliveApp *m_app;
     Mlt::Producer *m_mltTextProducer;
     
     QTimer *refreshTimer;
     bool m_isRendering;
     QString m_renderingFormat;
     
     
     int exportDuration, firstExportFrame, lastExportFrame;


	/** If true, we are currently parsing some data. Otherwise, we are not. */
    bool m_parsing;
	/** If we have started our own renderer, this is it's process */
    KProcess m_process;
	/** Holds the scenelist to be sent, if pending. */
    QDomDocument m_sceneList;
	/** Holds the buffered communication from the socket, ready for processing. */
    QString m_buffer;
	/** The name of this renderer - useful to identify the renderes by what they do - e.g. background rendering, workspace monitor, etc... */
    QString m_name;
	/** Holds a description of all available file formats. */
     QPtrList < AVFileFormatDesc > m_fileFormats;
	/** The parse stack for start/end element events. */
     QValueStack < StackValue > m_parseStack;
	/** Holds a buffer of characters, as returned by the parser */
    QString m_characterBuffer;
	/** Holds the file format during construction. Keep an eye out for potential memory leaks and
	 null pointer exceptions. */
    AVFileFormatDesc *m_fileFormat;
	/** Holds a codec list description during constructuion. Keep an eye out for potential memory leaks
	and null pointer exceptions. */
    AVFormatDescCodecList *m_desccodeclist;
	/** Holds a codec description during constructuion. Keep an eye out for potential memory leaks
	and null pointer exceptions. */
    AVFormatDescCodec *m_codec;

	/** Holds a list of all available codecs. */
     QPtrList < AVFormatDescCodec > m_codeclist;
	/** Holds a list of all available effects. */
    EffectDescriptionList m_effectList;
	/** The renderer version number. */
    QString m_version;
	/** Holds the authors of this renderer. */
     QMap < QString, QString > m_authors;
	/** A human-readable description of this renderer. */
    QString m_description;
	/** Holds the filename of the current getFileProperties message when processing an error */
    QString m_filePropertiesFileName;
	/** Holds the last error message discovered */
    QString m_errorMessage;

	/**The current seek position */
    GenTime m_seekPosition;

	/** A bit hackish, well, a lot haackish really. File properties exist across a number of xml tags,
	 * so we have to collect them together before emmitting them. We do that with this value here. */
     QMap < QString, QString > m_filePropertyMap;
	/** The actually seek command, private so people can't avoid the buffering of multiple seek commands. */
    void sendSeekCommand(GenTime time);

	/** Sets the renderer version for this renderer. */
    void setVersion(QString version);
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


     signals:			// Signals
	/** This signal is emitted once the renderer has initialised itself. */
    void initialised();
	/** This signal is emitted upon connection */
    void connected();
	/** This signal is emitted upon disconnection */
    void disconnected();
	/** This signal is emitted once a reply to createVideoXWidow() has been recieved. */
    void replyCreateVideoXWindow(WId);
	/** emitted when the renderer recieves a reply to a getFileProperties request. */
    void replyGetFileProperties(const QMap < QString, QString > &);

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
	/** Emitted when file formats are updated. */
    void signalFileFormatsUpdated(const QPtrList < AVFileFormatDesc > &);
	/** No descriptions */
    void effectListChanged(const QPtrList < EffectDesc > &);
	/** Emitted when an error occurs within this renderer. */
    void error(const QString &, const QString &);

    
    public slots:		// Public slots
	/** Start Consumer */
    void start();
	/** Stop Consumer */
    void stop();
    
	/** If the file is readable by mlt, return true, otherwise false */
    bool isValid(KURL url);
    
    /** Create a new producer to show the title clip on top of the current monitor view */
    void setTitlePreview(QString tmpFileName);
    
    /** Restore the original producer after a title preview */
    void restoreProducer();
    
    	/** Wraps the VEML command of the same name. Requests the file properties
    for the specified url from the renderer. Upon return, the result will be emitted
    via replyGetFileProperties(). */
    void getFileProperties(KURL url);
    
    void exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime);

};

#endif
