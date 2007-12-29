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

KdenliveDoc::KdenliveDoc(KUrl url, double fps, int width, int height, QWidget *parent):QObject(parent), m_url(url), m_projectName(NULL)
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
    KMessageBox::error(parent, 
        KIO::NetAccess::lastErrorString());
  }
  }
}

KdenliveDoc::~KdenliveDoc()
{
}

QString KdenliveDoc::documentName()
{
  return m_projectName;
}

QDomNodeList KdenliveDoc::producersList()
{
  return m_document.elementsByTagName("producer");
}

#include "kdenlivedoc.moc"

