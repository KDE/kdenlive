/***************************************************************************
                          documentmacrocommands  -  description
                             -------------------
    begin                : Sat Dec 27 2003
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
#ifndef DOCUMENTMACROCOMMANDS_H
#define DOCUMENTMACROCOMMANDS_H

#include "gentime.h"

class KCommand;

class DocTrackBase;
class KdenliveDoc;

/**
A collection of commands that manipulate the document by using two or more base commands.

@author Jason Wood
*/
namespace Command {

class DocumentMacroCommands{
public:
	/** Razors the clip that is on the specified track, at the specified time. */
	static KCommand *razorClipAt( KdenliveDoc *document, DocTrackBase &track, const GenTime &time );
	/** Razor all clips which are at the time specified, irrespective of which track they are on. */
	static KCommand *razorAllClipsAt( KdenliveDoc *document, const GenTime &time );
	/** Razor all selected clips which are at the time specified, irrespective of which track they are on. */
	static KCommand *razorSelectedClipsAt( KdenliveDoc *document, const GenTime &time );

    DocumentMacroCommands();
    ~DocumentMacroCommands();

};

} // namespace Command

#endif
