/***************************************************************************
                          kdenlivedoc.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 01:46:16 GMT 2002
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

// include files for Qt
#include <qdir.h>
#include <qwidget.h>

// include files for KDE
#include <klocale.h>
#include <kmessagebox.h>
#include <kio/job.h>
#include <kio/netaccess.h>

// application specific includes
#include "kdenlivedoc.h"
#include "kdenlive.h"
#include "kdenliveview.h"

QList<KdenliveView> *KdenliveDoc::pViewList = 0L;

KdenliveDoc::KdenliveDoc(QWidget *parent, const char *name) : QObject(parent, name)
{
  if(!pViewList)
  {
    pViewList = new QList<KdenliveView>();
  }

  pViewList->setAutoDelete(true);
}

KdenliveDoc::~KdenliveDoc()
{
}

void KdenliveDoc::addView(KdenliveView *view)
{
  pViewList->append(view);
}

void KdenliveDoc::removeView(KdenliveView *view)
{
  pViewList->remove(view);
}
void KdenliveDoc::setURL(const KURL &url)
{
  doc_url=url;
}

const KURL& KdenliveDoc::URL() const
{
  return doc_url;
}

void KdenliveDoc::slotUpdateAllViews(KdenliveView *sender)
{
  KdenliveView *w;
  if(pViewList)
  {
    for(w=pViewList->first(); w!=0; w=pViewList->next())
    {
      if(w!=sender)
        w->repaint();
    }
  }

}

bool KdenliveDoc::saveModified()
{
  bool completed=true;

  if(modified)
  {
    KdenliveApp *win=(KdenliveApp *) parent();
    int want_save = KMessageBox::warningYesNoCancel(win,
                                         i18n("The current file has been modified.\n"
                                              "Do you want to save it?"),
                                         i18n("Warning"));
    switch(want_save)
    {
      case KMessageBox::Yes:
           if (doc_url.fileName() == i18n("Untitled"))
           {
             win->slotFileSaveAs();
           }
           else
           {
             saveDocument(URL());
       	   };

       	   deleteContents();
           completed=true;
           break;

      case KMessageBox::No:
           setModified(false);
           deleteContents();
           completed=true;
           break;

      case KMessageBox::Cancel:
           completed=false;
           break;

      default:
           completed=false;
           break;
    }
  }

  return completed;
}

void KdenliveDoc::closeDocument()
{
  deleteContents();
}

bool KdenliveDoc::newDocument()
{
  /////////////////////////////////////////////////
  // TODO: Add your document initialization code here
  /////////////////////////////////////////////////
  modified=false;
  doc_url.setFileName(i18n("Untitled"));

  return true;
}

bool KdenliveDoc::openDocument(const KURL& url, const char *format /*=0*/)
{
  QString tmpfile;
  KIO::NetAccess::download( url, tmpfile );
  /////////////////////////////////////////////////
  // TODO: Add your document opening code here
  /////////////////////////////////////////////////

  KIO::NetAccess::removeTempFile( tmpfile );

  modified=false;
  return true;
}

bool KdenliveDoc::saveDocument(const KURL& url, const char *format /*=0*/)
{
  /////////////////////////////////////////////////
  // TODO: Add your document saving code here
  /////////////////////////////////////////////////

  modified=false;
  return true;
}

void KdenliveDoc::deleteContents()
{
  /////////////////////////////////////////////////
  // TODO: Add implementation to delete the document contents
  /////////////////////////////////////////////////

}
