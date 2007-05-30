/***************************************************************************
                          listviewtagsearch.h  -  description
                             -------------------
    begin                : Sat April 4 2007
    copyright            : (C) 2007 by Ryan Hodge
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef LISTVIEWTAGSEARCH_H
#define LISTVIEWTAGSEARCH_H

#include <klistviewsearchline.h>

/**
  *@author Ryan Hodge
  */

class ListViewTagSearch : public KListViewSearchLine
{
    Q_OBJECT

public:

    /**
     * Constructs a ListViewTagSearch with \a listView being the KListView to
     * be filtered.
     *
     * If \a listView is null then the widget will be disabled until a listview
     * is set with setListView().
     */
    ListViewTagSearch(QWidget *parent = 0, KListView *listView = 0, const char *name = 0);

    /**
     * Constructs a KListViewSearchLine without any KListView to filter. The
     * KListView object has to be set later with setListView().
     */
    ListViewTagSearch(QWidget *parent, const char *name);

    /**
     * Destroys the ListViewTagSearch.
     */
    virtual ~ListViewTagSearch();

public slots:
    /**
     * Updates search to only make visible the items that match \a s.  If
     * \a s is null then the line edit's text will be used.
     * @todo The itemMatches override should be replaced by this, so that
     * we can pass a SearchContainer to an itemMatches instead of a string,
     * making it faster and more efficient
     */
    //virtual void updateSearch(const QString &s = QString::null);

protected:

    /**
     * Returns true if \a item matches the search \a s.  This will be evaluated
     * based on the value of caseSensitive().  This can be overridden in
     * subclasses to implement more complicated matching schemes.
     */
    virtual bool itemMatches(const QListViewItem *item, const QString &s) const;

private:
    class ListViewTagSearchImpl;
    ListViewTagSearchImpl *impl_;
};

/**
 * Creates a widget featuring a ListViewTagSearch, a label with the text
 * "Search" and a button to clear the search.
 *
  *@author Ryan Hodge
 */
class ListViewTagSearchWidget : public KListViewSearchLineWidget
{
    Q_OBJECT

public:
    /**
     * Creates a ListViewTagSearchWidget for \a listView with \a parent as the
     * parent with and \a name.
     */
    ListViewTagSearchWidget(KListView *listView = 0, QWidget *parent = 0,
                              const char *name = 0);

    /**
     * Creates the search line. Reimplemented from KListVieSearchLineWidget
     */
    virtual KListViewSearchLine *createSearchLine(KListView *listView);

    /**
     * Returns a pointer to the search line. Reimplemented here because in
     * KListViewSearchLineWidget there is no way to create the search line
     * before createWidgets is called. We want to be able to access the search
     * line right after the widget has been created
     */
    ListViewTagSearch *searchLine() const;

private:
    ListViewTagSearch *searchLine_;
};

#endif // LISTVIEWTAGSEARCH_H
