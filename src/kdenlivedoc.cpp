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


KdenliveDoc::KdenliveDoc(KUrl url, double fps, int width, int height, QWidget *parent):QObject(parent), m_render(NULL), m_url(url), m_fps(fps), m_width(width), m_height(height), m_projectName(NULL)
{

  m_commandStack = new KUndoStack(this);
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
    /*QDomElement westley = m_document.createElement("westley");
    m_document.appendChild(westley);
    QDomElement doc = m_document.createElement("kdenlivedoc");
    doc.setAttribute("version", "0.6");
    westley.appendChild(doc);
    QDomElement props = m_document.createElement("properties");
    doc.setAttribute("width", m_width);
    doc.setAttribute("height", m_height);
    doc.setAttribute("projectfps", m_fps);
    doc.appendChild(props);*/


    QDomElement westley = m_document.createElement("westley");
    m_document.appendChild(westley);
    QDomElement prod = m_document.createElement("producer");
    prod.setAttribute("resource", "colour");
    prod.setAttribute("colour", "red");
    prod.setAttribute("id", "black");
    prod.setAttribute("in", "0");
    prod.setAttribute("out", "0");

    QDomElement tractor = m_document.createElement("tractor");
    QDomElement multitrack = m_document.createElement("multitrack");

    QDomElement playlist1 = m_document.createElement("playlist");
    playlist1.appendChild(prod);
    multitrack.appendChild(playlist1);
    QDomElement playlist2 = m_document.createElement("playlist");
    multitrack.appendChild(playlist2);
    QDomElement playlist3 = m_document.createElement("playlist");
    multitrack.appendChild(playlist3);
    QDomElement playlist4 = m_document.createElement("playlist");
    playlist4.setAttribute("hide", "video");
    multitrack.appendChild(playlist4);
    QDomElement playlist5 = m_document.createElement("playlist");
    playlist5.setAttribute("hide", "video");
    multitrack.appendChild(playlist5);
    tractor.appendChild(multitrack);
    westley.appendChild(tractor);
    
  }
  if (fps == 30000.0 / 1001.0 ) m_timecode.setFormat(30, true);
    else m_timecode.setFormat((int) fps);
}

KdenliveDoc::~KdenliveDoc()
{
}

KUndoStack *KdenliveDoc::commandStack()
{
  return m_commandStack;
}

void KdenliveDoc::setRenderer(Render *render)
{
  m_render = render;
  if (m_render) m_render->setSceneList(m_document);
}

QDomDocument KdenliveDoc::generateSceneList()
{
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement prod = doc.createElement("producer");
}

QDomDocument KdenliveDoc::toXml()
{
  return m_document;
}

Timecode KdenliveDoc::timecode()
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

