/***************************************************************************
                          renderdebugpanel.cpp  -  description
                             -------------------
    begin                : Sun Jan 5 2003
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

#include "renderdebugpanel.h"

RenderDebugPanel::RenderDebugPanel(QWidget *parent, const char *name ) :
                                            QVBox(parent,name),
                                            m_textEdit(this, "edit")
{
  m_textEdit.setTextFormat(LogText);
}

RenderDebugPanel::~RenderDebugPanel()
{
}

/** Prints a warning message to the debug area. */
void RenderDebugPanel::slotPrintWarning(const QString &message)
{
  QString newmess = message;
  newmess.replace("<", "&lt;");
  newmess.replace(">", "&gt;");  
  m_textEdit.append("<font color=red>" + newmess + "</font>");
}

/** Prints an error message to the debug window. */
void RenderDebugPanel::slotPrintError(const QString &message)
{
  QString newmess = message;
  newmess.replace("<", "&lt;");
  newmess.replace(">", "&gt;");
  m_textEdit.append("<font color=red>" + newmess + "</font>");  
}

/** Prints a debug (informational) message to the debug */
void RenderDebugPanel::slotPrintDebug(const QString &message)
{
  QString newmess = message;
  newmess.replace("<", "&lt;");
  newmess.replace(">", "&gt;");
  m_textEdit.append("<font color=black>" + newmess + "</font>");  
}
