/***************************************************************************
                          krender.cpp  -  description
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

#include "krender.h"

#include <kdebug.h>

KRender::KRender(KURL appPath, unsigned int port, QObject *parent, const char *name ) :
																				QObject(parent, name),
																				QXmlDefaultHandler()
{
	startTimer(200);
	m_parsing = false;
	m_seekPending = false;
	m_nextSeek = -1.0;
	m_setSceneListPending = false;

	m_xmlReader.setContentHandler(this);

	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	connect(&m_socket, SIGNAL(error(int)), this, SLOT(error(int)));
	connect(&m_socket, SIGNAL(connected()), this, SLOT(connected()));
	connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readData()));

	connect(&m_process, SIGNAL(processExited(KProcess *)), this, SLOT(processExited()));

	m_portNum = port;
  m_appPath = appPath;
  launchProcess();
}

KRender::~KRender()
{
	killTimers();
	quit();
}

/** Recieves timer events */
void KRender::timerEvent(QTimerEvent *event)
{
  if(m_socket.state() == QSocket::Idle) {
    if(!m_process.isRunning()) {
    	launchProcess();
    } else {
     	m_socket.connectToHost("127.0.0.1", m_portNum);
    }
  }
}

/** Catches errors from the socket. */
void KRender::error(int error)
{	
	switch(error) {
		case QSocket::ErrConnectionRefused :
							//kdDebug() << "Connection Refused" << endl;
							m_socket.connectToHost("127.0.0.1", m_portNum);
							break;
		case QSocket::ErrHostNotFound :
							kdDebug() << "Host Not Found" << endl;		
							break;
		case QSocket::ErrSocketRead :
							kdDebug() << "Error reading Socket" << endl;		
							break;
	}
}

/** Called when we have connected to the renderer. */
void KRender::connected()
{
	kdDebug() << "Connected" << endl;

  getCapabilities();
  
	emit initialised();
}

/** Called when some data has been recieved by the socket, reads the data and processes it. */
void KRender::readData()
{
  m_buffer += QString(m_socket.readAll());
  int pos;

  while((pos = m_buffer.find("\n\n")) != -1) {    
    QString temp = m_buffer.left(pos+1);    
    m_buffer = m_buffer.right(m_buffer.length() - pos);
    while(m_buffer.left(1) == "\n") m_buffer = m_buffer.right(m_buffer.length() -1);
    
    QXmlInputSource source;
    source.setData(temp);
    if(!m_xmlReader.parse(&source, false)) {
      kdWarning() << "Parse Failed on " << temp << endl;
    }
  }  
}

/** Sends an XML command to the renderer. */
void KRender::sendCommand(QDomDocument command)
{
//	kdDebug() << "Sending Command " << command.toString() << endl;
	QCString str = command.toCString() + "\n";
	m_socket.writeBlock(str, strlen(str));	
}

/** Generates the quit command */
void KRender::quit()
{
	QDomDocument doc;	
	doc.appendChild(doc.createElement("quit"));
	sendCommand(doc);
}

/** Called if the rendering process has exited. */
void KRender::processExited()
{
	kdDebug() << "Renderer process exited" << endl;
}

/** Launches a renderer process. */
void KRender::launchProcess()
{
	m_process.clearArguments();
	m_process.setExecutable(m_appPath.path());
  m_process << "-p " << m_portNum;

	kdDebug() << "Launching process " << m_appPath.path() << " as server on port " << m_portNum << endl;
	if(m_process.start()) {
		kdDebug() << "Process launching successfully" << endl;
		kdDebug() << "Connecting to server on port " << m_portNum << endl;
		m_socket.connectToHost("127.0.0.1", m_portNum);
	} else {
		kdError() << "Could not start process" << endl;	
	}
}

/** Wraps the VEML command of the same name; requests that the renderer
should create a video window. If show is true, then the window should be
displayed, otherwise it should be hidden. KRender will emit the signal
replyCreateVideoXWindow() once the renderer has replied. */
void KRender::createVideoXWindow(bool show)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("createVideoXWindow");	
	elem.setAttribute("show", show ? "true" : "false");
	doc.appendChild(elem);
	
	sendCommand(doc);
}

/** Wraps the VEML command of the same name; Seeks the renderer clip to the given time. */
void KRender::seek(GenTime time)
{
    sendSeekCommand(time);    
/*  if(m_seekPending) {
    m_nextSeek = time;
  } else {
    sendSeekCommand(time);    
    m_seekPending = true;
  }*/
}

void KRender::getFileProperties(KURL url)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("getFileProperties");

	elem.setAttribute("filename", url.path());
	doc.appendChild(elem);

	sendCommand(doc);
}

/** Wraps the VEML command of the same name. Sets the current scene list to
be list. */
void KRender::setSceneList(QDomDocument list)
{
	m_sceneList = list;
	m_setSceneListPending = true;
}

/** Wraps the VEML command of the same name - sends a <ping> command to the server, which
should reply with a <pong> - let's us determine the round-trip latency of the connection. */
void KRender::ping(QString &ID)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("ping");
	elem.setAttribute("id", ID);
	doc.appendChild(elem);
	sendCommand(doc);
}

void KRender::play(double speed)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("play");
	elem.setAttribute("speed", speed);
	doc.appendChild(elem);
	sendCommand(doc);	
}

void KRender::render(const KURL &url)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("render");
	elem.setAttribute("filename", url.path());
	doc.appendChild(elem);
	sendCommand(doc);
}

void KRender::sendSeekCommand(GenTime time)
{
	if(m_setSceneListPending) {
		sendSetSceneListCommand(m_sceneList);
	}
	
 	QDomDocument doc;
 	QDomElement elem = doc.createElement("seek");
 	elem.setAttribute("time", QString::number(time.seconds()));
 	doc.appendChild(elem);
	sendCommand(doc);  
}

void KRender::sendSetSceneListCommand(const QDomDocument &list)
{
	m_setSceneListPending = false;

	QDomDocument doc;
	QDomElement elem = doc.createElement("setSceneList");
	elem.appendChild(doc.importNode(list.documentElement(), true));	
	doc.appendChild(elem);
	sendCommand(doc);
}

void KRender::getCapabilities()
{
  QDomDocument doc;
  QDomElement elem = doc.createElement("getCapabilites");
  doc.appendChild(elem);
  sendCommand(doc);
}




/** Occurs upon starting to parse an XML document */
bool KRender::startDocument()
{
	kdDebug() << "Starting to parse document" << endl;
	return true;
}

/** Occurs upon finishing reading an XML document */
bool KRender::endDocument()
{
	kdDebug() << "finishing parsing document" << endl;  
	return true;
}

/** Called when the xml parser encounters an opening element */
bool KRender::startElement(const QString & namespaceURI, const QString & localName,
																	const QString & qName, const QXmlAttributes & atts )
{
	kdDebug() << "Discovered opening tag " << localName << endl;

	return (this->*m_funcStartElement)(namespaceURI, localName, qName, atts);
}

/** Called when the xml parser encounters a closing tag */
bool KRender::endElement ( const QString & namespaceURI, const QString & localName, const QString & qName )
{
	kdDebug() << "Discovered closing tag " << localName << endl;

	return (this->*m_funcEndElement)(namespaceURI, localName, qName);
}

/** Called when the xml parser encounters an opening element and we are outside of a parsing loop. */
bool KRender::topLevelStartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	if(localName == "reply") {
		QString command = atts.value("command");
		if(command.isNull()) {
			kdError() << "Reply recieved, no command specified" << endl;
			return false;
		} else if(command == "createVideoXWindow") {
			QString winID = atts.value("WinID");
			WId retID = 0;
			if(winID.isNull()) {
				kdWarning() << "Window ID not specified - emitting 0" << endl;
			} else {
				retID = winID.toInt();
			}
			emit replyCreateVideoXWindow(retID);
			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
			return true;
		} else if(command == "getFileProperties") {		
			QMap<QString, QString> map;

			map["filename"] = atts.value("filename");
			map["duration"] = atts.value("duration");

			emit replyGetFileProperties(map);

			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
			return true;
		} else if(command == "play") {
			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
			return true;
    } else if(command == "seek") {
			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
      if(m_seekPending) {
        if(m_nextSeek < 0.0) {
          m_seekPending = false;
        } else {
          sendSeekCommand(m_nextSeek);
          m_nextSeek = -1.0;
        }
      }
			return true;
		} else if(command == "render") {
			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
			return true;
		}
	} else if(localName == "pong") {
		QString id = atts.value("id");
		kdDebug() << "pong recieved : " << id << endl;
		m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
  	m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
    return true;
	}

	kdWarning() << "Unknown tag" << endl;
	return false;
}

/** Called when we are parsing a close tag and are at the top level of the document. */
bool KRender::topLevelEndElement(const QString & namespaceURI, const QString & localName,
																									const QString & qName)
{
	kdWarning() << "Parsing topLevel End Element - this should not happen, ever!" << endl;
	return false;
}

bool KRender::reply_GenericEmpty_StartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	kdWarning() << "Should not recieve reply_GenericEmpty_StartElement!" << endl;
	return false;
}

bool KRender::reply_GenericEmpty_EndElement(const QString & namespaceURI, const QString & localName,
																									const QString & qName)
{
	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	return true;
}

