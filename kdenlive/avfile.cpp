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

#include <qfileinfo.h>

#include <kdebug.h>

#include "avfile.h"

AVFile::AVFile(const QString name, const KURL url) :
						m_duration(0.0)
{
	if(name.isNull()) {
   	setName(url.filename());
	} else {
		setName(name);
 	}

  m_refCount = 0;
  
	m_url = url;

	calculateFileProperties(QMap<QString, QString>());
}

AVFile::~AVFile()
{
}

/** Calculates properties for this file that will be useful for the rest of the program. */
void AVFile::calculateFileProperties(QMap<QString, QString> attributes)
{
	if(m_url.isLocalFile()) {
		kdDebug() << "caluclating file properties..." << endl;
		QFileInfo fileInfo(m_url.directory(false, false) + m_url.filename());

	 	/* Determines the size of the file */
		m_filesize = fileInfo.size();

		if(attributes.contains("duration")) {
			m_duration = GenTime(attributes["duration"].toDouble());
		} else {
			// No duration known, use an arbitrary one until it is.
			m_duration = GenTime(60.0);		
		}

	} else {
		/** If the file is not local, then no file properties are currently returned */
		m_duration = GenTime(0.0);
		m_filesize = -1;
	}
}

/** Read property of QString m_name. */
const QString& AVFile::name()
{
	return m_name;
}

/** Write property of QString m_name. */
void AVFile::setName( const QString& _newVal){
	m_name = _newVal;
}

const QString AVFile::fileName() 
{
	return m_url.fileName();
}

const KURL AVFile::fileURL()
{
	return m_url;
}

/** returns the size of the file */
const signed int AVFile::fileSize() {
	return m_filesize;
}

GenTime AVFile::duration() const
{
	return m_duration;
}

int AVFile::addReference()
{
	m_refCount++;
	return numReferences();
}
/** Removes the reference of this clip from this avFile. If the reference did not exist, a warning will be issued to stderr. Returns the number of references to avFile. */
int AVFile::removeReference()
{
	m_refCount--;
	return numReferences();
}

/** Returns the number of clips which reference this avFile. */
int AVFile::numReferences()
{
	return m_refCount;
}
