/***************************************************************************
                          kmmscreen.cpp  -  description
                             -------------------
    begin                : Mon Mar 25 2002
    copyright            : (C) 2002 by Jason Wood
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

#include "kmmscreen.h"
#include <string.h>
#include <kdebug.h>
#include <klocale.h>
#include <qxembed.h>

#include "krender.h"
#include "krendermanager.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"

KMMScreen::KMMScreen(KdenliveApp *app, QWidget *parent, const char *name ) :
                                    QVBox(parent,name),
                                    m_render(app->renderManager()->createRenderer(name)),
                                    m_app(app),
                                    m_embed(new QXEmbed(this, name))
{
	m_embed->setBackgroundMode(Qt::PaletteDark);
  
	connect(m_render, SIGNAL(initialised()), this, SLOT(rendererReady()));
	connect(m_render, SIGNAL(replyCreateVideoXWindow(WId)), this, SLOT(embedWindow(WId)));
  connect(m_render, SIGNAL(connected()), this, SIGNAL(rendererConnected()));
  connect(m_render, SIGNAL(disconnected()), this, SIGNAL(rendererDisconnected()));
  connect(m_render, SIGNAL(positionChanged(const GenTime &)), this, SIGNAL(seekPositionChanged(const GenTime &)));
}

KMMScreen::~KMMScreen()
{
	if(m_render) delete m_render;
}

/** The renderer is ready, so we open a video window, etc. here. */
void KMMScreen::rendererReady()
{
	m_render->createVideoXWindow(false);
}

/** Embeds the specified window. */
void KMMScreen::embedWindow(WId wid)
{
	if(wid != 0) {
		m_embed->embed(wid);
	}
}

/** Seeks to the specified time */
void KMMScreen::seek(const GenTime &time)
{ 
	m_render->seek(time);
}

/** Set the play speed of the screen */
void KMMScreen::play(double speed)
{
	m_render->play(speed);
}

/** Set the displayed scenelist to the one specified. */
void KMMScreen::setSceneList(const QDomDocument &scenelist)
{
  m_render->setSceneList(scenelist);
}
