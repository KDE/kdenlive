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
#include <qptrlist.h>
#include <qvaluestack.h>

#include <kprocess.h>
#include <kurl.h>

#include "gentime.h"
#include "avfileformatdesc.h"
#include "avformatdesccodec.h"
#include "effectdesc.h"

/**KRender encapsulates the client side of the interface to a renderer.
From Kdenlive's point of view, you treat the KRender object as the
renderer, and simply use it as if it was local. Calls are asyncrhonous - 
you send a call out, and then recieve the return value through the 
relevant signal that get's emitted once the call completes.
  *@author Jason Wood
  */

class KRender;
class AVFormatDescCodecList;
  
struct StackValue {
  QString element;
  /** A function pointer to the relevant method that should parse tagOpen events */
  bool (KRender::*funcStartElement)(const QString & localName, const QString & qName, const QXmlAttributes & atts );
  /** A function pointer to the relevant method that should parse tagClose events */
  bool (KRender::*funcEndElement)(const QString & localName, const QString & qName);  
};

class KRender : public QObject, public QXmlDefaultHandler  {
   Q_OBJECT
public: 
	KRender(const QString &rendererName, KURL appPath, unsigned int port, QObject *parent=0, const char *name=0);
	~KRender();
  /** Wraps the VEML command of the same name; requests that the renderer
should create a video window. If show is true, then the window should be
displayed, otherwise it should be hidden. KRender will emit the signal
replyCreateVideoXWindow() once the renderer has replied. */
  void createVideoXWindow(bool show);
  /** Occurs upon finishing reading an XML document */
  bool endDocument();
  /** Occurs upon starting to parse an XML document */
  bool startDocument();
  /** Called when the xml parser encounters a closing tag */
  bool endElement (const QString &nameSpace, const QString & localName, const QString & qName );
  /** Called when the xml parser encounters an opening element */
  bool startElement(const QString &nameSpace, const QString & localName, const QString & qName, const QXmlAttributes & atts );
  /** Called when the xml parser encounters characters */  
  bool characters( const QString &ch );
  /** Called when the xml parser encounters an opening element and we are outside of any command. */
  bool topLevelStartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);
  bool reply_getCapabilities_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);
  bool reply_createVideoXWindow_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & atts);
  bool reply_capabilities_iostreams_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);  
  bool reply_capabilities_iostreams_outstream_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);
  bool reply_capabilities_iostreams_outstream_file_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);
  bool reply_capabilities_iostreams_outstream_file_EndElement(const QString & localName, const QString & qName);  
  bool reply_capabilities_iostreams_outstream_container_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);
  bool reply_capabilities_iostreams_outstream_codec_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & att);
  bool reply_capabilities_iostreams_outstream_codec_EndElement(const QString & localName, const QString & qName);
  bool reply_capabilities_codecs_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & atts);
  bool reply_capabilities_codecs_encoder_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & atts);
  bool reply_capabilities_codecs_encoder_EndElement(const QString & localName, const QString & qName);
  bool reply_capabilities_codecs_encoder_about_EndElement(const QString & localName, const QString & qName);
  bool reply_capabilities_effects_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & atts);
  bool reply_capabilities_effects_effect_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & atts);    
  bool reply_capabilities_effects_effect_EndElement(const QString & localName, const QString & qName);
  bool reply_capabilities_renderer_StartElement(const QString & localName, const QString & qName, const QXmlAttributes & atts);
  bool reply_capabilities_renderer_about_EndElement(const QString & localName, const QString & qName);
  
  /** Seeks the renderer clip to the given time. */
  void seek(GenTime time);
  /** Wraps the VEML command of the same name. Requests the file properties
	for the specified url from the renderer. Upon return, the result will be emitted
	via replyGetFileProperties(). */
  void getFileProperties(KURL url);
  /** Wraps the VEML command of the same name. Sets the current scene list to
	be list. */
  void setSceneList(QDomDocument list);
  /** Wraps the VEML command of the same name - sends a <ping> command to the server, which
should reply with a <pong> - let's us determine the round-trip latency of the connection. */
  void ping(QString &ID);;
  /** Wraps the VEML command of the same name. Tells the renderer to
play the current scene at the speed specified, relative to normal
playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
backwards. */
  void play(double speed);
  /** Wraps the VEML command of the same name - render the currently
specified sceneList - set with setSceneList() - to the document
name specified. */
  void render(const KURL &url);
  /** Wraps the VEML command of the same name. Requests that the renderer should return it's capabilities. */
  void getCapabilities();
  /** Returns a list of all available file formats in this renderer. */
  QPtrList<AVFileFormatDesc> &fileFormats();
  /** Returns the codec with the given name */
  AVFormatDescCodec * findCodec(const QString &name);
  /** Returns the effect list. */
  const QPtrList<EffectDesc> effectList();
  /** Returns the renderer version. */
  QString version();
  /** Returns the description of this renderer */
  QString description();
  /** Returns the last known seek position for this renderer. This may have been set by the user, or returned by the renderer. */
  const GenTime & lastSeekPosition();
protected: // Protected methods
  /** Recieves timer events */
  virtual void timerEvent(QTimerEvent *event);
private: // Private attributes
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
  /** True if we have sent a seek command but have not yet recieved a reply. */
  bool m_seekPending;;
  /** Holds the next seek value to send to the renderer. If negative, we do not send a value. */
  GenTime m_nextSeek;
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
  /** Holds a list of all available codecs. */
  QPtrList<AVFormatDescCodec> m_codeclist;
  /** Holds a list of all available effects. */
  QPtrList<EffectDesc> m_effectList;  
  /** The renderer version number. */
  QString m_version;
  /** Holds the authors of this renderer. */
  QMap<QString, QString> m_authors;
  /** The last known seek position for this renderer. */
  GenTime m_lastSeek;
  /** A human-readable description of this renderer. */
  QString m_description;
private slots: // Private slots
  /** Catches errors from the socket. */
  void error(int error);
  /** Called when some data has been recieved by the socket, reads the data and processes it. */
  void readData();
  /** Called when we have connected to the renderer. */
  void slotConnected();
  /** Called when we have disconnected to the renderer. */
  void slotDisconnected();  
  /** Called if the rendering process has exited. */
  void processExited();
private: // Private methods
  /** Sends a quit command to the renderer*/
  void quit();
  /** Sends an XML command to the renderer. */
  void sendCommand(QDomDocument command);
  /** Launches a renderer process. */
  void launchProcess();
  /** The actually seek command, private so people can't avoid the buffering of multiple seek commands. */
  void sendSeekCommand(GenTime time);
  /** The actually setScenelist command, private so people can't avoid the buffering of multiple setSceneList commands. */
  void sendSetSceneListCommand(const QDomDocument &list);  
  /** Pushes a stack parse with no definition, this effectively causes all
following tags in the current hierarch to be ignored until the end tag is
reached. */
  void pushIgnore();
  /** Sets the renderer version for this renderer. */
  void setVersion(QString version);
  /** Sets the description of this renderer to desc. */
  void setDescription(const QString &description);
signals: // Signals
  /** This signal is emitted once the renderer has initialised itself. */
  void initialised();
  /** This signal is emitted upon connection */
  void connected();
  /** This signal is emitted upon disconnection */
  void disconnected();  
  /** This signal is emitted once a reply to createVideoXWidow() has been recieved. */
  void replyCreateVideoXWindow(WId);
  /** emitted when the renderer recieves a reply to a getFileProperties request. */
  void replyGetFileProperties(QMap<QString, QString>);
  /** Emitted when the renderer has recieved text from stdout */
  void recievedStdout(const QString &, const QString &);
  /** Emitted when the renderer has recieved text from stderr */
  void recievedStderr(const QString &, const QString &);
  /** Emitted when the renderer has some information to pass on */
  void recievedInfo(const QString &, const QString &);
  /** Emitted when the renderer stops, either playing or rendering. */
  void stopped();
  /** Emitted when the renderer starts playing. */
  void playing();
  /** Emitted when the current seek position has been changed by the renderer. */
  void positionChanged(const GenTime &);
  /** Emitted when file formats are updated. */
  void signalFileFormatsUpdated(const QPtrList<AVFileFormatDesc> &);
  /** No descriptions */
  void effectListChanged(const QPtrList<EffectDesc> &);  
public: // Public attributes
  /** If true, we are currently parsing some data. Otherwise, we are not. */
  bool m_parsing;
public slots: // Public slots
  /** This slot reads stdIn and processes it. */
  void slotReadStdout(KProcess *proc, char *buffer, int buflen);
  void slotReadStderr(KProcess *proc, char *buffer, int buflen);  
};

#endif
