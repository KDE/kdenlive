/***************************************************************************
                          configureprojectdialog  -  description
                             -------------------
    begin                : Sat Nov 15 2003
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
#ifndef CONFIGUREPROJECTDIALOG_H
#define CONFIGUREPROJECTDIALOG_H

#include <kdialogbase.h>

#include "avfileformatdesc.h"

class QSplitter;
class QVBox;
class KJanusWidget;
class KListBox;
class KPushButton;

class ExportConfig;
class ConfigureProject;

/**
Dialog for configuring the project. This configuration dialog is used for both project creation and simply changing the project configuration

@author Jason Wood
*/
class ConfigureProjectDialog : public KDialogBase
{
Q_OBJECT
public:
    ConfigureProjectDialog(QPtrList<AVFileFormatDesc> &formatList, QWidget *parent = 0, const char *name = 0, WFlags f = 0);

    ~ConfigureProjectDialog();
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
	QSplitter *m_hSplitter;
	QVBox *m_presetVBox;
	KListBox *m_presetList;
	KPushButton *m_addButton;
	KPushButton *m_deleteButton;

	KJanusWidget *m_tabArea;

	ExportConfig *m_export;
	ConfigureProject *m_config;
};

#endif
