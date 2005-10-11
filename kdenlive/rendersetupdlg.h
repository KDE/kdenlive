/***************************************************************************
                          rendersetupdlg.h  -  description
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

#ifndef RENDERSETUPDLG_H
#define RENDERSETUPDLG_H

#include <qwidget.h>
#include <rendersetupdlg_ui.h>

class KRenderManager;

namespace Gui
{
	class KdenliveApp;

/**Contains the Setup dialog for the renderer.
  *@author Jason Wood
  */

class RenderSetupDlg : public RenderSetupDlg_UI  {
   Q_OBJECT
public:
  RenderSetupDlg(KRenderManager *renderManager, QWidget *parent=0, const char *name=0);
  ~RenderSetupDlg();
  /** Read the settings from the application */
  void readSettings();
  /** Write the current settings back to the application */
  void writeSettings();
private:
  KRenderManager *m_renderManager;
};

} // namespace Gui

#endif
