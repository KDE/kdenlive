/***************************************************************************
                          kdenlivesetupdlg.h  -  description
                             -------------------
    begin                : Sat Dec 28 2002
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

#ifndef KDENLIVESETUPDLG_H
#define KDENLIVESETUPDLG_H

#include <qwidget.h>
#include <kdialogbase.h>

class KdenliveApp;
class RenderSetupDlg;

/**This class handles the standard "Configure Kdenlive..." dialog box.
  *@author Jason Wood
  */

class KdenliveSetupDlg : public KDialogBase  {
   Q_OBJECT
public: 
	KdenliveSetupDlg(KdenliveApp *app, QWidget *parent=0, const char *name=0);
	~KdenliveSetupDlg();
public slots: // Public slots
  /** Occurs when the apply button is clicked. */
  void slotApply();
  /** Called when the "Default" button is pressed. */
  void slotDefault();
  /** Called when the cancel button is clicked. */
  void slotCancel();
  /** Called when the ok button is clicked. */
  void slotOk();
private:
  RenderSetupDlg *m_renderDlg;
};

#endif
