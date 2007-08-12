/***************************************************************************
                          kmmmonitor.h  -  description
                             -------------------
    begin                : Sun Mar 24 2002
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

#ifndef KMMMONITOR_H
#define KMMMONITOR_H

#include "kmonitor.h"
#include "kmmscreen.h"
#include "kmmeditpanel.h"
#include "gentime.h"

class DocClipBase;
class DocClipRef;
class KdenliveDoc;

/**KMMMonitor provides a multimedia bar and the
ability to play Arts PlayObjects. It is also capable
of editing the object down, and is Drag 'n drop
aware with The Timeline, The Project List, and
external files.
  *@author Jason Wood
  */

namespace Gui {

    class KdenliveApp;

    class KMMMonitor:public KMonitor {
      Q_OBJECT public:
	KMMMonitor(KdenliveApp * app, KdenliveDoc * document,
	    QWidget * parent = 0, const char *name = 0);
	 virtual ~ KMMMonitor();

	/** ***Nasty Hack***
	Swaps the screens of two monitors, reparenting and reconnecting all
	relevant signals/slots. This is required so that if a render instance
	uses xv (of which there is only one port), we can use it in multiple
	monitors. */
//      void swapScreens(KMMMonitor *monitor);

	/** Returns the current seek position */
	const GenTime & seekPosition() const;

	/** See m_noSeek property for details. */
	void setNoSeek(bool noSeek);
	int externalMonitor();
	void playingStopped();
	/** Starts a drag operation, using the currently selected clip and the specified in
	 *  and out points for it. */
	void startDrag();
	/** Causes the monitor to act as if a right mouse click has happened on it. */
	void rightClickMonitor();
	/** Causes the monitor to act as if a mouse click has happened on it. */
	void clickMonitor();
	/** This slot is called when the screen changes position. */
	void screenPositionChanged(const GenTime & time);
	KRender *findRenderer(const char *name);

	virtual DocClipRef *clip() const;

	virtual KMMEditPanel *editPanel() const;
	virtual KMMScreen *screen() const;
        /** Save current frame as picture file */
        virtual void exportCurrentFrame(KURL url, bool notify) const;


      protected:
	void mousePressEvent(QMouseEvent * e);
      private:
	/** Bring up the monitor context menu, if one exist */
	void popupContextMenu();
	KdenliveApp *m_app;
	KdenliveDoc *m_document;
	/** a widget that acts as a holding place in the monitor layout - so that we can
	reparent the screen out of the monitor, reparent another screen in, and it ends
	up in the correct place. Without this, the screen ends up at the bottom of the
	widget stack and the editpanel, etc. ends up at the top of the monitor instead of
	at the bottom. */
	QVBox *m_screenHolder;
	KMMScreen *m_screen;
	KMMEditPanel *m_editPanel;
	/** If true, then changing the clip in the monitor will not change the seek position UNLESS
	 * it falls outside of the current inpoint/outpoint range. If false, then changing the clip
	 * will always seek to the inpoint position.*/
	bool m_noSeek;
	/** The clip being displayed by the monitor. */
	DocClipRef *m_clip;
	/** A reference to the clip being displayed in the monitor. This is useful if we want to update the monitor
	when this particular clip changes on the timeline. Note - we do not own this clip, it should not be deleted.*/
	DocClipRef *m_referredClip;
	/** Connects all signals/slots to the screen. */
	void connectScreen();
	/** Disconnects all signals/slots from the screen */
	void disconnectScreen();

	/** Commont functionality for the setClip slots. */
        void doCommonSetClip(bool resetCropPosition = true);

	public slots:		// Public slots
	/** Set the monitors scenelist to the one specified. */
	void setSceneList(const QDomDocument & scenelist, int position = 0);
	void slotSetVolume(double volume) const;
	/** Refresh display (check if on screen display should be on or off */
	void refreshDisplay() const;
	/** Sets this monitor to be the active monitor. It's colour changes to show it is active. */
	void slotSetActive() const;
	/** Sets this monitor to be an inactive monitor. It's colour changes to show it is inactive. */
	void slotSetInactive() const;
	/** Sets the displayed clip in the timeline. */
	void slotSetClip(DocClipBase * clip);
	/** Sets the displayed clip in the timeline. */
	void slotSetClip(DocClipRef * clip);

	void slotSetClip(QDomDocument playlist, GenTime duration);
	/** Clears the displayed clip on the timeline */
	void slotClearClip();

	void activateMonitor();
	void refreshClip();

	/** The specified clip has changed, if the monitor uses this clip it will update it's
	representation, otherwise it will ignore the method. */
	void slotUpdateClip(DocClipRef * clip);

	/** Called when a clip's crop start has changed. */
	void slotClipCropStartChanged(DocClipRef * clip);

	/** Called when a clip's crop end has changed. */
	void slotClipCropEndChanged(DocClipRef * clip);

	/** Toggles a snap marker on or off at the given position in the clip.. */
	void slotToggleSnapMarker();

	/** go to the previous snap marker from the current seek position */
	void slotPreviousSnapMarker();

	/** go to the next snap marker from the current seek position */
	void slotNextSnapMarker();

	void slotPlaySpeedChanged(double speed);

	 signals:		// Signals
	/** Emitted when the monitor's current position has changed. */
	void seekPositionChanged(const GenTime &);
	/** Emitted when the monitor's current inpoint has changed. */
	void inpointPositionChanged(const GenTime &);
	/** Emitted when the monitor's current outpoint has changed. */
	void outpointPositionChanged(const GenTime &);
	void activatedSlider(int);

	private slots:
	    // Update the edit panel, make sure that it's buttons are in sync.
	void updateEditPanel(const GenTime & time);
    };

}				// namespace Gui
#endif
