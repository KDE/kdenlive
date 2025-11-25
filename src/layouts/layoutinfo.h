/*
SPDX-FileCopyrightText: 2025 Kdenlive contributors
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QString>

/**
 * @brief Represents a single layout configuration. Used for presenting the layout in the switcher and in the layout manager dialog.
 */
struct LayoutInfo
{
    QString internalId;  // Internal ID like "kdenlive_editing"
    QString displayName; // User specified name or for default layouts this we be the translated name like "Editing"
    QString path;        // The path to the json layout
    QString verticalPath; // The path to the json vertical layout
    bool isDefault;      // Whether this is a default layout
    // int sortOrder;       // Custom order for display

    /**
     * @brief Constructor for a layout
     * @param id Internal ID of the layout
     * @param name Display name of the layout (translated)
     * @param layoutData Serialized layout data
     * @param defaultLayout Whether this is a default layout
     */
    LayoutInfo(const QString &id = QString(), const QString &name = QString())
        : internalId(id)
        , displayName(name)
    {
    }

    /**
     * @brief Check if the layout has valid data
     */
    bool isValid() const { return !internalId.isEmpty() && (!path.isEmpty() || !verticalPath.isEmpty()); }
    bool hasHorizontalData() const { return !path.isEmpty(); }
    bool hasVerticalData() const { return !verticalPath.isEmpty(); }
};
