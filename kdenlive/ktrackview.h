/***************************************************************************
                         kmmtimelinetrackview.h  -  description
                            -------------------
   begin                : Wed Aug 7 2002
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

#ifndef KTRACKVIEW_H
#define KTRACKVIEW_H

#include <qwidget.h>
#include <qpixmap.h>

#include "gentime.h"
#include "trackpanelfunctionfactory.h"
#include "rangelist.h"
#include "dynamicToolTip.h"

class DocClipBase;

namespace Gui {

    class KTimeLine;
    class KTrackPanel;
    class KdenliveApp;

/**Timeline track view area.
  *@author Jason Wood
  */

    class KTrackView:public QWidget {
      Q_OBJECT public:
	KTrackView(KTimeLine & timeLine, QWidget * parent =
	    0, const char *name = 0);
	~KTrackView();
	void resizeEvent(QResizeEvent * event);
	void paintEvent(QPaintEvent * event);
	/** This event occurs when the mouse has been moved. */
	void mouseMoveEvent(QMouseEvent * event);
	/** This event occurs when a mouse button is released. */
	void mouseReleaseEvent(QMouseEvent * event);
	/** This event occurs when a mouse button is pressed. */
	void mousePressEvent(QMouseEvent * event);
	/** This event occurs when a double click occurs. */
	void mouseDoubleClickEvent(QMouseEvent * event);

	/** Returns the track panel that lies at the specified y coordinate on the
	TimelineTrackView. */
	KTrackPanel *panelAt(int y);

	virtual void dragEnterEvent(QDragEnterEvent *);
	virtual void dragMoveEvent(QDragMoveEvent *);
	virtual void dragLeaveEvent(QDragLeaveEvent *);
	virtual void dropEvent(QDropEvent *);

	void registerFunction(const QString & name, TrackPanelFunction * function);
	void clearFunctions();
	void setDragFunction(const QString & name);
	void tip(const QPoint &pos, QRect &rect, QString &tipText);
        
      protected:
          void wheelEvent( QWheelEvent * e );

      private:			// Private methods
        RangeList < int >m_bufferDrawList;
        void drawBackBuffer();
        void drawBackBuffer(int start, int end);
	TrackPanelFunction *getApplicableFunction(KTrackPanel * panel,
	const QString & editMode, QMouseEvent * event);
	int m_startTrack;
	int m_endTrack;
	QColor m_selectedColor;
	QPixmap m_backBuffer;
	 KTimeLine & m_timeline;
	/** True if the back buffer needs to be redrawn. */
	bool m_bufferInvalid;
	int m_trackBaseNum;
	KTrackPanel *m_panelUnderMouse;

	/** The current function that is in operation*/
	TrackPanelFunction *m_function;

	/** This function will be called if a drag operation starts/stops. */
	TrackPanelFunction *m_dragFunction;

	/** Factory containing the various decorating functions that can be applied to different tracks */
	TrackPanelFunctionFactory m_factory;
	GenTime currentSeekPos;

	KdenliveApp *m_app;

	DynamicToolTip *trackview_tips;

    public slots:		// Public slots
	/** Invalidate the back buffer, alerting the trackview that it should redraw itself. */
	void invalidateBackBuffer(int startTrack = 0, int endTrack = -1);
        void invalidatePartialBackBuffer(int pos1, int pos2, int startTrack = 0, int endTrack = -1);
        
    signals: 
        void rightButtonPressed();
        void changeZoom(bool);
    };

}				// namespace Gui
#endif
