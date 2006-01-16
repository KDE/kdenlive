/***************************************************************************
                        kaddeffectcommand  -  description
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
#ifndef COMMANDKADDEFFECTCOMMAND_H
#define COMMANDKADDEFFECTCOMMAND_H

#include <kcommand.h>

#include <gentime.h>
#include <qdom.h>

class DocClipRef;
class Effect;
class KdenliveDoc;

namespace Command {

/**
Command to add effects to clips.

@author Jason Wood
*/
    class KAddEffectCommand:public KCommand {
      public:
	/** Returns an AddEffectCommand that will append the effect to the end of the effect list.*/
	static KAddEffectCommand *appendEffect(KdenliveDoc * document,
	    DocClipRef * clip, Effect * effect);

	/** Returns an AddEffectCommand that will insert the effect at the specified index in the effect list.*/
	static KAddEffectCommand *insertEffect(KdenliveDoc * document,
	    DocClipRef * clip, int effectIndex, Effect * effect);

	/** Returns an AddEffectCommand that will insert the effect at the specified index in the effect list.*/
	static KAddEffectCommand *removeEffect(KdenliveDoc * document,
	    DocClipRef * clip, int effectIndex);

	/** Constructs a command to move the specified effect to a new place in the effect list.
		@param document The document this command will act upon.
		@param clip The clip this command wil act upon.
		@param effectIndex the index of the effect that will be moved.
		@param newEffectIndex the index to which the effect will be moved.
		@returns the AddEffectCommand that will perform the operation.
	*/
	static KCommand *moveEffect(KdenliveDoc * document,
	    DocClipRef * clip, int effectIndex, int newEffectIndex);

	~KAddEffectCommand();

	/** Returns the (translated) name of this command */
	virtual QString name() const;

	/** Unexecutes this command */
	void unexecute();
	/** Executes this command */
	void execute();
      private:
	/** Creates a KAddEffectCommand that will add the specified effect at the specified effectIndex. */
	 KAddEffectCommand(KdenliveDoc * document, DocClipRef * clip,
	    int effectIndex, Effect * effect);

	/** Creates a KAddEffectCommand that will remove the effect at the specified index */
	 KAddEffectCommand(KdenliveDoc * document, DocClipRef * clip,
	    int effectIndex);

	/** True if we are adding an Effect, false if we are removing an effect. */
	bool m_addEffect;

	/** Xml representation of the effect this command stores. */
	QDomDocument m_effect;

	/** Track index of the document track the clip is on. */
	int m_trackIndex;

	/** The clips position on the track. */
	GenTime m_position;

	/** The clips effect list index where this clip will be inserted or deleted. */
	int m_effectIndex;

	/** Add the effect to the clip. */
	void addEffect();
	/** Deletes the effect from the clip. */
	void deleteEffect();

	KdenliveDoc *m_document;
    };

}
#endif
