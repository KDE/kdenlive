/***************************************************************************
                          kaddmarkercommand  -  Command to add/remove a marker from a clip.
                             -------------------
    begin                : Sat Nov 29 2003
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
#ifndef COMMANDKADDMARKERCOMMAND_H
#define COMMANDKADDMARKERCOMMAND_H

#include <kcommand.h>

#include "gentime.h"

class KdenliveDoc;
class DocClipRef;


namespace Command {

/**
@author Jason Wood
*/
class KAddMarkerCommand : public KCommand
{
public:
    KAddMarkerCommand(KdenliveDoc &document, DocClipRef *clip, const GenTime &clipTime, bool create=true);

    ~KAddMarkerCommand();

	/** Unexecute the command */
	void unexecute();
	/** Execute the command */
	void execute();
	/** Returns the name of this command */
	QString name() const;
private:
	KdenliveDoc &m_document;
	/** True if this command creates a marker when executed, false otherwise. */
	bool m_create;

	/** The time (in clip time) that the marker should be placed at. */
	GenTime m_clipTime;

	/** A time on the track where the clip in question exists. */
	GenTime m_trackTime;

	/** The track on which the marker is to be placed. */
	int m_track;

	/** Adds the marker that this command is supposed to add. */
	void addMarker();

	/** Removes the marker this command is supposed to remove. */
	void deleteMarker();
};

}

#endif
