/***************************************************************************
                          kmmrulerpanel.h  -  description
                             -------------------
    begin                : Sat Sep 14 2002
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

#ifndef KMMRULERPANEL_H
#define KMMRULERPANEL_H

#include <qwidget.h>

#include "kmmrulerpanel_ui.h"

/**The zoom panel contains various options to zoom the Timeline ruler to various scales
  *@author Jason Wood
  */

class KMMRulerPanel : public KMMRulerPanel_UI  {
   Q_OBJECT
public: 
	KMMRulerPanel(QWidget *parent=0, const char *name=0);
	~KMMRulerPanel();
signals: // Signals
  /** emits the newly requested time scale. */
  void timeScaleChanged(int);
public slots: // Public slots
  /** takes index and figures out the correct scale value from it, which then get's emitted. */
  void comboScaleChange(int index);
public: // Public attributes
  /** This scale is used to convert the combo box entries to scale values. */
  static int comboScale[];
};

#endif
