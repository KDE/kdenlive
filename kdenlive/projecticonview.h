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

#ifndef PROJECTICONVIEW_H
#define PROJECTICONVIEW_H


#include <qtooltip.h>
#include <qheader.h>

#include <kiconview.h>

#include "aviconviewitem.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"


/**
	* ProjectIconView contains a derived class from KListView which sets up the correct column headers
	* for the view, etc.
  *@author Jason Wood
  */

class ProjectIconView:public KIconView {
  Q_OBJECT public:
    ProjectIconView(QWidget * parent = 0, const char *name = 0);
    ~ProjectIconView();
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
    DocClipRef *selectedItem() const;

  signals:			// Signals
	/** This signal is called whenever clips are drag'n'dropped onto the project list view. */
    void dragDropOccured(QDropEvent * e, QIconViewItem * parent);
	/** This signal is called whenever a drag'n'drop is started */
    void dragStarted(QIconViewItem * i);

  protected:
		/** returns true if the drop event is compatable with the project list */
     bool acceptDrag(QDropEvent * event) const;
    private slots:		// Private slots
	/** This slot function should be called whenever a drag has been dropped onto the class. */
    void dragDropped(QDropEvent * e, const QValueList<QIconDragItem>&);
  private:			// Private attributes
	/** The document that keeps this list up-to-date. */
     KdenliveDoc * m_doc;
};


class IconViewToolTip : public QToolTip
{
public:
    IconViewToolTip( QIconView* parent );
protected:
    void maybeTip( const QPoint& p );
private:
    QIconView* iconView;
};
inline IconViewToolTip::IconViewToolTip( QIconView* parent )
    : QToolTip( parent->viewport() ), iconView( parent ) {}

inline void IconViewToolTip::maybeTip( const QPoint& p ) {
    if ( !iconView )
        return;
    QIconViewItem *iconItem = iconView->findItem( p );
    const AVIconViewItem* item = static_cast<AVIconViewItem *>(iconItem);
    if ( !item )
        return;
    const QRect itemRect = iconItem->rect();
    if ( !itemRect.isValid() )
        return;
  QString tipStr;
  tipStr = item->getInfo();

  tip( itemRect, tipStr );
};


#endif
