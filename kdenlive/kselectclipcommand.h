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
class DocClipBase;

/**This command is responsible for the selection/deselection of clips on the timeline.
  *@author Jason Wood
  */

class KSelectClipCommand : public KCommand  {
public:
	KSelectClipCommand(KdenliveDoc *doc, DocClipBase *clip, bool select);
	~KSelectClipCommand();
  /** Rerturns the (translated) name of this command */
  QString name() const;
  /** Unexecutes the command */
  void unexecute();
  /** Executes the command */
  void execute();	
private: // Private attributes
  /** True if this clip executes a "select clip" command, false if it executes a "deselect clip" command. */
  bool m_selectClip;
  /** Holds a pointer to the document */
  KdenliveDoc *m_doc;
  /** The track index */
  int m_track;
  /** A time on the track which lies within the clip to be selected/deselected */
  GenTime m_findTime;
};

#endif
