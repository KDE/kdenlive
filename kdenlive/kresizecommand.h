/***************************************************************************
                          kresizecommand.h  -  description
                             -------------------
    begin                : Sat Dec 14 2002
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

#ifndef KRESIZECOMMAND_H
#define KRESIZECOMMAND_H

#include <kcommand.h>

#include "gentime.h"

/**This command handles the resizing of clips.
  *@author Jason Wood
  */

class KdenliveDoc;
class DocClipRef;

namespace Command {

    class KResizeCommand:public KCommand {
      public:
	KResizeCommand(KdenliveDoc * doc, DocClipRef & clip);
	~KResizeCommand();

	/** Examines the clip, and picks out the relevant size info. */
	void setEndSize(DocClipRef & clip);

  /** Unexecutes this command */
	void unexecute();
  /** Executes this command */
	void execute();
  /** Returns the name of this command */
	QString name() const;
  /** Sets the clip trackEnd for the end destination to the time specified. */
	void setEndTrackEnd(const GenTime & time);
  /** Sets the clip trackStart for the end destination to the time specified. */
	void setEndTrackStart(const GenTime & time);
      private:			// Private attributes
  /** The track number that the clip is on. */
	int m_trackNum;
  /** A time within the clip, which allows us to discover the clip */
	GenTime m_start_trackStart;
	GenTime m_start_cropStart;
	GenTime m_start_trackEnd;
	GenTime m_end_trackStart;
	GenTime m_end_cropStart;
	GenTime m_end_trackEnd;
  /** Pointer to the document */
	KdenliveDoc *m_doc;
    };

}				// namespace Command
#endif
