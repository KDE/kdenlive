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
#include <kprocess.h>

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
	KRender(QObject *parent=0, const char *name=0);
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
/** Called when we are parsing a close tag and are in a replyCreateVideoXWindow of the document. */
  bool replyCreateVideoXWindowEndElement(const QString & namespaceURI, const QString & localName,                    const QString & qName);
  /** Called when the xml parser encounters an opening element and we are replyCreateVideoXWindow of the document. */
  bool replyCreateVideoXWindowStartElement(const QString & namespaceURI, const QString & localName, const QString & qName, const QXmlAttributes & att); 
  /** Seeks the renderer clip to the given time. */
  void seek(GenTime time);
protected: // Protected methods
  /** Recieves timer events */
  virtual void timerEvent(QTimerEvent *event);
private: // Private attributes
  /** The socket that will connect to the server */
  QSocket m_socket;
  /** If we have started our own renderer, this is it's process */
	KProcess m_process;
  /** Contains the port number that should be used for the next renderer */  
  static unsigned int NextPort;
  /** The port number used to connect to the renderer */
  unsigned int m_portNum;
  /** The XML reader */
  QXmlSimpleReader m_xmlReader;
  /** The input source for the xml reader - set to be the incoming tcp socket */
  QXmlInputSource *m_xmlInputSource;
	/** A function pointer to the relevant method that should parse tagOpen events */
	bool (KRender::*m_funcStartElement)(const QString & namespaceURI, const QString & localName,
																	const QString & qName, const QXmlAttributes & atts );
	/** A function pointer to the relevant method that should parse tagClose events */
	bool (KRender::*m_funcEndElement)(const QString & namespaceURI, const QString & localName,
																	const QString & qName);	
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
};

#endif
