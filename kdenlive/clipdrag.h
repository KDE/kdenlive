/***************************************************************************
                          clipdrag.h  -  description
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

#ifndef CLIPDRAG_H
#define CLIPDRAG_H

#include <kurldrag.h>

class KdenliveDoc;
class DocClipBase;
class DocClipBaseList;
class AVFile;

/**Allows the dragging of clips within and outside of the application.
  *@author Jason Wood
  */

class ClipDrag : public KURLDrag
{
public: 
	ClipDrag(DocClipBase *clip, QWidget *dragSource, const char *name);
   ClipDrag(AVFile * clip, QWidget * dragSource, const char * name);
	~ClipDrag();
	/** Set the clip which is contained within this ClipDrag object. */
	void setClip(DocClipBase *clip);
	/** Returns true if the mime type is decodable, false otherwise. */
	static bool canDecode(const QMimeSource *mime);
	/** Attempts to decode the mimetype e as a clip. Returns a clip, or returns null */
	static DocClipBaseList decode(KdenliveDoc &doc, const QMimeSource *e);
  /** Constructs a clipDrag object consisting of the clips within the
DocCLipBaseList passed. */
   ClipDrag(DocClipBaseList &clips, QWidget *dragSource, const char *name);
protected:
	/** Reimplemented for internal reasons; the API is not affected.  */
	QByteArray encodedData(const char *mime) const;
	/** Reimplemented for internal reasons; the API is not affected.  */
	virtual const char * format(int i) const;
private: // Private methods
 	/** Returns a QValueList containing the URL of the clip.
	 *
	 * This is necessary, because the KURLDrag class which ClipDrag inherits
	 * requires a list of URL's rather than a single URL. 
	 **/
	static KURL::List createURLList(DocClipBaseList *clipList);
	static KURL::List createURLList(DocClipBase *clip);
	static KURL::List createURLList(AVFile *clip);

	/** Holds the XML representation of the clips being dragged */
	QString m_xml;
};

#endif
