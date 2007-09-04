/***************************************************************************
                          projectlist.h  -  description
                             -------------------
    begin                : Sat Feb 16 2002
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

#ifndef PROJECTLIST_H
#define PROJECTLIST_H

#include <qvaluelist.h>
#include <qpopupmenu.h>
#include <qevent.h>
#include <qheader.h>
#include <qtooltip.h>

#include <kurl.h>
#include <kprinter.h>
#include <kiconviewsearchline.h>

#include "listviewtagsearch.h"
#include "projectlistview.h"
#include "projecticonview.h"
#include "projectview_ui.h"

class DocClipRef;
class KdenliveDoc;
class DocumentBaseNode;

namespace Gui {

    class KdenliveApp;

/**
  * ProjectList is the dialog which contains the project list.
  *@author Jason Wood
  */
//adds tooltips to project list column headers
    class columnToolTip:public QToolTip {
      public:
	columnToolTip(QHeader * header, QToolTipGroup * group = 0);
	virtual ~ columnToolTip();
      private:
	void maybeTip(const QPoint & p);
    };

    class ProjectList:public ProjectList_UI {
      Q_OBJECT public:
	ProjectList(KdenliveApp * app, KdenliveDoc * document, bool iconView, QWidget * parent = 0, const char *name = 0);
	~ProjectList();
	/** Returns the currently selected clip in the project list. */
	DocClipRefList currentSelection();
	DocClipRef* currentClip();
        QString parentName();
	QString currentItemName();
	bool currentItemIsFolder();
	bool renameFolder(QString newName);
	void setListView();
	void setIconView();
	bool isListView();
	void focusView();
	bool hasChildren();
	QStringList currentItemChildrenIds();
	bool isEmpty();
	/** Fix broken url in playlist clips */
	void fixPlaylists();
	void doPrinting(KPrinter *printer, QPainter *p, uint images, bool fullPath, bool grayscale, bool filtered);

      private:			// Private methods
	/** Holds the document that this projectlist makes use of. */
	 KdenliveDoc * m_document;
	/** Holds a pointer to the application. FIXME: Is this necessary? */
	KdenliveApp *m_app;
	ProjectListView *m_listView;
	ProjectIconView *m_iconView;
	QPopupMenu *contextcreateMenu();
        columnToolTip * colToolTip;
	ListViewTagSearchWidget *lv_search;
	KIconViewSearchLine *iv_search;
	bool m_isIconView;

      public slots:		// Public slots
	/** No descriptions */
	void rightButtonPressed(QListViewItem * listViewItem,
	    const QPoint & pos, int column);
        void rightButtonPressed(QIconViewItem * iconViewItem,
	const QPoint & pos);
	/** Get a fresh copy of files and clips from KdenliveDoc and display them. */
	void slot_UpdateList();
	/** The clip specified has changed - update the display. */
	void slot_clipChanged(DocClipRef *);
        void slot_clipChanged();
	void refresh();
	/** The node specified has been deleted - update the display. */
	void slot_nodeDeleted(DocumentBaseNode * node);
	/** updates the list when an item changed */
	void updateListItem();
        void selectClip(DocClipBase *clip);
        void selectItem(int id);
	void createItemChildren(QListViewItem *item);
        
      signals:		// Signals
	/** this signal is called when a number of clips have been dropped onto the project list view. */
	void dragDropOccured(QDropEvent *, QListViewItem *);
	/** This signal is emitted when an AVFile is selected in the project list. */
	void clipSelected(DocClipRef *);
	void playlistItemSelected(QDomDocument, GenTime);
        void editItem();
        
	private slots:		// Private slots
	/** Called when the project list changes. */
	    //void projectListSelectionChanged(QListViewItem *item);
         /** an item was double clicked */
        void editRequested( QListViewItem *i, const QPoint &, int col);
	void editRequested( QIconViewItem *);
	void setupListView();
	void setupIconView();
	void addClipRequest();
	void projectModified();
    };

}				// namespace Gui
#endif
