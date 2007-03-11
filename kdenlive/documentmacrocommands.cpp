/***************************************************************************
                          documentmacrocommands  -  description
                             -------------------
    begin                : Sat Dec 27 2003
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
#include "documentmacrocommands.h"

#include <kcommand.h>
#include <kdebug.h>
#include <klocale.h>

#include "docclipref.h"
#include "kdenlivedoc.h"

#include "kaddrefclipcommand.h"
#include "kresizecommand.h"
#include "kselectclipcommand.h"
#include "kaddtransitioncommand.h"

namespace Command {

    DocumentMacroCommands::DocumentMacroCommands() {
    } DocumentMacroCommands::~DocumentMacroCommands() {
    }

    KCommand *DocumentMacroCommands::razorAllClipsAt(KdenliveDoc *
	document, const GenTime & time) {
	KMacroCommand *command =
	    new KMacroCommand(i18n("Razor all tracks"));

	for (uint count = 0; count < document->numTracks(); ++count) {
	    DocTrackBase *track = document->track(count);
	    KCommand *razorCommand = razorClipAt(document, *track, time);
	    if (razorCommand)
		command->addCommand(razorCommand);
	}

	return command;
    }

    KCommand *DocumentMacroCommands::razorSelectedClipsAt(KdenliveDoc *
	document, const GenTime & time) {
	KMacroCommand *command =
	    new KMacroCommand(i18n("Razor Clip"));

	for (uint count = 0; count < document->numTracks(); ++count) {
	    DocTrackBase *track = document->track(count);
	    DocClipRef *clip = track->getClipAt(time);
	    if (clip) {
		if (track->clipSelected(clip)) {
		    KCommand *razorCommand =
			razorClipAt(document, *track, time);
		    if (razorCommand)
			command->addCommand(razorCommand);
		}
	    }
	}

	return command;
    }

    KCommand *DocumentMacroCommands::razorClipAt(KdenliveDoc * document,
	DocTrackBase & track, const GenTime & time) {
	KMacroCommand *command = 0;

	DocClipRef *clip = track.getClipAt(time);
	if (clip) {
	    // disallow the creation of clips with 0 length.
	    if ((clip->trackStart() == time) || (clip->trackEnd() == time))
		return 0;

	    command = new KMacroCommand(i18n("Razor clip"));

	    DocClipRef *clone = clip->clone(document);

	    clone->moveCropStartTime(clip->cropStartTime() + (time - clip->trackStart()));
	    clone->setTrackStart(time);

	    if (clone) {
	
	    // Remove original clip's transitions that are after the cut
	    TransitionStack transitionStack = clip->clipTransitions();
	    if (!transitionStack.isEmpty()) {
    		TransitionStack::iterator itt = transitionStack.begin();
    		while (itt) {
        	    if ((*itt)->transitionStartTime()>=time) {
			Command::KAddTransitionCommand * remTransitionCommand =
		    new Command::KAddTransitionCommand(clip, *itt, false);
			command->addCommand(remTransitionCommand);
		    }
        	++itt;
    		}
	    }


		command->
		addCommand(Command::KSelectClipCommand::
		selectNone(document));

		Command::KResizeCommand * resizeCommand =
		    new Command::KResizeCommand(document, *clip);
		resizeCommand->setEndTrackEnd(time);
		command->addCommand(resizeCommand);

		command->
		    addCommand(new Command::KAddRefClipCommand(document->
			effectDescriptions(), *document, clone, true));

		delete clone;
	    } else {
		kdError() << "razorClipAt - could not clone clip!!!" <<
		    endl;
	    }
	}

	return command;
    }

}				// namespace Command
