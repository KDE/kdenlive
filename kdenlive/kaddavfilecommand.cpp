/***************************************************************************
                          kaddclipcommand.cpp  -  description
                             -------------------
    begin                : Fri Dec 13 2002
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

#include "kaddavfilecommand.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"

namespace Command {

/** Construct an AddClipCommand that will add or delete a clip */
KAddAVFileCommand::KAddAVFileCommand(KdenliveDoc *doc, const KURL &url, bool create) :
		m_doc(doc),
		m_url(url),
		m_create(create)
{
}

KAddAVFileCommand::KAddAVFileCommand(KdenliveDoc *doc, AVFile *avfile, bool create) :
		m_doc(doc),
		m_url(avfile->fileURL()),
		m_create(create)
{
}

KAddAVFileCommand::~KAddAVFileCommand()
{
}

/** Returns the name of this command */
QString KAddAVFileCommand::name() const
{
	if(m_create) {
		return i18n("Add Clip");
	} else {
		return i18n("Delete Clip");
	}
}

/** Execute the command */
void KAddAVFileCommand::execute()
{
	if(m_create) {
		m_doc->insertAVFile(m_url);
	} else {
		deleteAVFile();
	}
}

/** Unexecute the command */
void KAddAVFileCommand::unexecute()
{
	if(m_create) {
		deleteAVFile();
	} else {
		m_doc->insertAVFile(m_url);
	}
}

void KAddAVFileCommand::deleteAVFile()
{
	AVFile *file = m_doc->findAVFile(m_url);
	if(file) {
		m_doc->deleteAVFile(file);
	}
}

} // namespace command

