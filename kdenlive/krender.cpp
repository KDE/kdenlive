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

unsigned int KRender::NextPort = 6100;

KRender::KRender(QObject *parent, const char *name ) :
																				QObject(parent, name),
																				QXmlDefaultHandler()
{
	startTimer(50);
	m_xmlInputSource = 0;
	m_parsing = false;

	m_xmlReader.setContentHandler(this);

	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	connect(&m_socket, SIGNAL(error(int)), this, SLOT(error(int)));
	connect(&m_socket, SIGNAL(connected()), this, SLOT(connected()));
	connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readData()));

//	connect(&m_process, SIGNAL(processExited(KProcess *)), this, SLOT(processExited()));

	m_portNum = NextPort;
	NextPort++;
	m_socket.connectToHost("127.0.0.1", m_portNum);
//	launchProcess();
}

KRender::~KRender()
{
	if(m_xmlInputSource != 0) delete m_xmlInputSource;
	killTimers();
	quit();
}

/** Recieves timer events */
void KRender::timerEvent(QTimerEvent *event)
{
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
	if(m_xmlInputSource != 0) delete m_xmlInputSource;
	m_xmlInputSource = new QXmlInputSource((QIODevice *)&m_socket);

	emit initialised();
}

/** Called when some data has been recieved by the socket, reads the data and processes it. */
void KRender::readData()
{
	kdDebug() << "Reading data" << endl;
	m_xmlInputSource->fetchData();
	
	if(m_parsing == false) {
		kdDebug() << "parsing data" << endl;
		m_parsing = true;
		
		if(!m_xmlReader.parse(m_xmlInputSource, true)) {
			kdError() << "XML Parsing failed" << endl;
			m_parsing = false;			
		}
	} else {
		kdDebug() << "continuing parse" << endl;		
		if(!m_xmlReader.parseContinue()) {
			kdError() << "Error parsing XML from server" << endl;
			m_parsing = false;
		}
	}
}

/** Sends an XML command to the renderer. */
void KRender::sendCommand(QDomDocument command)
{
	QCString str = command.toCString();
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
/*	m_portNum = NextPort;
	NextPort++;

	m_process.clearArguments();
	m_process << "/home/uchian/piave/piave/piave" << "-s";

	kdDebug() << "Launching process as server on port " << m_portNum << endl;
	if(m_process.start()) {
		kdDebug() << "Process launching successfully" << endl;
		kdDebug() << "Connecting to server on port " << m_portNum << endl;
		m_socket.connectToHost("127.0.0.1", m_portNum);		
	} else {
		kdError() << "Could not start process" << endl;	
	}*/
}

/** Wraps the VEML command of the same name; requests that the renderer
should create a video window. If show is true, then the window should be
displayed, otherwise it should be hidden. KRender will emit the signal
replyCreateVideoXWindow() once the renderer has replied. */
void KRender::createVideoXWindow(bool show)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("createVideoXWindow");
	elem.setAttribute("show", "true");
	doc.appendChild(elem);
	
	sendCommand(doc);
}

/** Wraps the VEML command of the same name; Seeks the renderer clip to the given time. */
void KRender::seek(GenTime time)
{
	QDomDocument doc;
	QDomElement elem = doc.createElement("seek");	
	elem.setAttribute("time", QString::number(time.seconds()));
	doc.appendChild(elem);

	sendCommand(doc);
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
	QDomDocument doc;
	QDomElement elem = doc.createElement("setSceneList");
	elem.appendChild(doc.importNode(list.documentElement(), true));	
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

	(this->*m_funcStartElement)(namespaceURI, localName, qName, atts);
	
	return true;
}

/** Called when the xml parser encounters a closing tag */
bool KRender::endElement ( const QString & namespaceURI, const QString & localName, const QString & qName )
{
	kdDebug() << "Discovered closing tag " << localName << endl;

	(this->*m_funcEndElement)(namespaceURI, localName, qName);	
	return true;
}

/** Called when the xml parser encounters an opening element and we are outside of a parsing loop. */
bool KRender::topLevelStartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	kdDebug() << "Parsing topLevel startElement()" << endl;

	if(localName == "reply") {
		QString command = atts.value("command");
		if(command.isNull()) {
			kdError() << "Reply recieved, no command specified" << endl;
			return false;
		}
		if(command == "createVideoXWindow") {
			QString winID = atts.value("WinID");
			WId retID = 0;
			if(winID.isNull()) {
				kdWarning() << "Window ID not specified - emitting 0" << endl;
			} else {
				retID = winID.toInt();
			}
			emit replyCreateVideoXWindow(retID);
			m_funcStartElement = &KRender::replyCreateVideoXWindowStartElement;
			m_funcEndElement = &KRender::replyCreateVideoXWindowEndElement;
			return true;
		}
		if(command == "getFileProperties") {		
			QMap<QString, QString> map;

			map["filename"] = atts.value("filename");
			map["duration"] = atts.value("duration");

			emit replyGetFileProperties(map);

			m_funcStartElement = &KRender::replyGetFilePropertiesStartElement;
			m_funcEndElement = &KRender::replyGetFilePropertiesEndElement;
			return true;
		}
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

bool KRender::replyCreateVideoXWindowStartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	kdWarning() << "Should not recieve replyCreateVideoxWindowStartElement!" << endl;
	return false;
}

bool KRender::replyCreateVideoXWindowEndElement(const QString & namespaceURI, const QString & localName,
																									const QString & qName)
{
	kdDebug() << "Inside replyCreateVideoXWindowEndElement" << endl;

	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	m_parsing = false;	
	return true;
}

bool KRender::replyGetFilePropertiesStartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	kdWarning() << "Should not recieve replyGetFilePropertiesStartElement!" << endl;
	return false;
}

bool KRender::replyGetFilePropertiesEndElement(const QString & namespaceURI, const QString & localName,
																									const QString & qName)
{
	kdDebug() << "Inside replyGetFilePropertiesEndElement" << endl;

	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	m_parsing = false;
	return true;
}
