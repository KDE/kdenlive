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
#include <qvbox.h>
#include <qcheckbox.h>
#include <qlistbox.h>
#include <qpushbutton.h>
#include <qsplitter.h>
#include <qtextedit.h>
#include <qwidgetstack.h>

/**The render debug panel captures output from a KRender object, and displays it. It can also be used to initiate commands to the server.
  *@author Jason Wood
  */

class QTextEdit;

class RenderDebugPanel:public QVBox {
  Q_OBJECT public:
    RenderDebugPanel(QWidget * parent = 0, const char *name = 0);
    ~RenderDebugPanel();

    void setIgnoreMessages(bool ignore);
    bool ignoreMessages() const;

  private:
     QHBox m_mainLayout;
    QListBox m_rendererList;
    QSplitter m_textLayout;
    QWidgetStack m_widgetStack;
    QTextEdit m_vemlEdit;
    QHBox m_buttonLayout;
    QWidget m_spacer;
    QCheckBox m_ignoreMessages;
    QPushButton m_sendVemlButton;
    QPushButton m_saveMessages;
     QMap < QString, int >m_boxNames;
    int m_nextId;
    public slots:		// Public slots
		/** Prints a debug (informational) message to the debug */
    void slotPrintRenderDebug(const char *&name, const QString & message);
		/** Prints a warning (oh oh....) message to the debug */
    void slotPrintRenderWarning(const char *&name,
	const QString & message);
		/** Prints an error (ARRGHH!) message to the debug window */
    void slotPrintRenderError(const char *&name, const QString & message);
		/** Prints a debug (informational) message to the debug */
    void slotPrintDebug(const char *&name, const QString & message);
		/** Prints an error message to the debug window. */
    void slotPrintError(const char *&name, const QString & message);
		/** Prints a warning message to the debug area. */
    void slotPrintWarning(const char *&name, const QString & message);
		/** Requests a filename from the user and saves all messages into that file. */
    void saveMessages();
    private slots:
		/** Get the contents of m_vemlEdit and send it to the currently selected renderer. This is
		mostly of use for testing piave when the functionality to do so does not exist in kdenlive.*/
    void sendDebugVeml();
  private:			// Private methods
		/** Returns the text edit widget with the given name, creating one if it doesn't exist. */
     QTextEdit * getTextEdit(const char *&name);
     signals:void debugVemlSendRequest(const QString & rendererName,
	const QString & request);
};

#endif
