/***************************************************************************
                          avlistviewitem.cpp  -  description
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

#include <qpixmap.h>
#include <qpainter.h>
#include <qheader.h>
#include <qimage.h>


#include <kglobal.h>
#include <kurl.h>
#include <kiconloader.h>
#include <klocale.h>

#include "gentime.h"
#include "kdenlivesettings.h"
#include "westleylistviewitem.h"


WestleyListViewItem::WestleyListViewItem(QListViewItem * parent, QDomElement e, int width, int height):
BaseListViewItem(parent, BaseListViewItem::PLAYLISTITEM), m_in(-1), m_out(-1), m_xml(e)
{
    parseItem(width, height);
}

WestleyListViewItem::WestleyListViewItem(QListView * parent, QDomElement e, int width, int height):
BaseListViewItem(parent, BaseListViewItem::PLAYLISTITEM), m_in(-1), m_out(-1), m_xml(e)
{
    parseItem(width, height);
}

WestleyListViewItem::WestleyListViewItem(QListViewItem * parent, QString itemName, int in, int out, Timecode tc):
BaseListViewItem(parent, BaseListViewItem::PLAYLISTITEM), m_in(in), m_out(out), m_id(QString::null)
{
    m_type = ((WestleyListViewItem *) parent)->getType();
    setText(1, getComment(tc));
}

WestleyListViewItem::~WestleyListViewItem()
{
}

void WestleyListViewItem::parseItem(int width, int height)
{
	m_id = m_xml.attribute("id", QString::null);
	m_type = DocClipBase::CLIPTYPE(m_xml.attribute("type", QString::number(-1)).toInt());
	
		if (m_type == DocClipBase::COLOR) {
			    QPixmap pix = QPixmap(width, height);
			    QString col = m_xml.attribute("colour", QString::null);
        		    col = col.replace(0, 2, "#");
        		    pix.fill(QColor(col.left(7)));
			    setText(1, DocClipBase::getTypeName(m_type));
			    setPixmap(0, pix);
		}
		else if (m_type == DocClipBase::AUDIO || m_type == DocClipBase::AV || m_type == DocClipBase::VIDEO || m_type == DocClipBase::PLAYLIST) {
			    m_url = KURL(m_xml.attribute("resource", QString::null));
			    setText(1, m_url.fileName());
	 	}
		else if (m_type == DocClipBase::TEXT) {
			    m_url = KURL(m_xml.attribute("resource", QString::null));
			    setText(1, m_url.fileName());
			    QImage i(m_url.path());
			    QPixmap pix(width, height);
			    pix.convertFromImage(i.smoothScale(width, height));
			    setPixmap(0, pix);
		}
		else if (m_type == DocClipBase::IMAGE) {
			    m_url = KURL(m_xml.attribute("resource", QString::null));
			    setText(1, m_url.fileName());
			    QImage i(m_url.path());
			    QPixmap pix(width, height);
			    pix.convertFromImage(i.smoothScale(width, height));
			    setPixmap(0, pix);
		}
		else if (m_type == DocClipBase::SLIDESHOW) {
			    setText(1, DocClipBase::getTypeName(m_type));
		}
		else {
			    m_url =  KURL(m_xml.attribute("resource", QString::null));
			    setText(1, m_url.fileName());
		}
}


QString WestleyListViewItem::getId() const
{
    return m_id;
}

DocClipBase::CLIPTYPE WestleyListViewItem::getType() const
{
    return m_type;
}

bool WestleyListViewItem::isPlayListEntry() const
{
    return (m_out != -1);
}

QString WestleyListViewItem::getInfo() const
{
    QString text = "<b>"+i18n("Playlist Item\n") + DocClipBase::getTypeName(m_type) +"</b><br>";
    return text;
}

QString WestleyListViewItem::getComment(Timecode tc) const {
	QString text;

	double fps = KdenliveSettings::defaultfps();
	text = tc.getTimecode(GenTime(m_in, fps), fps) + "-";
	text += tc.getTimecode(GenTime(m_out, fps), fps);
        text += " ( " + Timecode::getEasyTimecode(GenTime(m_out - m_in, fps), KdenliveSettings::defaultfps()) + " )";
	return text;
}

