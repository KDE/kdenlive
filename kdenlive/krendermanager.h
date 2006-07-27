/***************************************************************************
                         krendermanager.h  -  description
                            -------------------
   begin                : Wed Jan 15 2003
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

#ifndef KRENDERMANAGER_H
#define KRENDERMANAGER_H

#include "qobject.h"
#include "qptrlist.h"
#include "kurl.h"

#include "krender.h"
#include "kdenlive.h"

class KConfig;

/**This class manages the various renderers that are directly accessed by the application.

It manages the creation and deletion of such renderers.
  *@author Jason Wood
  */

class KRenderManager:public QObject {
  Q_OBJECT public:
          KRenderManager(Gui::KdenliveApp* parent=0);
    ~KRenderManager();
	/** Creates a new renderer, guaranteeing it it's own port number, etc.
	The name specified is used to identify this renderer, and may be shown to
	the user in, for example, the debug dialog.*/
    KRender *createRenderer(const QString &name);
    /** Finds a renderer by name. Returns null if no such renderer exists. */
    KRender * findRenderer(const QString & name);
    void resetRenderers();

  private:
     QPtrList < KRender > m_renderList;
     Gui::KdenliveApp *m_app;
};

#endif
