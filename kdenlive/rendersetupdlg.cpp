/***************************************************************************
                          rendersetupdlg.cpp  -  description
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

#include "rendersetupdlg.h"
#include "kdenlive.h"
#include "kurlrequester.h"
#include "knuminput.h"

#include <kdebug.h>

RenderSetupDlg::RenderSetupDlg(KdenliveApp *app, QWidget *parent, const char *name ) : RenderSetupDlg_UI(parent,name)
{
  m_app = app;

  readSettings();
}

RenderSetupDlg::~RenderSetupDlg()
{
}

/** Write the current settings back to the application */
void RenderSetupDlg::writeSettings()
{
  kdDebug() << "Writing url " << m_appPathBrowser->url() << endl;
  m_app->setRenderAppPath(KURL(m_appPathBrowser->url()));
  m_app->setRenderAppPort(m_appPortNum->value());
}

/** Read the settings from the application */
void RenderSetupDlg::readSettings()
{
  m_appPathBrowser->setURL(m_app->renderAppPath().path());
  m_appPortNum->setValue(m_app->renderAppPort());      
}
