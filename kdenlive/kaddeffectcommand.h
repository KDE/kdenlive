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

namespace Command
{

/**
Command to add effects to clips.

@author Jason Wood
*/
class KAddEffectCommand : public KCommand
{
public:
	/** Returns an AddEffectCommand that will append the effect to the end of the effect list.*/
	static KAddEffectCommand *appendEffect(KdenliveDoc *document, DocClipRef *clip, Effect *effect);

	/** Creates a KAddEffectCommand that will add the specified effect at the specified effectIndex. */
	KAddEffectCommand( KdenliveDoc *document, DocClipRef *clip, int effectIndex, Effect *effect);

	/** Creates a KAddEffectCommand that will remove the effect at the specified index */
	KAddEffectCommand( KdenliveDoc *document, DocClipRef *clip, int effectIndex);

	~KAddEffectCommand();

	/** Returns the (translated) name of this command */
	virtual QString name() const;

	/** Unexecutes this command */
	void unexecute();
	/** Executes this command */
	void execute();
private:
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

};

#endif
