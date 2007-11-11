/***************************************************************************
                         krazorclipscommand.h  -  description
                            -------------------

   copyright            : (C) 2007 by Jean-Baptiste Mardelle
   email                : jb@kdenlive.org
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KRAZORCLIPSCOMMAND_H
#define KRAZORCLIPSCOMMAND_H

#include <qwidget.h>
#include <qstring.h>
#include <kcommand.h>

#include <gentime.h>
#include <kdenlive.h>

class KdenliveDoc;
class KdenliveApp;
class DocClipRef;
class DocClipRefList;

namespace Command {

/**This command handles the splitting of clips, and the "unexecuting" of command, for undo/redo purposes.
  *@author Jean-Baptiste Mardelle
  */

    class KRazorClipsCommand:public KCommand {
      public:
	KRazorClipsCommand(Gui::KdenliveApp * app, KdenliveDoc * doc, DocTrackBase & track,
	GenTime time);
	~KRazorClipsCommand();
	/** Returns the (translated) name of this command */
	virtual QString name() const;
	/** Unexecute this command */
	void unexecute();
	/** Executes this command */
	void execute();


      private:
	KdenliveDoc * m_doc;
        Gui::KdenliveApp * m_app;
	/** The track the master clip is on */
	int m_trackNum;
	int m_playlistTrackNum;
	/** The start of the master clip before execution */
	GenTime m_startTime;
	/** The end of the master clip after execution */
	GenTime m_endTime;
	/** The split time */
	GenTime m_splitTime;
	/** The clip crop start time */
	GenTime m_cropStart;
    };

}				// namespace Command
#endif
