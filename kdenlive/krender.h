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

#include <kprocess.h>
#include <kurl.h>

#include "gentime.h"

/**KRender encapsulates the client side of the interface to a renderer.
From Kdenlive's point of view, you treat the KRender object as the
renderer, and simply use it as if it was local. Calls are asyncrhonous - 
you send a call out, and then recieve the return value through the 
relevant signal that get's emitted once the call completes.
  *@author Jason Wood
  */

class KRender : public QObject, public QXmlDefaultHandler  {
   Q_OBJECT
public: 
	KRender(KURL appPath, unsigned int port, QObject *parent=0, const char *name=0);
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
  bool endElement ( const QString & namespaceURI, const QString & localName, const QString & qName );
  /** Called when the xml parser encounters an opening element */
  bool startElement(const QString & namespaceURI, const QString & localName, const QString & qName, const QXmlAttributes & atts );
  /** Called when we are parsing a close tag and are outside of any command. */
  bool topLevelEndElement(const QString & namespaceURI, const QString & localName,                    const QString & qName);
  /** Called when the xml parser encounters an opening element and we are outside of any command. */
  bool topLevelStartElement(const QString & namespaceURI, const QString & localName, const QString & qName, const QXmlAttributes & att);

/** Called when we are parsing a close tag and are in a top level command of the document which has no inner tags */
  bool reply_GenericEmpty_EndElement(const QString & namespaceURI, const QString & localName,                    const QString & qName);
  /** Called when the xml parser encounters an opening element and we are in a top level command of the document which has no inner tags */
  bool reply_GenericEmpty_StartElement(const QString & namespaceURI, const QString & localName, const QString & qName, const QXmlAttributes & att);  
  
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
  
  /** A function pointer to the relevant method that should parse tagOpen events */
  bool (KRender::*m_funcStartElement)(const QString & namespaceURI, const QString & localName,
                                  const QString & qName, const QXmlAttributes & atts ); 
  /** A function pointer to the relevant method that should parse tagClose events */
  bool (KRender::*m_funcEndElement)(const QString & namespaceURI, const QString & localName,
				const QString & qName);
  /** The actually seek command, private so people can't avoid the buffering of multiple seek commands. */
  void sendSeekCommand(GenTime time);
  /** The actually setScenelist command, private so people can't avoid the buffering of multiple setSceneList commands. */
  void sendSetSceneListCommand(const QDomDocument &list);
private slots: // Private slots
  /** Catches errors from the socket. */
  void error(int error);
  /** Called when some data has been recieved by the socket, reads the data and processes it. */
  void readData();
  /** Called when we have connected to the renderer. */
  void connected();
  /** Called if the rendering process has exited. */
  void processExited();
private: // Private methods
  /** Sends a quit command to the renderer*/
  void quit();
  /** Sends an XML command to the renderer. */
  void sendCommand(QDomDocument command);
  /** Launches a renderer process. */
  void launchProcess();
signals: // Signals
  /** This signal is emitted once the renderer has initialised itself. */
  void initialised();
  /** This signal is emitted once a reply to createVideoXWidow() has been recieved. */
  void replyCreateVideoXWindow(WId);
public: // Public attributes
  /** If true, we are currently parsing some data. Otherwise, we are not. */
  bool m_parsing;
signals: // Signals
  /** emitted when the renderer recieves a reply to a getFileProperties request. */
  void replyGetFileProperties(QMap<QString, QString>);
};

#endif
