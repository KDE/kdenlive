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

#include <qwidget.h>

/**The render debug panel captures output from a KRender object, and displays it. It can also be used to initiate commands to the server.
  *@author Jason Wood
  */

class RenderDebugPanel : public QWidget  {
   Q_OBJECT
public: 
	RenderDebugPanel(QWidget *parent=0, const char *name=0);
	~RenderDebugPanel();
};

#endif
