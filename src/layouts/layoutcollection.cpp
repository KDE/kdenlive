/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layouts/layoutcollection.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <QFile>
#include <QJsonArray>
#include <QStandardPaths>

LayoutCollection::LayoutCollection()
{
    initTranslations();
}

void LayoutCollection::initTranslations()
{
    LayoutInfo logging(QStringLiteral("kdenlive_logging"), i18n("Logging"), QString(), true);
    LayoutInfo editing(QStringLiteral("kdenlive_editing"), i18n("Editing"), QString(), true);
    LayoutInfo audio(QStringLiteral("kdenlive_audio"), i18n("Audio"), QString(), true);
    LayoutInfo effects(QStringLiteral("kdenlive_effects"), i18n("Effects"), QString(), true);
    LayoutInfo color(QStringLiteral("kdenlive_color"), i18n("Color"), QString(), true);

    // Add translated names to the collection even if we don't have the data yet
    // This ensures name translations work properly
    m_layouts[logging.internalId] = logging;
    m_layouts[editing.internalId] = editing;
    m_layouts[audio.internalId] = audio;
    m_layouts[effects.internalId] = effects;
    m_layouts[color.internalId] = color;
}

void LayoutCollection::addLayout(const LayoutInfo &layout)
{
    if (!layout.isValid()) {
        return;
    }

    m_layouts[layout.internalId] = layout;

    // Update order list if not already present
    if (!m_layoutOrder.contains(layout.internalId)) {
        m_layoutOrder.append(layout.internalId);
    }
}

bool LayoutCollection::removeLayout(const QString &id)
{
    if (!m_layouts.contains(id)) {
        return false;
    }

    m_layouts.remove(id);
    m_layoutOrder.removeAll(id);

    return true;
}

LayoutInfo LayoutCollection::getLayout(const QString &id) const
{
    if (m_layouts.contains(id)) {
        return m_layouts.value(id);
    }
    return LayoutInfo();
}

bool LayoutCollection::hasLayout(const QString &id) const
{
    return m_layouts.contains(id);
}

QList<LayoutInfo> LayoutCollection::getAllLayouts() const
{
    QList<LayoutInfo> result;

    // Return layouts in the correct order
    for (const QString &id : m_layoutOrder) {
        if (m_layouts.contains(id)) {
            result.append(m_layouts.value(id));
        }
    }

    return result;
}

LayoutCollection LayoutCollection::getDefaultLayouts()
{
    LayoutCollection collection;

    // Load default layouts from the default config
    KConfig defaultConfig(QStringLiteral("kdenlivedefaultlayouts.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup defaultOrder(&defaultConfig, "Order");
    KConfigGroup defaultLayout(&defaultConfig, "Layouts");

    QStringList defaultIds = defaultOrder.entryMap().values();

    for (const QString &id : defaultIds) {
        if (collection.m_layouts.contains(id)) {
            // We already have this as a translation, update with data
            LayoutInfo layout = collection.m_layouts[id];
            layout.data = defaultLayout.readEntry(id);
            collection.addLayout(layout);
        } else {
            // Create a new default layout
            LayoutInfo layout(id, id, defaultLayout.readEntry(id), true);
            collection.addLayout(layout);
        }
    }

    return collection;
}

void LayoutCollection::reorderLayouts(const QStringList &newOrder)
{
    m_layoutOrder.clear();
    for (const QString &id : newOrder) {
        if (m_layouts.contains(id)) {
            m_layoutOrder.append(id);
        }
    }
}

void LayoutCollection::loadFromConfig(KSharedConfigPtr config)
{
    // Keep translated names but clear any existing layout data
    QMap<QString, LayoutInfo> translatedLayouts = m_layouts;
    m_layouts.clear();
    m_layoutOrder.clear();

    // Restore translated names
    for (auto it = translatedLayouts.begin(); it != translatedLayouts.end(); ++it) {
        if (it.value().isDefault) {
            m_layouts[it.key()] = it.value();
        }
    }

    // Load from config
    KConfigGroup layoutGroup(config, "Layouts");
    KConfigGroup orderGroup(config, "Order");

    if (!layoutGroup.exists()) {
        // If user config doesn't exist, try main config
        KSharedConfigPtr mainConfig = KSharedConfig::openConfig();
        KConfigGroup mainLayoutGroup(mainConfig, "Layouts");
        if (mainLayoutGroup.exists()) {
            // Migrate to the new config
            mainLayoutGroup.copyTo(&layoutGroup);
        }
    }

    // If user order doesn't exist, fill from default order
    QStringList entries;
    if (!orderGroup.exists()) {
        // Load default layouts
        KConfig defaultConfig(QStringLiteral("kdenlivedefaultlayouts.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
        KConfigGroup defaultOrder(&defaultConfig, "Order");
        KConfigGroup defaultLayout(&defaultConfig, "Layouts");

        QStringList defaultLayouts = defaultOrder.entryMap().values();
        entries = layoutGroup.keyList();

        // Add default layouts to user config if they don't exist
        for (const QString &id : defaultLayouts) {
            if (!entries.contains(id)) {
                entries.insert(defaultLayouts.indexOf(id), id);
                layoutGroup.writeEntry(id, defaultLayout.readEntry(id));
            }
        }

        // Update order
        orderGroup.deleteGroup();
        int i = 1;
        for (const QString &id : entries) {
            orderGroup.writeEntry(QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0')), id);
            i++;
        }
        config->reparseConfiguration();
    } else {
        // User has a custom order
        entries = orderGroup.entryMap().values();
    }

    // Now build the layouts from the entries
    for (const QString &id : entries) {
        if (id.isEmpty()) {
            continue;
        }

        LayoutInfo layout;
        layout.internalId = id;
        layout.displayName = getTranslatedName(id);
        if (layoutGroup.hasKey(id)) {
            layout.data = layoutGroup.readEntry(id);
        }
        layout.isDefault = m_layouts.contains(id) && m_layouts[id].isDefault;

        // Add to collection
        addLayout(layout);
    }
}

void LayoutCollection::saveToConfig(KSharedConfigPtr config) const
{
    KConfigGroup layoutGroup(config, "Layouts");
    KConfigGroup orderGroup(config, "Order");

    // Clear existing entries
    layoutGroup.deleteGroup();
    orderGroup.deleteGroup();

    // Write layouts and order
    int i = 1;
    for (const QString &id : m_layoutOrder) {
        if (m_layouts.contains(id) && !id.isEmpty()) {
            // Save layout data
            layoutGroup.writeEntry(id, m_layouts[id].data);

            // Save order
            orderGroup.writeEntry(QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0')), id);
            i++;
        }
    }

    config->sync();
}

QString LayoutCollection::getTranslatedName(const QString &id) const
{
    // If we have a translation in our collection, use it
    if (m_layouts.contains(id)) {
        return m_layouts[id].displayName;
    }

    // Otherwise just return the ID
    return id;
}
