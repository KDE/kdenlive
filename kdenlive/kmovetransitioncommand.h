/***************************************************************************
                        KMoveTransitionCommand  -  description
                           -------------------
  begin                : Thu Jan 22 2004
  copyright            : (C) 2004 by Jason Wood
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
#ifndef COMMANDKMoveTransitionCommand_H
#define COMMANDKMoveTransitionCommand_H

#include <kcommand.h>

#include <gentime.h>
#include <qdom.h>

class DocClipRef;
class Transition;
class KdenliveDoc;

namespace Command {

/**
Command to add effects to clips.

@author Jason Wood
*/
    class KMoveTransitionCommand:public KCommand {
      public:
	/** Creates a KMoveTransitionCommand that will add the specified effect at the specified effectIndex. */
	 KMoveTransitionCommand(KdenliveDoc * document, DocClipRef * clip, Transition * transit, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut);
	~KMoveTransitionCommand();

	/** Returns the (translated) name of this command */
	virtual QString name() const;

	/** Unexecutes this command */
	void unexecute();
	/** Executes this command */
	void execute();

      private:

	/** The clips position on the track. */
	GenTime m_position;
	GenTime m_transitionOldStart, m_transitionOldEnd, m_transitionNewStart, m_transitionNewEnd;

	/** Track index of the document track the clip is on. */
	int m_trackIndex;
	int m_transitionIndex;

	void moveTransition(bool revert);

	KdenliveDoc *m_document;
    };

}
#endif
