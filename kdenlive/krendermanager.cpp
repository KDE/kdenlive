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

#include <kconfig.h>
#include <kdebug.h>

#include "krendermanager.h"

KRenderManager::KRenderManager(Gui::KdenliveApp *parent):
QObject(), m_app(parent)
{
    m_renderList.setAutoDelete(true);
}

KRenderManager::~KRenderManager()
{}

/** Creates a new renderer, guaranteeing it it's own port number, etc. */
KRender *KRenderManager::createRenderer(const QString &name)
{
    KRender *render = new KRender(name, m_app, name.ascii());
    m_renderList.append(render);
    return render;
}


KRender *KRenderManager::findRenderer(const QString & name)
{
    KRender *result = 0;
    QPtrListIterator < KRender > itt(m_renderList);
    bool found = false;
    while (itt.current()) {
	if (itt.current()->rendererName() == name) {
	    result = itt.current();
            found = true;
	    break;
	}
	++itt;
    }
    if (!found) return createRenderer(name);
    return result;
}

void KRenderManager::resetRenderers()
{

    QStringList renderNames;
    QPtrListIterator < KRender > itt(m_renderList);
    while (itt.current()) {
	renderNames.append(itt.current()->rendererName());
	++itt;
    }
    m_renderList.clear();

    for ( QStringList::Iterator it = renderNames.begin(); it != renderNames.end(); ++it ) {
        createRenderer(*it);
    }
}
