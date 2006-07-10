/***************************************************************************
0                          kaddclipcommand.h  -  description
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

#ifndef KADDREFCLIPCOMMAND_H
#define KADDREFCLIPCOMMAND_H

#include <kcommand.h>
#include <klocale.h>
#include <qdom.h>

#include "gentime.h"

class ClipManager;
class DocClipProject;
class DocClipRef;
class EffectDescriptionList;
class KdenliveDoc;

/**Adds a clip to the document
  *@author Jason Wood
  */

namespace Command {

    class KAddRefClipCommand:public KCommand {
      public:
	/**
	Returns a command that will delete the currently selected clips on the timeline.
	*/
	static KMacroCommand *deleteSelectedClips(KdenliveDoc * document);
	static KMacroCommand *deleteAllTrackClips(KdenliveDoc *document, int ix);
	/** Construct an AddClipCommand that will delete a clip */
	 KAddRefClipCommand(const EffectDescriptionList & effectList,
	    ClipManager & clipManager,
	    DocClipProject * project,
	    DocClipRef * clip, bool create = true);

	~KAddRefClipCommand();
	/** Unexecute the command */
	void unexecute();
	/** Execute the command */
	void execute();
	/** Returns the name of this command */
	QString name() const;

      private:			// Private attributes
	 ClipManager & m_clipManager;
	const EffectDescriptionList & m_effectList;
	/** If true, then executing the command will create a clip, and
		unexecuting the command will delete a clip. Otherwise, it will be the
		other way around. */
	bool m_create;
	/** An xml representation of the clip */
	QDomDocument m_xmlClip;
	/** A time within the clip so that we can find it when we want to delete it. */
	GenTime m_findTime;
	/** The track that the clip to be deleted is on. */
	int m_track;
	/** The project this command acts upon. */
	DocClipProject *m_project;

	/** Deletes the clip */
	void deleteClip();
	/** Adds the clip */
	void addClip();
    };

}				// namespace command
#endif
