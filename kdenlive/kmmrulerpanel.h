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
  /** Occurs when the slider changes value, emits a corrected value to provide a non-linear (and better) value scaling. */
  void sliderScaleChange(int value);
public: // Public attributes
  /** This scale is used to convert the combo box entries to scale values. */
  static const int comboListLength;
  static const int comboScale[];
private: // Private attributes
  /** This variable is used when we are "syncing" the various widgets in the ruler
panel. Since it is unlikely that the various representations can be made exactly
equal, we instead make them as equal as possible. The m_sync variable
prevents infinte loops from occuring as the multiple widgets keep rearranging
each other's values. */
  bool m_sync;
};

#endif
