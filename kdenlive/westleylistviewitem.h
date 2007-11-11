/***************************************************************************
                          folderlistviewitem.h  -  description
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

#ifndef WESTLEYLISTVIEWITEM_H
#define WESTLEYLISTVIEWITEM_H

#include <qdom.h>
#include <qpoint.h>

#include <klistview.h>

#include "docclipbase.h"
#include "baselistviewitem.h"
#include "timecode.h"

/** Allows folders to be displayed in the project view
  *@author Jean-Baptiste Mardelle
  */

class DocClipRef;

class WestleyListViewItem:public BaseListViewItem {
  public:
    WestleyListViewItem(QListViewItem * parent, QDomElement e, int width, int height);
    WestleyListViewItem(QListViewItem * parent, QString itemName, int in, int out, Timecode tc);
    WestleyListViewItem(QListView * parent, QDomElement e, int width, int height);
    ~WestleyListViewItem();
    QString getId() const;
    bool isPlayListEntry() const;
    bool isBroken() const;
    virtual QString getInfo() const;
    DocClipBase::CLIPTYPE getType() const;
    QDomDocument getEntryPlaylist();
    QDomElement getXml() const;
    GenTime duration() const;

  private:
    void parseItem(int width, int height);
    QString getComment(Timecode tc) const;
    QString m_id;
    bool m_isBroken;
    int m_in;
    int m_out;
    QDomElement m_xml;
    KURL m_url;
    DocClipBase::CLIPTYPE m_type;
};

#endif
