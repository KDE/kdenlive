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

#ifndef AVLISTVIEWITEM_H
#define AVLISTVIEWITEM_H

#include <klistview.h>
#include "baselistviewitem.h"

class DocClipRef;
class DocumentBaseNode;


/**Allows clips to be displayed in a QListView
  *@author Jason Wood
  */

class AVListViewItem:public BaseListViewItem {
  public:
	/** Create an AVListViewItem. Note that AVList takes ownership of the clip passed in. */
    AVListViewItem(QListViewItem * parent,
	DocumentBaseNode * node);
    AVListViewItem(QListView * parent,
	DocumentBaseNode * node);
    ~AVListViewItem();
    virtual void setText(int column, const QString & text);
    virtual QString text(int column) const;
    virtual const QPixmap *pixmap(int column) const;
    virtual QString key ( int , bool ) const;
    DocClipRef *clip() const;
    virtual void paintCell(QPainter *p, const QColorGroup &cg, int column, int width, int align);

    virtual QString getInfo() const;


  private:
    void doCommonCtor();
    QString clipDuration() const;
    QListView *m_listView;
    DocumentBaseNode *m_node;
};

#endif
