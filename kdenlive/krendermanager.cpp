/***************************************************************************
                          krendermanager.cpp  -  description
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

#include "krendermanager.h"

#include <kconfig.h>

KRenderManager::KRenderManager() :
                        QObject(),
                        m_renderAppPath(KURL("/usr/local/bin/piave")),
                        m_firstPort(6100),
                        m_currentPort(6100)
{
  m_renderList.setAutoDelete(true);
}

KRenderManager::~KRenderManager()
{
}

/** Creates a new renderer, guaranteeing it it's own port number, etc. */
KRender * KRenderManager::createRenderer(const QString &name)
{
  KRender *render = new KRender(name, m_renderAppPath, m_currentPort);

  connect(render, SIGNAL(recievedStderr(const QString &, const QString &)), this, SIGNAL(recievedStderr(const QString &, const QString &)));
  connect(render, SIGNAL(recievedStdout(const QString &, const QString &)), this, SIGNAL(recievedStdout(const QString &, const QString &)));
  connect(render, SIGNAL(recievedInfo(const QString &, const QString &)), this, SIGNAL(recievedInfo(const QString &, const QString &)));
  connect(render, SIGNAL(error(const QString &, const QString &)), this, SIGNAL(error(const QString &, const QString &)));

  ++m_currentPort;
  
  m_renderList.append(render);
  return render;
}

/** Reads the configuration details for the renderer manager */
void KRenderManager::readConfig(KConfig *config)
{
  config->setGroup("Renderer Options");
  setRenderAppPath(config->readEntry("App Path", "/usr/local/bin/piave"));
  setRenderAppPort(config->readUnsignedNumEntry("App Port", 6100));
}

void KRenderManager::saveConfig(KConfig *config)
{
  config->setGroup("Renderer Options");
  config->writeEntry("App Path" , renderAppPath().path());
  config->writeEntry("App Port" , renderAppPort());
}

/** Read property of KURL m_renderAppPath. */
const KURL& KRenderManager::renderAppPath()
{
	return m_renderAppPath;
}

/** Write property of KURL m_renderAppPath. */
void KRenderManager::setRenderAppPath( const KURL& _newVal)
{
	m_renderAppPath = _newVal;
}

/** Read property of unsigned int m_renderAppPort. */
const unsigned int& KRenderManager::renderAppPort()
{
	return m_firstPort;
}

/** Write property of unsigned int m_renderAppPort. */
void KRenderManager::setRenderAppPort( const unsigned int& _newVal)
{
	m_firstPort = _newVal;
}
