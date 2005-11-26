/***************************************************************************
                          AVFormatDescCodecList.h  -  description
                             -------------------
    begin                : Tue Feb 4 2003
    copyright            : (C) 2003 by Jason Wood
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

#ifndef AVFormatDescCodecList_H
#define AVFormatDescCodecList_H

#include <avformatdescbase.h>

#include <qstringlist.h>

class KRender;

/**The Codec description lists all codecs that this class can accept, and allows differing codecs to be chosen and inputted.
  *@author Jason Wood
  */

class AVFormatDescCodecList : public AVFormatDescBase  {
public:
	AVFormatDescCodecList(KRender *renderer, const QString &description, const char * &name);
	~AVFormatDescCodecList();
  /** Constructs a widget to display this container. Most likely, a qgroupbox with a combo list box + widget stack. */
  AVFormatWidgetBase * createWidget(QWidget * parent);
  /** Returns the codec name list */
  const QStringList & codecList();
  /** Returns the renderer that generated this desc codec list */
  KRender * renderer();
  /** Adds a codec by name to this codec list. */
  void addCodec(const QString &codec);
private: // Private attributes
  /** A list of codec names in this codec list. */
  QStringList m_codecList;
  /** Holds a pointer to the renderer that generated this desc codec list. */
  KRender * m_renderer;
};

#endif
