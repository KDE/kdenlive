/***************************************************************************
                          renderdebugpanel.h  -  description
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

#ifndef RENDERDEBUGPANEL_H
#define RENDERDEBUGPANEL_H

#include <qhbox.h>
#include <qlistbox.h>
#include <qwidgetstack.h>

/**The render debug panel captures output from a KRender object, and displays it. It can also be used to initiate commands to the server.
  *@author Jason Wood
  */

class QTextEdit;

class RenderDebugPanel : public QHBox  {
   Q_OBJECT
public: 
	RenderDebugPanel(QWidget *parent=0, const char *name=0);
	~RenderDebugPanel();
private:
  QListBox m_rendererList;
  QWidgetStack m_widgetStack;
  QMap<QString, int> m_boxNames;
  int m_nextId;
public slots: // Public slots
  /** Prints a debug (informational) message to the debug */
  void slotPrintDebug(const QString &name, const QString &message);
public slots: // Public slots
  /** Prints an error message to the debug window. */
  void slotPrintError(const QString &name, const QString &message);
  /** Prints a warning message to the debug area. */
  void slotPrintWarning(const QString &name, const QString &message);
private: // Private methods
  /** Returns the text edit widget with the given name, creating one if it doesn't exist. */
  QTextEdit * getTextEdit(const QString &name);
};

#endif
