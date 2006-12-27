/***************************************************************************
                          avfile.cpp  -  description
                             -------------------
    begin                : Tue Jul 30 2002
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

#include <kdebug.h>

#include "avfile.h"

#include "docclipavfile.h"

AVFile::AVFile(const QString & name, const KURL & url):m_framesPerSecond(0)
{
    if (name.isNull()) {
	setName(url.filename());
    } else {
	setName(name);
    }

    m_referers.setAutoDelete(false);

    calculateFileProperties(QMap < QString, QString > (), NULL);
}

AVFile::~AVFile()
{
}

/** Read property of QString m_name. */
const QString & AVFile::name() const const
{
    return m_name;
}

/** Write property of QString m_name. */
void AVFile::setName(const QString & _newVal)
{
    m_name = _newVal;
}

QString AVFile::fileName() const const
{
    return m_url.fileName();
}

QPtrList < DocClipAVFile > AVFile::references()
{
    return m_referers;
}
