/***************************************************************************
                          AVFormatDescCodecList.cpp  -  description
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

#include "avformatdesccodeclist.h"
#include "avformatwidgetcodeclist.h"
#include "krender.h"

AVFormatDescCodecList::AVFormatDescCodecList(KRender *renderer, const QString &description, const QString &name) :
                        AVFormatDescBase(description, name),
                        m_renderer(renderer)
{
}

AVFormatDescCodecList::~AVFormatDescCodecList()
{
}

/** Constructs a widget to display this container. Most likely, a qgroupbox with a combo list box + widget stack. */
QWidget * AVFormatDescCodecList::createWidget(QWidget * parent)
{
  return new AVFormatWidgetCodecList(this, parent, m_name);
}

/** Returns the codec name list */
const QStringList & AVFormatDescCodecList::codecList()
{
  return m_codecList;
}

/** Adds a codec by name to this codec list. */
void AVFormatDescCodecList::addCodec(const QString &codec)
{
  m_codecList.append(codec);
}

/** Returns the renderer that generated this desc codec list */
KRender * AVFormatDescCodecList::renderer()
{
  return m_renderer;
}
