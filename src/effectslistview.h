/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef EFFECTSLISTVIEW_H
#define EFFECTSLISTVIEW_H

#include <KIcon>

#include "ui_effectlist_ui.h"
#include "gentime.h"

#include <QDomElement>
#include <QFocusEvent>

class EffectsList;
class EffectsListWidget;
class QTreeWidget;

/**
 * @class EffectsListView
 * @brief Manages the controls for the treewidget containing the effects.
 * @author Jean-Baptiste Mardelle
 */

class EffectsListView : public QWidget, public Ui::EffectList_UI
{
    Q_OBJECT

public:
    EffectsListView(QWidget *parent = 0);

    /** @brief Re-initializes the list of effets. */
    void reloadEffectList();
    //void slotAddEffect(GenTime pos, int track, QString name);

private:
    EffectsListWidget *m_effectsList;

private slots:
    /** @brief Applies the type filter to the effect list.
    * @param pos Index of the combo box; where 0 = All, 1 = Video, 2 = Audio, 3 = Custom */
    void filterList(int pos);

    /** @brief Updates the info panel to match the selected effect. */
    void slotUpdateInfo();

    /** @brief Toggles the info panel's visibility. */
    void showInfoPanel();

    /** @brief Emits addEffect signal for the selected effect. */
    void slotEffectSelected();

    /** @brief Removes the XML file for the selected effect.
    *
    * Only used for custom effects */
    void slotRemoveEffect();

    /** @brief Makes sure the item fits the type filter.
    * @param item Current item
    * @param hidden Hidden or not
    *
    * This is necessary to make the search obey to the type filter.
    * Called when the visibility of this item was changed by searching */
    void slotUpdateSearch(QTreeWidgetItem *item, bool hidden);

    /** @brief Expands folders that match our search.
    * @param text Current search string */
    void slotAutoExpand(QString text);

signals:
    void addEffect(const QDomElement);
    void reloadEffects();
};

#endif
