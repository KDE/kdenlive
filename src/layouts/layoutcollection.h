/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "layouts/layoutinfo.h"
#include <KSharedConfig>
#include <QList>
#include <QMap>
#include <QStringList>

/**
 * @brief Manages a collection of layouts
 */
class LayoutCollection
{
public:
    LayoutCollection();

    /**
     * @brief Add a layout to the collection
     * @param layout The layout to add
     */
    void addLayout(const LayoutInfo &layout);

    /**
     * @brief Remove a layout by ID
     * @param id The ID of the layout to remove
     * @return True if the layout was removed, false if it wasn't found
     */
    bool removeLayout(const QString &id);

    /**
     * @brief Get a layout by ID
     * @param id The ID of the layout to retrieve
     * @return The layout info, or an invalid layout if not found
     */
    LayoutInfo getLayout(const QString &id) const;

    /**
     * @brief Check if a layout exists in the collection
     * @param id The ID of the layout to check
     * @return True if the layout exists
     */
    bool hasLayout(const QString &id) const;

    /**
     * @brief Get all layouts in the collection
     * @return List of all layouts
     */
    QList<LayoutInfo> getAllLayouts() const;

    /**
     * @brief Get the default layout collection
     * @return A collection with default layouts
     */
    static LayoutCollection getDefaultLayouts();

    /**
     * @brief Reorder the layouts according to the given list of IDs
     * @param newOrder The new order of layout IDs
     */
    void reorderLayouts(const QStringList &newOrder);

    /**
     * @brief Load layouts from config
     * @param config The config to load from
     */
    void loadFromConfig(KSharedConfigPtr config);

    /**
     * @brief Save layouts to config
     * @param config The config to save to
     */
    void saveToConfig(KSharedConfigPtr config) const;

    /**
     * @brief Get the translated name for a layout ID
     * @param id The layout ID
     * @return The translated name, or the ID if not found
     */
    QString getTranslatedName(const QString &id) const;

private:
    QMap<QString, LayoutInfo> m_layouts; // Map of layouts by ID
    QStringList m_layoutOrder;           // Ordered list of layout IDs

    /**
     * @brief Initialize the layout translations
     */
    void initTranslations();
};