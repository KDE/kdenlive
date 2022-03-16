/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef RENDERPRESETREPOSITORY_H
#define RENDERPRESETREPOSITORY_H

#include "definitions.h" //for QString hash function
#include <QReadWriteLock>
#include <QString>
#include <memory>
#include <mutex>
#include <unordered_map>

class RenderPresetModel;

/** @class RenderPresetRepository
    @brief This class is used to read all the presets available to the user (MLT defaults, Kdenlive defaults and Custom ones).
    You can then query presets based on their name
    Note that this class is a Singleton, with Mutex protections to allow concurrent access.
 */
class RenderPresetRepository
{

public:
    // Returns the instance of the Singleton
    static std::unique_ptr<RenderPresetRepository> &get();

    /** @brief Reloads all the presets from the disk */
    void refresh(bool fullRefresh = false);

    /** @brief Returns a list of all the pairs (description, path) of all the presets loaded */
    QVector<QString> getAllPresets() const;

    QStringList groupNames() const { return m_groups; };

    /** @brief Returns a preset model given the presets's @param name
     */
    std::unique_ptr<RenderPresetModel> &getPreset(const QString &name);

    /** @brief Returns true if the given preset exists in repository
     */
    bool presetExists(const QString &name) const;

    static QStringList acodecs() { checkCodecs(); return m_acodecsList; };
    static QStringList vcodecs() { checkCodecs(); return m_vcodecsList; };
    static QStringList supportedFormats() { checkCodecs(); return m_supportedFormats; };

    /** @brief Saves given preset
     *  @returns The name of the saved preset
     */
    const QString savePreset(RenderPresetModel *profile, const QString &overrideGroupName = {});

    /** @brief Delete a (custom) preset*/
    bool deletePreset(const QString &path, bool dontRefresh = false);

protected:
    // Constructor is protected because class is a Singleton
    RenderPresetRepository();
    void parseFile(const QString &exportFile, bool editable);
    void parseMltPresets();

    static void checkCodecs(bool forceRefresh = false);

    static std::unique_ptr<RenderPresetRepository> instance;
    static std::once_flag m_onceFlag; // flag to create the repository only once;

    static std::vector<std::pair<int, QString>> colorProfiles;

    static QStringList m_acodecsList;
    static QStringList m_vcodecsList;
    static QStringList m_supportedFormats;

    mutable QReadWriteLock m_mutex;

    /** @brief map from the presets name to the instance of the preset.
     * @details We use unordered_map because QMap and QHash currently don't support
     * move insertion, hence inserting unique_ptr is impossible.
    */
    std::unordered_map<QString, std::unique_ptr<RenderPresetModel>> m_profiles;
    QStringList m_groups;
};

#endif
