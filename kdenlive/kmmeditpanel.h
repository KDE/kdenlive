/***************************************************************************
                          kmmeditpanelimplementation.h  -  description
                             -------------------
    begin                : Mon Apr 8 2002
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

#ifndef KMMEDITPANELIMPLEMENTATION_H
#define KMMEDITPANELIMPLEMENTATION_H

#include "kmmeditpanel_ui.h"

#include "gentime.h"

class KdenliveDoc;

/**Implementation for the edit panel
  *@author Jason Wood
  */

class KMMEditPanel : public KMMEditPanel_UI  {
	Q_OBJECT
public: 
	KMMEditPanel(KdenliveDoc *document, QWidget* parent = 0, const char* name = 0, WFlags fl = 0);
	~KMMEditPanel();
	/** Sets the length of the clip that we are viewing. */
	void setClipLength(int frames);
	/** Returns the inpoint time for the current clip. */
	GenTime inpoint() const;
	/** Returns the outpoint time for the current clip. */
	GenTime outpoint() const;
private slots: // Private slots
  /** A slider on the ruler has changed value */
  void rulerValueChanged(int ID, int value);
signals: // Signals
  /** Emitted when the seek position has changed */
  void seekPositionChanged(const GenTime &);
  /** Emitted when the outpoint position has changed */
  void outpointPositionChanged(const GenTime &);
  /** Emitted when the inpoint position has changed */
  void inpointPositionChanged(const GenTime &);
  /** Emitted by the EditPanel when the playSpeed should change. */
  void playSpeedChanged(double);
private: // Private attributes
  /** The document associated with this edit panel */
  KdenliveDoc * m_document;
public slots: // Public slots
  /** Seeks to the end of the ruler */
  void seekEnd();
  /** Seeks to the beginning of the ruler. */
  void seekBeginning();
  /** Steps slider value forwards one frame. */
  void stepForwards();
  /** Steps slider value one step backwards. */
  void stepBack();
  /** Called when the "stop" button has been pressed. */
  void stop();
  /** Called when the "play" button is pressed */
  void play();
  /** Sets the current seek position to the one specified */
  void seek(const GenTime &time);
  /** Alerts the edit panel that the renderer has disconnected. */
  void rendererConnected();
  /** Alerts the edit panel that the renderer has disconnected. */
  void rendererDisconnected();  
  /** Sets the outpoint position to the current seek position */
  void setOutpoint();
  /** Sets the inpoint position to the current seek position */
  void setInpoint();
  /** Sets the outpoint position to the current seek position */
  void setOutpoint(const GenTime &outpoint);
  /** Sets the inpoint position to the current seek position */
  void setInpoint(const GenTime &inpoint);
};

#endif
