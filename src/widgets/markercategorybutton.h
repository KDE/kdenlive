/*
 * SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb.kdenlive@org>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QToolButton>
#include <QWidget>

class MarkerListModel;
class QMenu;

class MarkerCategoryButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(bool allowAll READ allowAll WRITE setAllowAll NOTIFY changed)
    Q_PROPERTY(bool onlyUsed READ onlyUsed WRITE setOnlyUsed NOTIFY changed)

public:
    MarkerCategoryButton(QWidget *parent = nullptr);
    /** @brief Set currently selected category by its number. */
    void setCurrentCategories(QList<int> categories);
    /** @brief get the number of the currently selected category. */
    QList<int> currentCategories();
    /** @brief Set the marker model of the chooser. Only needed if @property onlyUsed is true.*/
    void setMarkerModel(const MarkerListModel *model);
    /** @brief Whether the user should be able to select "All Categories" */
    void setAllowAll(bool allowAll);
    /** @brief Show only categories that are used by markers in the model.
     *  If no model is set, all categories will be show. @see setMarkerModel
     */
    void setOnlyUsed(bool onlyUsed);
    /** @brief Calling this will make the button checkable and have a default action on its own (for example enable/disable filtering) */
    void enableFilterMode();

private slots:
    void categorySelected(QAction *ac);

private:
    const MarkerListModel *m_markerListModel;
    bool m_allowAll;
    bool m_onlyUsed;
    QMenu *m_menu;

    void refresh();
    bool allowAll() { return m_allowAll; };
    bool onlyUsed() { return m_onlyUsed; };

signals:
    void changed();
    void categoriesChanged(const QList<int> categories);
};

