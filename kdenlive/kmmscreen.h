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
#include <qpixmap.h>

#include <kurl.h>

#include "gentime.h"

class KRender;

namespace Gui {
    class KMMMonitor;

/**KMMScreen acts as a wrapper for the window provided by the cutter.
	It requests a video window from the cutter, and embeds it within
	itself to allow seamless operation.
  *@author Jason Wood
  */

    class KMMScreen:public QVBox {
      Q_OBJECT
public:
	KMMScreen(KMMMonitor *monitor, QWidget * parent =
	    0, const char *name = 0);
	~KMMScreen();
	/** Returns the current play speed */
	double playSpeed();

	/** Returns the current seek position */
	const GenTime & seekPosition() const;

protected:
	void mousePressEvent(QMouseEvent * e);
	void mouseReleaseEvent(QMouseEvent * e);
	void wheelEvent(QWheelEvent * e);
	void mouseMoveEvent(QMouseEvent * e);
	void paintEvent ( QPaintEvent * );

private:			// Private attributes
	KMMMonitor *m_monitor;
	KRender * m_render;
	GenTime m_clipLength;
	QString m_name;
	

public slots:
	/** Embeds the specified window. */
	    //void embedWindow(WId wid);
	/** Seeks to the specified time */
	void seek(const GenTime & time);
	/** Stop playing */
	void playStopped(const GenTime & startTime);
	/** Start renderer's consumer */
	void startRenderer();
	void stopRenderer();
	void resetRenderer();

	/** Set MLT SDL consumer volume */
        void setVolume(double volume) const;

	/** Refresh display (check if on screen display should be on or off */
	void refreshDisplay() const;

	QPixmap extractFrame(int frame, int width, int height);
	/** Set the play speed of the screen */
	void play(double speed);
	/** Set the play speed of the screen */
	void play(double speed, const GenTime & startTime);
	/** Set the play speed of the screen */
	void play(double speed, const GenTime & startTime,
	    const GenTime & endTime);
	/** Set the displayed scenelist to the one specified. */
	void setSceneList(const QDomDocument & scenelist, int position = 0);
	/** Called when the renderer stops playing */
	void slotRendererStopped();
	int getLength();

  	void exportToFirewire(QString url, int port, GenTime startTime, GenTime endTime);
  	void exportCurrentFrame(KURL url, bool notify);

	/** return current scenelist */
	QDomDocument sceneList();

	void updateClipLength();

signals:
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
  	void exportOver();
    };
}
#endif
