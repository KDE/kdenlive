/*
 * SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb.kdenlive@org>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "markercategorybutton.h"
#include "../bin/model/markerlistmodel.hpp"
#include "core.h"

#include <KLocalizedString>
#include <QMenu>

MarkerCategoryButton::MarkerCategoryButton(QWidget *parent)
    : QToolButton(parent)
    , m_markerListModel(nullptr)
    , m_allowAll(true)
    , m_onlyUsed(false)
{
    setCheckable(false);
    setIcon(QIcon::fromTheme(QLatin1String("view-filter")));
    setPopupMode(QToolButton::InstantPopup);
    m_menu = new QMenu(this);
    setMenu(m_menu);
    if (pCore) {
        refresh();
    }
    connect(m_menu, &QMenu::triggered, this, &MarkerCategoryButton::categorySelected);
    connect(this, &MarkerCategoryButton::changed, this, &MarkerCategoryButton::refresh);
}

void MarkerCategoryButton::enableFilterMode()
{
    setCheckable(true);
    setPopupMode(QToolButton::MenuButtonPopup);
}

void MarkerCategoryButton::categorySelected(QAction *ac)
{
    int index = ac->data().toInt();
    QList<QAction *> actions = m_menu->actions();
    if (index == -1) {
        if (!ac->isChecked()) {
            // Don't allow unselecting all categories
            ac->setChecked(true);
            if (isCheckable()) {
                setChecked(false);
            }
            return;
        }
        // All categories selected, unselect any other
        for (auto &action : actions) {
            if (action != ac && action->isChecked()) {
                action->setChecked(false);
            }
        }
        emit categoriesChanged({-1});
        if (isCheckable()) {
            setChecked(false);
        }
        return;
    } else {
        // If a category is selected, ensure "all categories" is unchecked
        if (!ac->isChecked()) {
            bool categorySelected = false;
            for (auto &action : actions) {
                if (action->data().toInt() != -1 && action->isChecked()) {
                    categorySelected = true;
                    break;
                }
            }
            if (!categorySelected) {
                for (auto &action : actions) {
                    if (action->data().toInt() == -1) {
                        action->setChecked(true);
                        break;
                    }
                }
            }
        } else {
            // A category is checked, ensure "all categories is unchecked
            for (auto &action : actions) {
                if (action->data().toInt() == -1) {
                    action->setChecked(false);
                    break;
                }
            }
        }
    }
    QList<int> selection;
    for (auto &action : actions) {
        if (action->isChecked()) {
            selection << action->data().toInt();
        }
    }
    emit categoriesChanged(selection);
    if (isCheckable()) {
        setChecked(selection != (QList<int>() << -1));
    }
}

void MarkerCategoryButton::refresh()
{
    QList<int> selected;
    // remember selected categories
    QList<QAction *> actions = m_menu->actions();
    for (auto &ac : actions) {
        if (ac->isChecked()) {
            selected << ac->data().toInt();
        }
    }
    m_menu->clear();
    // Set up guide categories
    QPixmap pixmap(32, 32);
    QMapIterator<int, Core::MarkerCategory> i(pCore->markerTypes);
    while (i.hasNext()) {
        i.next();
        if (m_onlyUsed && m_markerListModel && m_markerListModel->getAllMarkers(i.key()).isEmpty()) {
            continue;
        }
        pixmap.fill(i.value().color);
        QIcon colorIcon(pixmap);
        QAction *ac = new QAction(colorIcon, i.value().displayName, m_menu);
        ac->setData(i.key());
        ac->setCheckable(true);
        ac->setChecked(selected.contains(i.key()));
        m_menu->addAction(ac);
    }
    if (m_menu->actions().count() == 0 && !m_allowAll) {
        setEnabled(false);
        setText(i18n("Nothing to select"));
        return;
    }
    setEnabled(true);
    if (m_allowAll) {
        QAction *ac = new QAction(i18n("All Categories"), m_menu);
        ac->setData(-1);
        ac->setCheckable(true);
        if (m_menu->actions().isEmpty()) {
            m_menu->addAction(ac);
        } else {
            m_menu->insertAction(m_menu->actions().first(), ac);
        }
        if (selected.isEmpty() || selected.contains(-1)) {
            ac->setChecked(true);
        }
    }
}

void MarkerCategoryButton::setCurrentCategories(QList<int> categories)
{
    QList<QAction *> actions = m_menu->actions();
    if (categories.contains(-1)) {
        for (auto &ac : actions) {
            if (ac->data().toInt() == -1) {
                ac->setChecked(true);
            } else {
                ac->setChecked(false);
            }
        }
    } else {
        for (auto &ac : actions) {
            int categoryIndex = ac->data().toInt();
            if (categoryIndex == -1) {
                ac->setChecked(false);
            } else {
                ac->setChecked(categories.contains(categoryIndex));
            }
        }
    }
}

QList<int> MarkerCategoryButton::currentCategories()
{
    QList<int> selected;
    QList<QAction *> actions = m_menu->actions();
    for (auto &ac : actions) {
        if (ac->isChecked()) {
            selected << ac->data().toInt();
        }
    }
    if (selected.contains(-1)) {
        return {-1};
    }
    return selected;
}

void MarkerCategoryButton::setMarkerModel(const MarkerListModel *model)
{
    m_markerListModel = model;
    connect(m_markerListModel, &MarkerListModel::categoriesChanged, this, &MarkerCategoryButton::changed);
    emit changed();
}

void MarkerCategoryButton::setAllowAll(bool allowAll)
{
    m_allowAll = allowAll;
    emit changed();
}

void MarkerCategoryButton::setOnlyUsed(bool onlyUsed)
{
    m_onlyUsed = onlyUsed;
    if (m_onlyUsed) {
        connect(m_menu, &QMenu::aboutToShow, this, &MarkerCategoryButton::changed);
    }
    emit changed();
}
