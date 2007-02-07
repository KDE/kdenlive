/***************************************************************************
                          avlistviewitem.h  -  description
                             -------------------
    begin                : Wed Mar 20 2002
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

#ifndef AVICONVIEWITEM_H
#define AVICONVIEWITEM_H

#include <kiconview.h>

class DocClipRef;
class DocumentBaseNode;
class KdenliveDoc;

/**Allows clips to be displayed in a QListView
  *@author Jason Wood
  */

class AVIconViewItem:public KIconViewItem {
  public:
	/** Create an AVIconViewItem. Note that AVList takes ownership of the clip passed in. */
    AVIconViewItem(KdenliveDoc * doc, QIconViewItem * parent,
	DocumentBaseNode * node);
    AVIconViewItem(KdenliveDoc * doc, QIconView * parent,
	DocumentBaseNode * node);
    ~AVIconViewItem();
    DocClipRef *clip() const;
    QString getInfo() const;
    virtual QString text() const;
    virtual QPixmap *pixmap() const;


  private:
    void doCommonCtor();
    QString clipDuration() const;
    QIconView *m_iconView;
    DocumentBaseNode *m_node;
    KdenliveDoc *m_doc;
};

#endif
