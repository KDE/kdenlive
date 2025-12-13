/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <julius.kuenzel@kde.org>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "layouts/layoutcollection.h"
#include "kdenlivesettings.h"
#include <KConfigGroup>
#include <KLocalizedString>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

LayoutCollection::LayoutCollection()
{
    initTranslations();
}

void LayoutCollection::initTranslations()
{
    // Ensure we have a reference to the translated names
    QStringList translations = {i18nc("Short layout name for logging interface", "Logging"), i18nc("Short layout name for editing interface", "Editing"),
                                i18nc("Short layout name for audio interface", "Audio"), i18nc("Short layout name for effects interface", "Effects"),
                                i18nc("Short layout name for color grading interface", "Color")};
}

void LayoutCollection::addLayout(const LayoutInfo &layout)
{
    qDebug() << ":::::: ADDING LAYOUT: " << layout.internalId;
    if (!layout.isValid()) {
        qDebug() << ":::::: ADDING LAYOUT INVALID!!! " << layout.internalId;
        return;
    }

    m_layouts[layout.internalId] = layout;

    // Update order list if not already present
    if (!KdenliveSettings::layoutsOrder().contains(layout.internalId)) {
        QStringList order = KdenliveSettings::layoutsOrder();
        order << layout.internalId;
        KdenliveSettings::setLayoutsOrder(order);
    }
}

bool LayoutCollection::removeLayout(const QString &id)
{
    if (!m_layouts.contains(id)) {
        return false;
    }

    m_layouts.remove(id);
    QStringList order = KdenliveSettings::layoutsOrder();
    order.removeAll(id);
    KdenliveSettings::setLayoutsOrder(order);
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
    for (const QString &id : KdenliveSettings::layoutsOrder()) {
        if (m_layouts.contains(id)) {
            result.append(m_layouts.value(id));
        }
    }

    return result;
}

void LayoutCollection::resetDefaultLayouts()
{
    LayoutCollection collection;

    // Remove any custom layout overwriting defaults
    QDir customLayoutsFolder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (customLayoutsFolder.cd(QStringLiteral("layouts"))) {
        for (auto &id : KdenliveSettings::defaultLayoutsOrderValue()) {
            if (!m_layouts.contains(id)) {
                // This should not happen, default layouts should always be there
                continue;
            }
            if (customLayoutsFolder.exists(id + QStringLiteral(".json"))) {
                customLayoutsFolder.remove(id + QStringLiteral(".json"));
            }
            if (customLayoutsFolder.exists(id + QStringLiteral("_vertical.json"))) {
                customLayoutsFolder.remove(id + QStringLiteral("_vertical.json"));
            }
        }
    }

    // Reset layouts order and reload
    KdenliveSettings::setLayoutsOrder({});
    loadLayouts();
}

void LayoutCollection::loadLayouts()
{
    // Keep translated names but clear any existing layout data
    m_layouts.clear();

    // Search layouts
    QMap<QString, LayoutInfo> foundlayouts;
    const QStringList layoutsFolders =
        QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, QStringLiteral("layouts"), QStandardPaths::LocateDirectory);
    for (const QString &folderpath : layoutsFolders) {
        QDir dir(folderpath);
        QDirIterator it(dir.absolutePath(), {QStringLiteral("*.json")}, QDir::Files);
        while (it.hasNext()) {
            const QFileInfo fInfo(dir.absoluteFilePath(it.next()));
            QString layoutId = fInfo.baseName();
            if (foundlayouts.contains(layoutId) && !fInfo.isWritable()) {
                // Only overwrite if the folder is writable
                auto layout = foundlayouts.value(layoutId);
                if (layoutId.endsWith(QLatin1String("_vertical"))) {
                    // We are processing a vertical layout, ensure we have none yet
                    if (layout.hasVerticalData()) {
                        continue;
                    }
                } else if (layout.hasHorizontalData()) {
                    continue;
                }
            }
            // Read layout info
            QFile loadFile(fInfo.absoluteFilePath());
            if (!loadFile.open(QIODevice::ReadOnly)) {
                qDebug() << "// Cannot read layout: " << fInfo.absoluteFilePath();
                continue;
            }
            const QByteArray saveData = loadFile.readAll();
            QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
            QVariantList results = loadDoc["kdenliveInfo"].toArray().toVariantList();
            if (results.isEmpty()) {
                qDebug() << "// No Kdenlive data found in layout: " << fInfo.absoluteFilePath();
                continue;
            }
            if (layoutId.endsWith(QLatin1String("_vertical"))) {
                // Remove _vertical suffix from id
                layoutId.chop(9);
            }
            const QString displayName = results[0].toMap().value(QStringLiteral("displayName")).toString();
            const QString insideId = results[0].toMap().value(QStringLiteral("id")).toString();
            bool isDefault = results[0].toMap().value(QStringLiteral("default"), false).toBool();
            bool isVertical = results[0].toMap().value(QStringLiteral("vertical"), false).toBool();
            if (!insideId.isEmpty()) {
                // Allow overriding id based on file name
                layoutId = insideId;
            }
            qDebug() << ":::: LOADING LAYOUT: " << layoutId << ", VERTICAL: " << isVertical;
            LayoutInfo layout;
            if (foundlayouts.contains(layoutId)) {
                layout = foundlayouts.value(layoutId);
                if ((isVertical && !layout.verticalPath.isEmpty()) || (!isVertical && !layout.path.isEmpty())) {
                    // Duplicate layout
                    qDebug() << "Duplicate layout, ignoring: " << layoutId << " = " << fInfo.absoluteFilePath();
                    continue;
                }
            } else {
                layout = LayoutInfo(layoutId, isDefault ? i18n(displayName.toUtf8().constData()) : displayName);
            }
            layout.isDefault = isDefault;
            if (isVertical) {
                layout.verticalPath = fInfo.absoluteFilePath();
            } else {
                layout.path = fInfo.absoluteFilePath();
            }
            foundlayouts.insert(layoutId, layout);
        }
    }

    QStringList previousOrder = KdenliveSettings::layoutsOrder();
    KdenliveSettings::setLayoutsOrder({});
    if (previousOrder.isEmpty()) {
        // Default layout order
        previousOrder = KdenliveSettings::defaultLayoutsOrderValue();
    }

    // Now load the layouts from the entries
    for (const QString &id : std::as_const(previousOrder)) {
        if (id.isEmpty() || !foundlayouts.contains(id)) {
            continue;
        }
        LayoutInfo layout = foundlayouts.take(id);
        if (layout.isValid()) {
            addLayout(layout);
        }
    }
    // Add remaining layouts not yet in order
    for (auto i = foundlayouts.cbegin(), end = foundlayouts.cend(); i != end; ++i) {
        if (i.value().isDefault) {
            // Default layout not in list, was removed so skip
            continue;
        }
        if (i.value().isValid()) {
            addLayout(i.value());
        }
    }
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
