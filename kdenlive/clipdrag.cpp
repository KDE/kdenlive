/***************************************************************************
                          clipdrag.cpp  -  description
                             -------------------
    begin                : Mon Apr 15 2002
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

#include "clipdrag.h"

ClipDrag::ClipDrag(DocClipBase *clip, QWidget *dragSource, const char *name) :
											KURLDrag(createURLList(clip), dragSource, name)
{
}

ClipDrag::~ClipDrag()
{
}

/** Reimplemented for internal reasons; the API is not affected.  */
const char * ClipDrag::format(int i) const
{
	switch (i)
  {
		case 0 		: return "application/x-kdenlive-clip";
		default 	:	return KURLDrag::format(i-1);
	}
}

/** Reimplemented for internal reasons; the API is not affected.  */
QByteArray ClipDrag::encodedData(const char *mime) const
{
}

/** Set the clip which is contained within this ClipDrag object. */
void ClipDrag::setClip(DocClipBase *clip)
{
}

/** Returns true if the mime type is decodable, false otherwise. */
bool ClipDrag::canDecode(const QMimeSource *mime)
{
}

/** Attempts to decode the mimetype e as a clip. Returns true, or false. */
bool ClipDrag::decode(const QMimeSource *e, DocClipBase &clip)
{
}
/** Returns a QValueList containing the URL of the clip.

This is necessary, because the KURLDrag class which ClipDrag inherits
requires a list of URL's rather than a single URL. */
KURL::List ClipDrag::createURLList(DocClipBase *clip)
{
	KURL::List list;
	
 	list.append(clip->avFile()->fileUrl());
	return list;
}
