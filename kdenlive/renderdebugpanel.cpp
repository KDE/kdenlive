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

#include <qtextedit.h>
#include <kdebug.h>

RenderDebugPanel::RenderDebugPanel(QWidget *parent, const char *name ) :
                                            QHBox(parent,name),
                                            m_rendererList(this, "list"),
                                            m_widgetStack(this, "stack"),
                                            m_nextId(0)                                            
{
  m_rendererList.setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
  m_widgetStack.setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));  
  connect(&m_rendererList, SIGNAL(highlighted(int)), &m_widgetStack, SLOT(raiseWidget(int)));
}

RenderDebugPanel::~RenderDebugPanel()
{
}

/** Prints a warning message to the debug area. */
void RenderDebugPanel::slotPrintWarning(const QString &name, const QString &message)
{
  QTextEdit *edit = getTextEdit(name);
  QString newmess = message;
  
/*newmess.replace("<", "&lt;");
  newmess.replace(">", "&gt;");  
  edit->append("<font color=red>" + newmess + "</font>");*/
  if(edit) {  
    edit->setColor(QColor(0, 0, 0));
    edit->append(newmess);
  } else {
    kdError() << "Render Debug Edit box does not exist" << endl;
  }  
}

/** Prints an error message to the debug window. */
void RenderDebugPanel::slotPrintError(const QString &name, const QString &message)
{
  QTextEdit *edit = getTextEdit(name);
  QString newmess = message;
/*newmess.replace("<", "&lt;");
  newmess.replace(">", "&gt;");
  edit->append("<font color=red>" + newmess + "</font>");*/
  if(edit) {
    edit->setColor(QColor(255, 0, 0));  
    edit->append(newmess);
  } else {
    kdError() << "Render Debug Edit box does not exist" << endl;
  }
}

/** Prints a debug (informational) message to the debug */
void RenderDebugPanel::slotPrintDebug(const QString &name, const QString &message)
{
  QTextEdit *edit = getTextEdit(name);  
  QString newmess = message;
/*newmess.replace("<", "&lt;");
  newmess.replace(">", "&gt;");
  edit->append("<font color=black>" + newmess + "</font>");*/
  if(edit) {  
    edit->setColor(QColor(0, 128, 0));  
    edit->append(newmess);
  } else {
    kdError() << "Render Debug Edit box does not exist" << endl;
  }  
}

/** Returns the text edit widget with the given name, creating one if it doesn't exist. */
QTextEdit * RenderDebugPanel::getTextEdit(const QString &name)
{
  QTextEdit *edit;

  if(!m_boxNames.contains(name)) {
    edit = new QTextEdit(&m_widgetStack, name);
    //edit->setTextFormat(LogText);
    edit->setReadOnly(true);
    m_widgetStack.addWidget(edit, m_nextId);
    m_rendererList.insertItem(name, m_nextId);
    m_boxNames[name] = m_nextId;
    ++m_nextId;
  } else {
    edit = (QTextEdit *)m_widgetStack.widget(m_boxNames[name]);
  }

  return edit;
}
