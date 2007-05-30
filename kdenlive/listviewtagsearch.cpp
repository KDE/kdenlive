/***************************************************************************
                          listviewtagsearch.cpp  -  description
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

#include "listviewtagsearch.h"

#include <klistview.h>

#include <qvaluelist.h>
#include <qstring.h>
#include <qregexp.h>

namespace
{
/** The structure of our simple search type. Our search structure
  * consists of a list of AND'd expressions, that can be optionally
  * OR'd with more lists of AND'd expressions, and each string expression
  * can be optionally negated
  */
typedef QPair<QString, bool> NotString;
typedef QValueList<NotString> AndContainer;
typedef QValueList<AndContainer> SearchContainer;

/** The bnf grammer of our expression language
  * <expr> := <and_expr> { <or_tok> <and_expr> }
  * <and_expr> := [not_tok] tag_tok { [not_tok] tag_tok }
  * <tag_tok> := alpha_num | quote_tok any_char quote_tok
  * <or_tok> := 'OR'
  * <not_tok> := '-'
  * <quote_tok> := '"'
  */

QString parseTag(const QString &string, int &currentIndex)
{
    const char QuoteTok = '\"';
    const QRegExp EndTok("[\\s\\\"\\-]");
    uint startIndex = 0;
    int endIndex = -1;

    // Get our start index and end index for our string, making sure
    // that our current index points to the end of the tag

    // Check if our first index is a quotation mark
    if (string[currentIndex] == QuoteTok)
    {
        // Check there is more string remaining
        if (static_cast<uint>(++currentIndex) >= string.length())
            return QString();

        // Our tag goes up to the next quotation mark
        startIndex = static_cast<uint>(currentIndex);
        endIndex = string.find(QuoteTok, currentIndex);
        currentIndex = endIndex == -1 ? -1: endIndex + 1;
    }
    else
    {
        // Find the end of our tag
        startIndex = static_cast<uint>(currentIndex);
        currentIndex = endIndex = string.find(EndTok, currentIndex);
    }

    uint length;
    if (endIndex == -1)
        length = string.length() - startIndex;
    else
        length = static_cast<uint>(endIndex) - startIndex;

    return string.mid(startIndex, length);
}

/** Creates a search container from a string expression. The container is
  * cleared before it is constructed from the string
  */
void createExpression(SearchContainer &container, const QString &string)
{
    container.clear();
    container.push_back(AndContainer());

    // Parse through our expression, skipping any preceding whitespace
    const QRegExp NotWs("\\S");
    const QString OrTok("OR");
    const char NotTok = '-';
    const int stringLength = static_cast<int>(string.length());

    int currentIndex = string.find(NotWs);

    while (currentIndex != -1 && currentIndex < stringLength)
    {
        if (string[currentIndex] == NotTok)
        {
            // A "-" must have a tag after it, so read it in
            if (++currentIndex < stringLength)
            {
                QString tag = parseTag(string, currentIndex);

                if (!tag.isEmpty())
                    // Valid tag, add it to our list
                    container.back().push_back(NotString(tag, true));
            }
        }
        else
        {
            // Read in our tag
            QString tag = parseTag(string, currentIndex);

            // Check if our tag is an OR
            if (tag.startsWith(OrTok, false) && tag.length() == OrTok.length())
                // Add a new set of and expressions
                container.push_back(AndContainer());
            else
                // Just a tag, add it to our list
                container.back().push_back(NotString(tag, false));
        }

        // Skip any whitespace before our next token
        if (currentIndex != -1 && currentIndex < stringLength)
            currentIndex = string.find(NotWs, currentIndex);
    }
}

bool evaluateAndExpression(const AndContainer &container,
    const QString &string, bool caseSensitive)
{
    for (AndContainer::const_iterator iter = container.begin();
         iter != container.end();
         ++iter)
    {
        bool found = string.find((*iter).first, 0, caseSensitive) != -1;

        // Check if the string is found only if it has no NOT
        if (found == (*iter).second)
            return false;
    }

    return true;
}

/**
  */
bool evaluateExpression(const SearchContainer &container,
    const QString &string, bool caseSensitive)
{
    for (SearchContainer::const_iterator iter = container.begin();
         iter != container.end();
         ++iter)
    {
        // Evaluate our and list if the list is not empty
        if (!(*iter).isEmpty() &&
            evaluateAndExpression(*iter, string, caseSensitive))
            return true;
    }

    return false;
}

} // private namespace

/** Implementation of our list view class
  */
class ListViewTagSearch::ListViewTagSearchImpl
{
public:
    ListViewTagSearchImpl() {}

    // The expression used to evaluate the search items, and the search
    // container
    QString stringExpr_;
    SearchContainer expr_;
};

// Interface functions

ListViewTagSearch::ListViewTagSearch(QWidget *parent, KListView *listView,
    const char *name)
: KListViewSearchLine(parent, listView, name),
  impl_(new ListViewTagSearchImpl)
{
}

ListViewTagSearch::ListViewTagSearch(QWidget *parent, const char *name)
: KListViewSearchLine(parent, name),
  impl_(new ListViewTagSearchImpl)
{
}

ListViewTagSearch::~ListViewTagSearch()
{
    delete impl_;
}
/*
void ListViewTagSearch::updateSearch(const QString &s)
{
    if (s.isNull())
    {
        createExpression(impl_->expr_, text());
        impl_->stringExpr_ = text();
    }
    else
    {
        createExpression(impl_->expr_, s);
        impl_->stringExpr_ = s;
    }

    // If there's a selected item that is visible, make sure that it's visible
    // when the search changes too (assuming that it still matches).
    // NB, here to the end of the func is just copied from KListViewSearchLine

    QListViewItem *currentItem = 0;

    switch(listView()->selectionMode())
    {
    case KListView::NoSelection:
        break;
    case KListView::Single:
        currentItem = listView()->selectedItem();
        break;
    default:
    {
        int flags = QListViewItemIterator::Selected | QListViewItemIterator::Visible;
        for(QListViewItemIterator it(listView(), flags);
            it.current() && !currentItem;
            ++it)
        {
            if(listView()->itemRect(it.current()).isValid())
                currentItem = it.current();
        }
    }
    }

    if(keepParentsVisible())
        checkItemParentsVisible(listView()->firstChild());
    else
        checkItemParentsNotVisible();

    if(currentItem)
        listView()->ensureItemVisible(currentItem);
}
*/
bool ListViewTagSearch::itemMatches(const QListViewItem *item,
    const QString &s) const
{
    // Not a valid search if the string is empty or contains only whitespace
    if (s.isEmpty() || s.find(QRegExp("\\S")) == -1)
        return true;

    if (!s.startsWith(impl_->stringExpr_, caseSensitive()) ||
        s.length() != impl_->stringExpr_.length())
    {
        createExpression(impl_->expr_, s);
        impl_->stringExpr_ = s;
    }

    // If the search column list is populated, search just the columns
    // specifified.  If it is empty default to searching all of the columns.
    if (!searchColumns().isEmpty())
    {
        for (QValueList<int>::ConstIterator it = searchColumns().constBegin();
             it != searchColumns().constEnd();
             ++it)
        {
            if (*it < item->listView()->columns() &&
                evaluateExpression(impl_->expr_, item->text(*it),
                    caseSensitive()))
                return true;
        }
    }
    else
    {
        for (int col = 0; col < item->listView()->columns(); ++col)
        {
            if (item->listView()->columnWidth(col) > 0 &&
                evaluateExpression(impl_->expr_, item->text(col),
                        caseSensitive()))
                return true;
        }
    }

    return false;
}

ListViewTagSearchWidget::ListViewTagSearchWidget(KListView *listView,
    QWidget *parent, const char *name)
: KListViewSearchLineWidget(listView, parent, name),
  searchLine_(0)
{
    searchLine_ = new ListViewTagSearch(this, listView);
}

KListViewSearchLine *ListViewTagSearchWidget::createSearchLine(
    KListView *)
{
    return searchLine_;
}

ListViewTagSearch *ListViewTagSearchWidget::searchLine() const
{
    return searchLine_;
}
