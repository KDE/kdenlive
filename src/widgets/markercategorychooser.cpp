/*
 * SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "markercategorychooser.h"
#include "../bin/model/markerlistmodel.hpp"
#include "core.h"

#include <KLocalizedString>

MarkerCategoryChooser::MarkerCategoryChooser(QWidget *parent)
    : QComboBox(parent)
    , m_markerListModel(nullptr)
    , m_allowAll(true)
    , m_onlyUsed(false)
{
    refresh();
    connect(this, &MarkerCategoryChooser::changed, this, &MarkerCategoryChooser::refresh);
}

void MarkerCategoryChooser::refresh()
{
    clear();
    if (pCore == nullptr) {
        return;
    }
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
        addItem(colorIcon, i.value().displayName, i.key());
    }
    if (count() == 0) {
        setEnabled(false);
        setPlaceholderText(i18n("Nothing to select"));
        return;
    }
    setEnabled(true);
    setPlaceholderText(QString());
    if (m_allowAll) {
        insertItem(0, i18n("All Categories"), -1);
        setCurrentIndex(0);
    }
}

void MarkerCategoryChooser::setCurrentCategory(int category)
{
    setCurrentIndex(findData(category));
}

int MarkerCategoryChooser::currentCategory()
{
    return currentData().toInt();
}

void MarkerCategoryChooser::setMarkerModel(const MarkerListModel *model)
{
    m_markerListModel = model;
    Q_EMIT changed();
}

void MarkerCategoryChooser::setAllowAll(bool allowAll)
{
    m_allowAll = allowAll;
    Q_EMIT changed();
}

void MarkerCategoryChooser::setOnlyUsed(bool onlyUsed)
{
    m_onlyUsed = onlyUsed;
    Q_EMIT changed();
}
