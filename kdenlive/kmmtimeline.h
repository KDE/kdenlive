/***************************************************************************
                          kmmtimeline.h  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#ifndef KMMTIMELINE_H
#define KMMTIMELINE_H

#include <qwidget.h>
#include <qvbox.h>
#include <qgrid.h>
#include <qlabel.h>
#include <qscrollbar.h>
#include <qscrollview.h>
#include <qlist.h>

#include <qpushbutton.h>

#include <kmmruler.h>
#include <kdenlivedoc.h>
#include <kmmtrackbase.h>

/**This is the timeline. It gets populated by tracks, which in turn are populated
by video and audio clips, or transitional clips, or any other clip imaginable.
  *@author Jason Wood
  */

class KMMTimeLine : public QVBox  {
   Q_OBJECT
public: 
	KMMTimeLine(KdenliveDoc *document, QWidget *parent=0, const char *name=0);
	~KMMTimeLine();
private:
		/** GUI elements */
		QHBox m_rulerBox;				 	// Horizontal box holding the ruler
		QScrollView m_trackScroll; 	// Scrollview holding the tracks
		QHBox m_scrollBox;			 	// Horizontal box holding the horizontal scrollbar.
		QLabel m_trackLabel;
		KMMRuler m_ruler;
		QPushButton m_addTrackButton;
		QPushButton m_deleteTrackButton;
		QScrollBar m_scrollBar;		// this scroll bar's movement is measured in pixels, not frames.
		/** track varables */
		QList<QWidget> m_trackPanels;
		QList<QWidget> m_trackViews;
		
	  /** A pointer to the document (project) that this timeline is based upon */
	  KdenliveDoc * m_document;		
public: // Public methods
  /** This method adds a new track to the trackGrid. */
  void appendTrack(QWidget *trackPanel, KMMTrackBase *trackView);
  void resizeEvent(QResizeEvent *event);
  /** Inserts a track at the position specified by index */
  void insertTrack(int index, QWidget *trackPanel, KMMTrackBase *trackView);
  /** No descriptions */
  void polish();

	void dragEnterEvent ( QDragEnterEvent * );
	void dragMoveEvent ( QDragMoveEvent * );
	void dragLeaveEvent ( QDragLeaveEvent * );
	void dropEvent ( QDropEvent * );

private: // private methods
	void resizeTracks();
public slots: // Public slots
  /** Called when a track within the project has been added or removed.
    *
		* The timeline needs to be updated to show these changes. */
  void syncWithDocument();


};

#endif
