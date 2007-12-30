/***************************************************************************
                        kdenlivedoc.cpp  -  description
                           -------------------
  begin                : Fri Nov 22 2002
  copyright            : (C) 2002 by Jason Wood
  email                : jasonwood@blueyonder.co.uk
  copyright            : (C) 2005 Lucio Flavio Correa
  email                : lucio.correa@gmail.com
  copyright            : (C) Marco Gittler
  email                : g.marco@freenet.de
  copyright            : (C) 2006 Jean-Baptiste Mardelle
  email                : jb@ader.ch

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <KDebug>
#include <KStandardDirs>
#include <KMessageBox>
#include <KLocale>
#include <KFileDialog>
#include <KIO/NetAccess>


#include "kdenlivedoc.h"

KdenliveDoc::KdenliveDoc(KUrl url, double fps, int width, int height, QWidget *parent):QObject(parent), m_url(url), m_fps(fps), m_width(width), m_height(height), m_projectName(NULL)
{
  if (!url.isEmpty()) {
    QString tmpFile;
    if(KIO::NetAccess::download(url.path(), tmpFile, parent))
    {
      QFile file(tmpFile);
      m_document.setContent(&file, false);
      file.close();
      m_projectName = url.fileName();
 
      KIO::NetAccess::removeTempFile(tmpFile);
    }
    else
    {
      KMessageBox::error(parent, KIO::NetAccess::lastErrorString());
    }
  }
  else {
    // Creating new document
    QDomElement westley = m_document.createElement("westley");
    m_document.appendChild(westley);
    QDomElement doc = m_document.createElement("kdenlivedoc");
    doc.setAttribute("version", "0.6");
    westley.appendChild(doc);
    QDomElement props = m_document.createElement("properties");
    doc.setAttribute("width", m_width);
    doc.setAttribute("height", m_height);
    doc.setAttribute("projectfps", m_fps);
    doc.appendChild(props);
    
  }
  if (fps == 30000.0 / 1001.0 ) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) fps);
}

KdenliveDoc::~KdenliveDoc()
{
}

Timecode  KdenliveDoc::timecode()
{
  return m_timecode;
}

QString KdenliveDoc::documentName()
{
  return m_projectName;
}

QDomNodeList KdenliveDoc::producersList()
{
  return m_document.elementsByTagName("producer");
}

void KdenliveDoc::setProducers(QDomElement doc)
{
  QDomNode kdenlivedocument = m_document.elementsByTagName("kdenlivedoc").item(0);

  QDomNodeList list = m_document.elementsByTagName("producer");
  int ct = list.count();
    kDebug()<<"DELETING CHILD PRODUCERS: "<<ct;
  for (int i = 0; i < ct; i++) {
    kdenlivedocument.removeChild(list.item(0));
   }

  QDomNode n = doc.firstChild();
  ct = 0;
  while(!n.isNull()) {
     QDomElement e = n.toElement(); // try to convert the node to an element.
     if(!e.isNull() && e.tagName() == "producer") {
	 kdenlivedocument.appendChild(m_document.importNode(e, true));
	ct++;
     }
     n = n.nextSibling();
  }
  kDebug()<<"ADDING CHILD PRODS: "<<ct<<"\n";
  //kDebug()<<m_document.toString();
}

double KdenliveDoc::fps()
{
  return m_fps;
}

int KdenliveDoc::width()
{
  return m_width;
}

int KdenliveDoc::height()
{
  return m_height;
}

#include "kdenlivedoc.moc"

