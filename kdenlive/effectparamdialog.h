/***************************************************************************
                          effectparamdialog.h  -  description
                             -------------------
    begin                : Sun Feb 9 2003
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

#ifndef EFFECTPARAMDIALOG_H
#define EFFECTPARAMDIALOG_H

#include <qwidget.h>
#include <qframe.h>

/**The effect param dialog displays the parameter settings for a particular effect. This may be in relation to a clip, or it may be in relation to the effect dialog.
  *@author Jason Wood
  */

class EffectParamDialog : public QFrame  {
   Q_OBJECT
public: 
	EffectParamDialog(QWidget *parent=0, const char *name=0);
	~EffectParamDialog();
};

#endif
