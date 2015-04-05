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


#include "ui_effectlist_ui.h"
#include "gentime.h"

#include <QDomElement>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDebug>

class EffectsList;
class EffectsListWidget;
class QTreeWidget;
class KActionCategory;

/**
 * @class MyDropButton
 * @brief A QToolButton accepting effect drops
 * @author Jean-Baptiste Mardelle
 */

class MyDropButton : public QToolButton
{
    Q_OBJECT

public:
    explicit MyDropButton(QWidget *parent = 0) : QToolButton(parent) {
        setAcceptDrops(true);
        setAutoExclusive(true);
        setCheckable(true);
        setAutoRaise(true);
    }
protected:
    void dragEnterEvent(QDragEnterEvent * event) {
        if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
            event->accept();
        }
    }
    void dropEvent(QDropEvent * event) {
        const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
        QDomDocument doc;
        doc.setContent(effects, true);
        QString id = doc.documentElement().attribute("id");
        if (id.isEmpty()) id = doc.documentElement().attribute("tag");
        emit addEffectToFavorites(id);
    }
signals:
    void addEffectToFavorites(QString);
};

/**
 * @class EffectsListView
 * @brief Manages the controls for the treewidget containing the effects.
 * @author Jean-Baptiste Mardelle
 */

class EffectsListView : public QWidget, public Ui::EffectList_UI
{
    Q_OBJECT

public:
    explicit EffectsListView(QWidget *parent = 0);

    /** @brief Re-initializes the list of effects. */
    void reloadEffectList(QMenu *effectsMenu, KActionCategory *effectActions);
    QMenu* getEffectsMenu();
    //void slotAddEffect(GenTime pos, int track, QString name);
    
    /** @brief Palette was changed, update styles. */
    void updatePalette();

private:
    EffectsListWidget *m_effectsList;
    const QString customStyleSheet() const;
    /** @brief Custom button to display favorite effects, accepts drops to add effect to favorites. */
    MyDropButton *m_effectsFavorites;
    /** @brief Action triggering remove effect from favorites or delete custom effect, depending on active tab. */
    QAction *m_removeAction;
    /** @brief Holds currently active tab index (all, video, audio, gpu, ...). */
    int m_filterPos;
    
    QMenu *m_contextMenu;

private slots:
    /** @brief Applies the type filter to the effect list.
    * @param pos Index of the combo box; where 0 = All, 1 = Video, 2 = Audio, 3 = GPU, 4 = Custom */
    void filterList();

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
    void slotAutoExpand(const QString &text);
    /** @brief Add an effect to the favorites
    * @param id id of the effect we want */
    void slotAddFavorite(QString id);
    void slotDisplayMenu(QTreeWidgetItem *item, const QPoint &pos);

signals:
    void addEffect(const QDomElement&);
    void reloadEffects();
};

#endif
