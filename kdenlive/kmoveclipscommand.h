/***************************************************************************
                         kmoveclipscommand.h  -  description
                            -------------------
   begin                : Thu Dec 12 2002
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

#ifndef KMOVECLIPSCOMMAND_H
#define KMOVECLIPSCOMMAND_H

#include <qwidget.h>
#include <qstring.h>
#include <kcommand.h>
#include <gentime.h>

class KdenliveDoc;
class DocClipRef;

namespace Command
{

/**This command handles the moving of clips, and the "unexecuting" of command, for undo/redo pruposes.
  *@author Jason Wood
  */

class KMoveClipsCommand : public KCommand
{
public:
	KMoveClipsCommand( KdenliveDoc *doc, DocClipRef *master );
	~KMoveClipsCommand();
	/** Returns the (translated) name of this command */
	virtual QString name() const;
	/** Unexecute this command */
	void unexecute();
	/** Executes this command */
	void execute();
	/** Specifies where the master clip should be moved to by this command. */
	void setEndLocation( DocClipRef *master );
private:
	KdenliveDoc *m_doc;

	/** The track the master clip is on before execution */
	int m_startTrack;
	/** The start of the master clip before execution */
	GenTime m_startTime;

	/** The track the master clip is on after execution */
	int m_endTrack;
	/** The start of the master clip after execution */
	GenTime m_endTime;
};

} // namespace Command

#endif
