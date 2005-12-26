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
#include <kdebug.h>

KRenderManager::KRenderManager() :
                        QObject(),
                        m_renderAppPath(KURL("/usr/bin/piave")),
                        m_firstPort(6100),
                        m_currentPort(6100)
{
  m_renderList.setAutoDelete(true);
	m_refCount=0;
}

KRenderManager::~KRenderManager()
{
}

/** Creates a new renderer, guaranteeing it it's own port number, etc. */
KRender * KRenderManager::createRenderer(const char * name)
{
	KRender *render = new KRender(name, m_renderAppPath, m_currentPort,0, name);

	connect(render, SIGNAL(recievedStderr(const QString &, const QString &)),
			this, SIGNAL(recievedStderr(const QString &, const QString &)));
	connect(render, SIGNAL(recievedStdout(const QString &, const QString &)),
			this, SIGNAL(recievedStdout(const QString &, const QString &)));
	connect(render, SIGNAL(error(const QString &, const QString &)),
			this, SIGNAL(error(const QString &, const QString &)));
	connect(render, SIGNAL(renderDebug(const QString &, const QString &)),
			this, SIGNAL(renderDebug(const QString &, const QString &)));
	connect(render, SIGNAL(renderWarning(const QString &, const QString &)),
			this, SIGNAL(renderWarning(const QString &, const QString &)));
	connect(render, SIGNAL(renderError(const QString &, const QString &)),
			this, SIGNAL(renderError(const QString &, const QString &)));

	++m_currentPort;

	m_renderList.append(render);
	return render;
}

/** Reads the configuration details for the renderer manager */
void KRenderManager::readConfig(KConfig *config)
{
  config->setGroup("Renderer Options");
  setRenderAppPath(config->readEntry("App Path", "/usr/bin/piave"));
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

void KRenderManager::sendDebugCommand(const QString &renderName, const QString &debugCommand)
{
	KRender *renderer = findRenderer(renderName);
	if(renderer) {
		renderer->sendDebugVemlCommand(debugCommand);
	} else {
		kdWarning() << "Cannot send debug command, renderer " << renderName << " does not exist. " << endl;
	}
}

KRender *KRenderManager::findRenderer(const QString &name)
{
	KRender *result = 0;
	QPtrListIterator<KRender> itt(m_renderList);
	while(itt.current()) {
		if(itt.current()->rendererName() == name) {
			result = itt.current();
			break;
		}
		++itt;
	}
	return result;
}
