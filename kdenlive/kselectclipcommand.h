/***************************************************************************
                         kselectclipcommand.h  -  description
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

#ifndef KSELECTCLIPCOMMAND_H
#define KSELECTCLIPCOMMAND_H

#include <kcommand.h>
#include <gentime.h>

class KdenliveDoc;
class DocClipRef;
class DocTrackBase;

/**This command is responsible for the selection/deselection of clips on the timeline.
  *@author Jason Wood
  */

namespace Command {

    class KSelectClipCommand:public KCommand {
      public:
	/** Returns a command that will deselect all clips in the selected document. */
	static KCommand *selectNone(KdenliveDoc * document);

	/** Selects all clips in the document that occur later than the specified time. If include is true
	 , then clips which overlap the specified time will be selected, otherwise only those clips which
	are later in the will be selected. */
	static KCommand *selectLaterClips(KdenliveDoc * document,
	    GenTime time, bool include);

	static KCommand *selectTrackLaterClips(KdenliveDoc * document, int ix, GenTime time, bool include);

    	static KCommand *selectRectangleClips(KdenliveDoc * document, int startTrack, int endTrack, GenTime startTime,  GenTime endTime, bool include);

	/** Selects the clip on the given track at the given value. */
	static KCommand *selectClipAt(KdenliveDoc * document,
	    const DocTrackBase & track, const GenTime & value);

	/** Toggle Selects the clip on the given track and at the given value. The clip will become
	selected if it wasn't already selected, and will be deselected if it is. */
	static KCommand *toggleSelectClipAt(KdenliveDoc * document,
	    const DocTrackBase & track, const GenTime & value);

	 KSelectClipCommand(KdenliveDoc * doc, DocClipRef * clip,
	    bool select);
	~KSelectClipCommand();
	/** Rerturns the (translated) name of this command */
	QString name() const;
	/** Unexecutes the command */
	void unexecute();
	/** Executes the command */
	void execute();
      private:			// Private attributes
	/** True if this clip executes a "select clip" command, false if it executes a "deselect clip" command. */
	 bool m_selectClip;
	/** Holds a pointer to the document */
	KdenliveDoc *m_doc;
	/** The track index */
	int m_track;
	/** A time on the track which lies within the clip to be selected/deselected */
	GenTime m_findTime;
	/** This is the selection state that the clip should be reverted to upon
	deselection. This is not always the opposite state to the execute
	state. */
	bool m_unexecuteSelection;
    };

}				// namespace Command
#endif
