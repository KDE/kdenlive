/***************************************************************************
                        KAddTransitionCommand  -  description
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
#ifndef COMMANDKAddTransitionCommand_H
#define COMMANDKAddTransitionCommand_H

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
    class KAddTransitionCommand:public KCommand {
      public:
	/** Returns an AddEffectCommand that will append the effect to the end of the effect list.*/
	static KAddTransitionCommand *appendTransition(DocClipRef * clip, GenTime time, const QString & type);
	static KAddTransitionCommand *appendTransition( DocClipRef * a_clip, DocClipRef * b_clip, const QString & type);
	static KAddTransitionCommand *removeTransition(DocClipRef * clip, Transition *transit);

	/** Constructs a command to move the specified effect to a new place in the effect list.
		@param document The document this command will act upon.
		@param clip The clip this command wil act upon.
		@param effectIndex the index of the effect that will be moved.
		@param newEffectIndex the index to which the effect will be moved.
		@returns the AddEffectCommand that will perform the operation.
	*/
/*	static KCommand *moveEffect(KdenliveDoc * document,
	    DocClipRef * clip, int effectIndex, int newEffectIndex);
*/
	/** Creates a KAddTransitionCommand that will add the specified effect at the specified effectIndex. */
	 KAddTransitionCommand(DocClipRef * clip, Transition * transit, bool add);
	~KAddTransitionCommand();

	/** Returns the (translated) name of this command */
	virtual QString name() const;

	/** Unexecutes this command */
	void unexecute();
	/** Executes this command */
	void execute();

      private:

	/** True if we are adding an Effect, false if we are removing an effect. */
	bool m_addTransition;

	/** The clips position on the track. */
	GenTime m_position;
	QDomElement m_transition;
	DocClipRef *m_clip;

	/** Add the effect to the clip. */
	void addTransition();
	/** Deletes the effect from the clip. */
	void deleteTransition();

	KdenliveDoc *m_document;
    };

}
#endif
