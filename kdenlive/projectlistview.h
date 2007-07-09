/***************************************************************************
                          projectlistview.h  -  description
                             -------------------
    begin                : Sun Jun 30 2002
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

#ifndef PROJECTLISTVIEW_H
#define PROJECTLISTVIEW_H


#include <qtooltip.h>
#include <qheader.h>

#include <klistview.h>

#include "avlistviewitem.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"


/**
	* ProjectListView contains a derived class from KListView which sets up the correct column headers
	* for the view, etc.
  *@author Jason Wood
  */

class ProjectListView:public KListView {
  Q_OBJECT public:
    ProjectListView(QWidget * parent = 0, const char *name = 0);
    ~ProjectListView();
	/** returns a drag object which is used for drag operations. */
    QDragObject *dragObject();
    QString m_popuptext;
	/** Sets the document to the one specified */
    void setDocument(KdenliveDoc * doc);
    QString popupText();
    void setPopupText(QString txt);
    DocClipRefList selectedItemsList() const;
    QString parentName();
    void selectItemsFromIds(QStringList idList);
    QStringList selectedItemsIds() const;

  signals:			// Signals
	/** This signal is called whenever clips are drag'n'dropped onto the project list view. */
    void dragDropOccured(QDropEvent *, QListViewItem *);
	/** This signal is called whenever a drag'n'drop is started */
    void dragStarted(QListViewItem *);
    void addClipRequest();

  protected:
		/** returns true if the drop event is compatable with the project list */
     bool acceptDrag(QDropEvent * event) const;
     virtual void contentsMouseDoubleClickEvent( QMouseEvent * e );

  private slots:		// Private slots
	/** This slot function should be called whenever a drag has been dropped onto the class. */
    void dragDropped(QDropEvent * e, QListViewItem * parent,
	QListViewItem * after);
  private:			// Private attributes
	/** The document that keeps this list up-to-date. */
     KdenliveDoc * m_doc;
};


class ListViewToolTip : public QToolTip
{
public:
    ListViewToolTip( QListView* parent );
protected:
    void maybeTip( const QPoint& p );
private:
    QListView* listView;
};
inline ListViewToolTip::ListViewToolTip( QListView* parent )
    : QToolTip( parent->viewport() ), listView( parent ) {}
inline void ListViewToolTip::maybeTip( const QPoint& p ) {
    if ( !listView )
        return;
    const AVListViewItem* item = static_cast<AVListViewItem *>(listView->itemAt( p ));
    if ( !item )
        return;
    const QRect itemRect = listView->itemRect( item );
    if ( !itemRect.isValid() )
        return;
    const int col = listView->header()->sectionAt( p.x() );
    if ( col == -1 )
        return;
    const QRect headerRect = listView->header()->sectionRect( col );
    if ( !headerRect.isValid() )
        return;
  const QRect cellRect( headerRect.left(), itemRect.top(),
                         headerRect.width(), itemRect.height() );
  QString tipStr;
  if( col == 2 )
       tipStr = item->text(2);
  else tipStr = item->getInfo();

  tip( cellRect, tipStr );
};


#endif
