/***************************************************************************
                          avfile.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include "avfile.h"

AVFile::AVFile(QString name, KURL url) {
	setName(name);
	m_url = url;
}

AVFile::~AVFile(){
}

QString AVFile::fileName() {
	return m_name;
}

void AVFile::setName(QString name) {
	m_name = name;
}

KURL AVFile::fileUrl() {
	return m_url;
}