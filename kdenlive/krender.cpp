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

KRender::KRender(const QString &rendererName, KURL appPath, unsigned int port, QObject *parent, const char *name ) :
																				QObject(parent, name),
																				QXmlDefaultHandler(),
                                        m_name(rendererName)
{
	startTimer(1000);
	m_parsing = false;
	m_seekPending = false;
	m_nextSeek = -1.0;
	m_setSceneListPending = false;

	m_xmlReader.setContentHandler(this);

	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	connect(&m_socket, SIGNAL(error(int)), this, SLOT(error(int)));
	connect(&m_socket, SIGNAL(connected()), this, SLOT(slotConnected()));
	connect(&m_socket, SIGNAL(connectionClosed()), this, SLOT(slotDisconnected()));  
	connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readData()));

	connect(&m_process, SIGNAL(processExited(KProcess *)), this, SLOT(processExited()));
  connect(&m_process, SIGNAL(receivedStdout(KProcess *, char *, int)), this, SLOT(slotReadStdout(KProcess *, char *, int)));
  connect(&m_process, SIGNAL(receivedStderr(KProcess *, char *, int)), this, SLOT(slotReadStderr(KProcess *, char *, int)));  

	m_portNum = port;
  m_appPath = appPath;
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
              emit recievedInfo(m_name, "Connection Refused");
							break;
		case QSocket::ErrHostNotFound :
              emit recievedInfo(m_name, "Host Not Found");
							break;
		case QSocket::ErrSocketRead :
              emit recievedInfo(m_name, "Error Reading Socket");              
							break;
	}
}

/** Called when we have connected to the renderer. */
void KRender::slotConnected()
{
  getCapabilities();

  emit recievedInfo(m_name, "Connected on port " + QString::number(m_socket.port()) + " to host on port " + QString::number(m_socket.peerPort()));  
	emit initialised();
  emit connected();
}

/** Called when we have disconnected from the renderer. */
void KRender::slotDisconnected()
{
  emit recievedInfo(m_name, "Disconnected");  

  emit disconnected();
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

    emit recievedInfo(m_name, "Parsing " + temp);
    
    m_funcStartElement = &KRender::topLevelStartElement;
    m_funcEndElement = &KRender::topLevelEndElement;    
    if(!m_xmlReader.parse(&source, false)) {
      emit recievedInfo(m_name, "Parse Failed on " + temp);      
    } else {
      emit recievedInfo(m_name, "Parse successfull");
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
  emit recievedInfo(m_name, "Render Process Exited");    
}

/** Launches a renderer process. */
void KRender::launchProcess()
{
	m_process.clearArguments();
	m_process.setExecutable("artsdsp");
  m_process << m_appPath.path();
  m_process << "-d";  
  m_process << "-p " + QString::number(m_portNum);
  m_process << "-frgb";

  emit recievedInfo(m_name, "Launching Process " + m_appPath.path() + " as server on port " + QString::number(m_portNum));
	if(m_process.start(KProcess::NotifyOnExit, KProcess::AllOutput)) {
    emit recievedInfo(m_name, "Process launching successfully, pid = " + QString::number(m_process.pid()));
    emit recievedInfo(m_name, "Connecting to server on port " + QString::number(m_portNum));
		m_socket.connectToHost("127.0.0.1", m_portNum);
	} else {
    emit recievedInfo(m_name, "Could not start process");    
	}
}

void KRender::slotReadStdout(KProcess *proc, char *buffer, int buflen)
{  
  QString mess;
  mess.setLatin1(buffer, buflen);
  emit recievedStdout(m_name, mess);
}

void KRender::slotReadStderr(KProcess *proc, char *buffer, int buflen)
{
  QString mess;
  mess.setLatin1(buffer, buflen);
  emit recievedStderr(m_name, mess);
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
	if(m_setSceneListPending) {
		sendSetSceneListCommand(m_sceneList);
	}  
	QDomDocument doc;
	QDomElement elem = doc.createElement("play");
	elem.setAttribute("speed", speed);
	doc.appendChild(elem);
	sendCommand(doc);	
}

void KRender::render(const KURL &url)
{
	if(m_setSceneListPending) {
		sendSetSceneListCommand(m_sceneList);
	}  
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
  emit recievedInfo(m_name, "Starting to parse document");  
	return true;
}

/** Occurs upon finishing reading an XML document */
bool KRender::endDocument()
{
  emit recievedInfo(m_name, "Finishing parsing document");    
	return true;
}

/** Called when the xml parser encounters an opening element */
bool KRender::startElement(const QString & namespaceURI, const QString & localName,
																	const QString & qName, const QXmlAttributes & atts )
{
  emit recievedInfo(m_name, "Discovered opening tag " + localName);   

	return (this->*m_funcStartElement)(namespaceURI, localName, qName, atts);
}

/** Called when the xml parser encounters a closing tag */
bool KRender::endElement ( const QString & namespaceURI, const QString & localName, const QString & qName )
{
  emit recievedInfo(m_name, "Discovered closing tag " + localName);     

	return (this->*m_funcEndElement)(namespaceURI, localName, qName);
}

/** Called when the xml parser encounters an opening element and we are outside of a parsing loop. */
bool KRender::topLevelStartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	if(localName == "reply") {
		QString command = atts.value("command");
		if(command.isNull()) {
      emit recievedInfo(m_name, "Reply recieved, no command specified");
			return false;
		} else if(command == "createVideoXWindow") {
			QString winID = atts.value("WinID");
			WId retID = 0;
			if(winID.isNull()) {
        emit recievedInfo(m_name, "Window ID not specified - emitting 0");        
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
		} else if(command == "getCapabilities") {
			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;      
      return true;
    } else if(command == "setSceneList") {
			m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
			m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;      
      return true;
    } else {      
    	emit recievedInfo(m_name, "Unknown reply '" + command + "'");      
      return false;
    }
	} else if(localName == "pong") {
		QString id = atts.value("id");
  	emit recievedInfo(m_name, "pong recieved : " + id);          
		m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
  	m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
    return true;
	} else if(localName == "playing") {
    QString tStr = atts.value("time");
    GenTime time(tStr.toDouble());
		m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
  	m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;
    emit positionChanged(time);
    return true;
  } else if(localName == "render") {
    QString tStr = atts.value("status");
		m_funcStartElement = &KRender::reply_GenericEmpty_StartElement;
  	m_funcEndElement = &KRender::reply_GenericEmpty_EndElement;    
    if(tStr == "playing") {
      emit playing();
      return true;
    } else if(tStr == "stopped") {
      emit stopped();
      return true;
    }
    emit recievedInfo(m_name, "Render command returned unknown status : '" + tStr + "'");    
    return false;   
  }

	emit recievedInfo(m_name, "Unknown tag '" + localName + "'");
  

	return false;
}

/** Called when we are parsing a close tag and are at the top level of the document. */
bool KRender::topLevelEndElement(const QString & namespaceURI, const QString & localName,
																									const QString & qName)
{
	emit recievedInfo(m_name, "Parsing topLevel End Element - this should not happen, ever!");
	return false;
}

bool KRender::reply_GenericEmpty_StartElement(const QString & namespaceURI, const QString & localName,
																		 const QString & qName, const QXmlAttributes & atts)
{
	emit recievedInfo(m_name, "Should not recieve reply_GenericEmpty_StartElement!");
	return false;
}

bool KRender::reply_GenericEmpty_EndElement(const QString & namespaceURI, const QString & localName,
																									const QString & qName)
{
	m_funcStartElement = &KRender::topLevelStartElement;
	m_funcEndElement = &KRender::topLevelEndElement;

	return true;
}
