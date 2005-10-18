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
#include <qxml.h>
#include <qstring.h>
#include <qmap.h>
#include <qmutex.h>
#include <qptrlist.h>
#include <qvaluestack.h>

#include <kprocess.h>
#include <kurl.h>

#include "gentime.h"
#include "avfileformatdesc.h"
#include "avformatdesccodec.h"
#include "effectdesc.h"
#include "effectdescriptionlist.h"
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
	class Miracle;
	class Consumer;
	class Producer;
};

struct StackValue
{
	QString element;
	/** A function pointer to the relevant method that should parse tagOpen events */
	bool ( KRender::*funcStartElement ) ( const QString & localName, const QString & qName, const QXmlAttributes & atts );
	/** A function pointer to the relevant method that should parse tagClose events */
	bool ( KRender::*funcEndElement ) ( const QString & localName, const QString & qName );
};
extern int m_refCount;
static QMutex mutex;
class KRender : public QObject, public QXmlDefaultHandler
{
	Q_OBJECT
public:
	
	enum FailStates { OK = 0,
	                  APP_NOEXIST
	                };

	KRender( const QString &rendererName, KURL appPath, unsigned int port, QObject *parent = 0, const char *name = 0 );
	~KRender();
	
	/** Wraps the VEML command of the same name; requests that the renderer
	should create a video window. If show is true, then the window should be
	displayed, otherwise it should be hidden. KRender will emit the signal
	replyCreateVideoXWindow() once the renderer has replied. */
	void createVideoXWindow( bool show,WId winid );
	/** Occurs upon finishing reading an XML document */
	bool endDocument();
	/** Occurs upon starting to parse an XML document */
	bool startDocument();
	/** Seeks the renderer clip to the given time. */
	void seek( GenTime time );
	/** Wraps the VEML command of the same name. Requests the file properties
	for the specified url from the renderer. Upon return, the result will be emitted
	via replyGetFileProperties(). */
	void getFileProperties( KURL url );
	void getImage(KURL url,int frame,QPixmap* image);

	/** Requests a particular frame from the given file. 
	 * 
	 * The pixmap will be returned by emitting the replyGetImage() signal.
	 * */
	void getImage(KURL url,int frame, int width, int height);
	void getSoundSamples(KURL url,int channel,int frame, double frameLength,QByteArray* array);
	/** Wraps the VEML command of the same name. Sets the current scene list to
	be list. */
	void setSceneList( QDomDocument list );

	/** Sets up the renderer as a capture device. */
	void setCapture();

	/** Wraps the VEML command of the same name - sends a <ping> command to the server, which
	should reply with a <pong> - let's us determine the round-trip latency of the connection. */
	void ping( QString &ID );
	/** Wraps the VEML command of the same name. Tells the renderer to
	play the current scene at the speed specified, relative to normal
	playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
	backwards. Does not specify start/stop times for playback.*/
	void play( double speed );
	/** Wraps the VEML command of the same name. Tells the renderer to
	play the current scene at the speed specified, relative to normal
	playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
	backwards. Specifes the start/stop times for playback.*/
	void play(double speed, const GenTime &startTime);
	void play( double speed, const GenTime &startTime, const GenTime &stopTime );
	/** Wraps the VEML command of the same name - render the currently
	specified sceneList - set with setSceneList() - to the document
	name specified. */
	void render( const KURL &url );
	/** Wraps the VEML command of the same name. Requests that the renderer should return it's capabilities. */
	void getCapabilities();
	/** Returns a list of all available file formats in this renderer. */
	QPtrList<AVFileFormatDesc> &fileFormats();
	/** Returns the codec with the given name */
	AVFormatDescCodec * findCodec( const QString &name );
	/** Returns the effect list. */
	const EffectDescriptionList &effectList() const;
	/** Returns the renderer version. */
	QString version();
	/** Returns the description of this renderer */
	QString description();  /** Returns true if the renderer is capable, or is believed to be capable of running and processing commands. It does not necessarily mean that the renderer is currently running, only that KRender has not given up trying to connect to/launch the renderer. Returns false if the renderer cannot be started. */
	bool rendererOk();

	/** Returns the name of the renderer. */
	const QString &rendererName() const;

	/** Returns the speed at which the renderer is currently playing, 0.0 if the renderer is
	not playing anything. */
	double playSpeed();

	/** Returns the current seek position of the renderer. */
	const GenTime &seekPosition() const;

	void sendDebugVemlCommand(const QString &name);

	void emitFrameNumber(const GenTime &time);
protected:  // Protected methods
	/** Recieves timer events */
	virtual void timerEvent( QTimerEvent *event );
private:  // Private attributes & methods
	Mlt::Miracle* m_mltMiracle;
	Mlt::Consumer* m_mltConsumer;
	Mlt::Producer* m_mltProducer;
	

	/** If true, we are currently parsing some data. Otherwise, we are not. */
	bool m_parsing;
	/** The socket that will connect to the server */
	QSocket m_socket;
	/** If we have started our own renderer, this is it's process */
	KProcess m_process;
	/** The port number used to connect to the renderer */
	unsigned int m_portNum;
	/** The XML reader */
	QXmlSimpleReader m_xmlReader;
	/** The path to the rendering application. */
	KURL m_appPath;
	/** true if we have a setSceneList command pending to be sent */
	bool m_setSceneListPending;
	/** Holds the scenelist to be sent, if pending. */
	QDomDocument m_sceneList;
	/** Holds the buffered communication from the socket, ready for processing. */
	QString m_buffer;
	/** The name of this renderer - useful to identify the renderes by what they do - e.g. background rendering, workspace monitor, etc... */
	QString m_name;
	/** The name of the associated renderer - this is the application we are using.*/
	QString m_renderName;
	/** The version of the renderer */
	QString m_renderVersion;
	/** Becomes true if it is known that the application path does not point to a valid file, false if
	this is unknown, or if a valid executable is known to exist */
	bool m_appPathInvalid;
	/** Holds a description of all available file formats. */
	QPtrList<AVFileFormatDesc> m_fileFormats;
	/** The parse stack for start/end element events. */
	QValueStack<StackValue> m_parseStack;
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
	/** Holds an effect description during construction. Keep an eye out for potential memory leaks. */
	EffectDesc *m_effect;
	/** Holds an effect parameter during construction. Keep an eye out for potential memory leaks. */
	EffectParamDesc *m_parameter;

	/** The factory used to create EffectDescParam objects. */
	EffectParamDescFactory m_effectDescParamFactory;

	/** Holds a list of all available codecs. */
	QPtrList<AVFormatDescCodec> m_codeclist;
	/** Holds a list of all available effects. */
	EffectDescriptionList m_effectList;
	/** The renderer version number. */
	QString m_version;
	/** Holds the authors of this renderer. */
	QMap<QString, QString> m_authors;
	/** A human-readable description of this renderer. */
	QString m_description;
	/** Holds the filename of the current getFileProperties message when processing an error */
	QString m_filePropertiesFileName;
	/** Holds the last error message discovered */
	QString m_errorMessage;

	/** The current speed of playback */
	double m_playSpeed;

	/**The current seek position */
	GenTime m_seekPosition;

	/** A bit hackish, well, a lot haackish really. File properties exist across a number of xml tags,
	 * so we have to collect them together before emmitting them. We do that with this value here. */
	QMap<QString, QString> m_filePropertyMap;
	/** Sends a quit command to the renderer*/
	void quit();
	/** Sends an XML command to the renderer. */
	void sendCommand( QDomDocument command );
	/** Launches a renderer process. */
	void launchProcess();
	/** The actually seek command, private so people can't avoid the buffering of multiple seek commands. */
	void sendSeekCommand( GenTime time );
	/** The actually setScenelist command, private so people can't avoid the buffering of multiple setSceneList commands. */
	void sendSetSceneListCommand( const QDomDocument &list );
	/** Pushes a stack parse with no definition, this effectively causes all
	following tags in the current hierarch to be ignored until the end tag is
	reached. */
	void pushIgnore();

	// Pushes a value onto the stack.
	void pushStack( QString element,
	                bool ( KRender::*funcStartElement ) ( const QString & localName, const QString & qName, const QXmlAttributes & atts ),
	                bool ( KRender::*funcEndElement ) ( const QString & localName, const QString & qName ) );

	/** Sets the renderer version for this renderer. */
	void setVersion( QString version );
	/** Sets the description of this renderer to desc. */
	void setDescription( const QString &description );
	void openMlt();
	void closeMlt();
private slots:  // Private slots
	/** Catches errors from the socket. */
	void error( int error );
	/** Called when some data has been recieved by the socket, reads the data and processes it. */
	void readData();
	/** Called when we have connected to the renderer. */
	void slotConnected();
	/** Called when we have disconnected to the renderer. */
	void slotDisconnected();
	/** Called if the rendering process has exited. */
	void processExited();
signals:  // Signals
	/** This signal is emitted once the renderer has initialised itself. */
	void initialised();
	/** This signal is emitted upon connection */
	void connected();
	/** This signal is emitted upon disconnection */
	void disconnected();
	/** This signal is emitted once a reply to createVideoXWidow() has been recieved. */
	void replyCreateVideoXWindow( WId );
	/** emitted when the renderer recieves a reply to a getFileProperties request. */
	void replyGetFileProperties( const QMap<QString, QString> & );
	
	/** emitted when the renderer recieves a reply to a getImage request. */
	void replyGetImage( const KURL &, int, const QPixmap &);
	
	/** emitted when the renderer recieves a failed reply to a getFileProperties request.
	    First string is file url, second string is the error message. */
	void replyErrorGetFileProperties( const QString &, const QString & );
	/** Emitted when the renderer has recieved text from stdout */
	void recievedStdout( const QString &, const QString & );
	/** Emitted when the renderer has recieved text from stderr */
	void recievedStderr( const QString &, const QString & );
	/** Emits useful rendering debug info. */
	void renderDebug( const QString &, const QString & );
	/** Emits renderer warnings info. */
	void renderWarning( const QString &, const QString & );
	/** Emits renderer errors. */
	void renderError( const QString &, const QString & );
	/** Emitted when the renderer stops, either playing or rendering. */
	void stopped();
	/** Emitted when the renderer starts playing. */
	void playing( double );
	/** Emitted when the renderer is rendering. */
	void rendering( const GenTime & );
	/** Emitted when rendering has finished */
	void renderFinished();
	/** Emitted when the current seek position has been changed by the renderer. */
	void positionChanged( const GenTime & );
	/** Emitted when file formats are updated. */
	void signalFileFormatsUpdated( const QPtrList<AVFileFormatDesc> & );
	/** No descriptions */
	void effectListChanged( const QPtrList<EffectDesc> & );
	/** Emitted when an error occurs within this renderer. */
	void error( const QString &, const QString & );
public slots:  // Public slots
	/** This slot reads stdIn and processes it. */
	void slotReadStdout( KProcess *proc, char *buffer, int buflen );
	void slotReadStderr( KProcess *proc, char *buffer, int buflen );
};

#endif
