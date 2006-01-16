/***************************************************************************
                          kmmscreen.h  -  description
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

#ifndef KMMSCREEN_H
#define KMMSCREEN_H

#include <qdom.h>
#include <qvbox.h>

#include "gentime.h"

class KRender;
class QXEmbed;

namespace Gui {
    class KdenliveApp;

/**KMMScreen acts as a wrapper for the window provided by the cutter.
	It requests a video window from the cutter, and embeds it within
	itself to allow seamless operation.
  *@author Jason Wood
  */

    class KMMScreen:public QVBox {
      Q_OBJECT public:
	KMMScreen(KdenliveApp * app, QWidget * parent =
	    0, const char *name = 0);
	~KMMScreen();
	/** Returns the current play speed */
	double playSpeed();

	/** Returns the current seek position */
	const GenTime & seekPosition() const;
	void setClipLength(int frames);
      protected:
	void mousePressEvent(QMouseEvent * e);
	void wheelEvent(QWheelEvent * e);
	void mouseMoveEvent(QMouseEvent * e);
      private:			// Private attributes
	 KRender * m_render;
	KdenliveApp *m_app;
	//QXEmbed *m_embed;
	GenTime m_clipLength;
	private slots:		// Private slots
  /** The renderer is ready, so we open
a video window, etc. here. */
	void rendererReady();
	public slots:		// Public slots
	/** Embeds the specified window. */
	    //void embedWindow(WId wid);
	/** Seeks to the specified time */
	void seek(const GenTime & time);
	/** Stop playing */
	void playStopped(const GenTime & startTime);
	/** Start renderer's consumer */
	void startRenderer();
	void stopRenderer();

	/** Set the play speed of the screen */
	void play(double speed);
	/** Set the play speed of the screen */
	void play(double speed, const GenTime & startTime);
	/** Set the play speed of the screen */
	void play(double speed, const GenTime & startTime,
	    const GenTime & endTime);
	/** Set the displayed scenelist to the one specified. */
	void setSceneList(const QDomDocument & scenelist,
	    bool resetPosition = true);
	/** Sets the screen up as a capture device */
	void setCapture();
	/** Called when the renderer stops playing */
	void slotRendererStopped();
	 signals:		// Signals
	/** Emitted when a renderer connects. */
	void rendererConnected();
	/** Emitted when a renderer disconnects. */
	void rendererDisconnected();
	/** Emitted when the screen has seeked to a new position, e.g. when the renderer is playing. */
	void seekPositionChanged(const GenTime &);
	/** Emitted when the play speed changes */
	void playSpeedChanged(double);
	/** Emitted when the mouse is clicked over the screen. */
	void mouseClicked();
	/** Emitted when the mouse is clicked over the screen. */
	void mouseRightClicked();
	/** Emitted when a drag has started to occur over the screen. */
	void mouseDragged();
    };

}
#endif
