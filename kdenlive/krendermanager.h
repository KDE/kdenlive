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

class KConfig;

/**This class manages the various renderers that are directly accessed by the application.

It manages the creation and deletion of such renderers.
  *@author Jason Wood
  */

class KRenderManager : public QObject {
  Q_OBJECT
public:
	KRenderManager();
	~KRenderManager();
  /** Creates a new renderer, guaranteeing it it's own port number, etc.
  The name specified is used to identify this renderer, and may be shown to
  the user in, for example, the debug dialog.*/
  KRender * createRenderer(const QString &name);
  /** Reads the configuration details for the renderer manager */
  void readConfig(KConfig *config);
  /** Saves the configuration details for the renderer manager */  
  void saveConfig(KConfig *config);  
  /** Write property of KURL m_renderAppPath. */
  void setRenderAppPath( const KURL& _newVal);
  /** Read property of KURL m_renderAppPath. */
  const KURL& renderAppPath();
  /** Write property of unsigned int m_renderAppPort. */
  void setRenderAppPort( const unsigned int& _newVal);
  /** Read property of unsigned int m_renderAppPort. */
  const unsigned int& renderAppPort();  
private:
  QPtrList<KRender> m_renderList;
  KURL m_renderAppPath;
  unsigned int m_firstPort;
  unsigned int m_currentPort;
signals:
  /** Emitted when the renderer has recieved text from stdout */
  void recievedStdout(const QString &, const QString &);
  /** Emitted when the renderer has recieved text from stderr */
  void recievedStderr(const QString &, const QString &);
  /** Emits useful rendering debug info. */
  void renderDebug(const QString &, const QString &);
  /** Emits renderer warnings info. */
  void renderWarning(const QString &, const QString &);
  /** Emits renderer errors. */
  void renderError(const QString &, const QString &);
  /** emitted when an error occurs within one of the managed renderers. */
  void error(const QString &, const QString &);
};

#endif
