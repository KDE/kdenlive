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
private slots: // Private slots
  /** A slider on the ruler has changed value */
  void rulerValueChanged(int ID, int value);
signals: // Signals
  /** Emitted when the seek position has changed */
  void seekPositionChanged(GenTime time);
  /** Emitted by the EditPanel when the playSpeed should change. */
  void playSpeedChanged(double);  
private: // Private attributes
  /** The document associated with this edit panel */
  KdenliveDoc * m_document;
public slots: // Public slots
  /** Seeks to the end of the ruler */
  void seekEnd();
public slots: // Public slots
  /** Seeks to the beginning of the ruler. */
  void seekBeginning();
};

#endif
