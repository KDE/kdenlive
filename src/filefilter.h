/* SPDX-FileCopyrightText: 2024 Pedro Oliva Rodrigues <pedroolivarodrigues@outlook.com>
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once

#include <QString>
#include <vector>
#include <initializer_list>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <KFileFilter>
#endif

/* @brief This namespace contains methods to query the supported file types and
   create file filters for file pickers.
*/
namespace FileFilter {
    enum Category {
        Video, // mp4, webm ...
        Audio, // mp3, aac, ...
        Image, // png, jpeg, ...
        Other, // kdenlivetitle, mlt-playlist, ...
        AllSupported, // all of the above in one
        User, // from 'KdenliveSettings::addedExtensions'
        All // all files (*.*)
    };

    QStringList getExtensions();

    /* @class FileFilter::Builder
       @brief A class that creates file filter for file pickers.
    */
    class Builder {
    public:
        Builder();

        Builder& setCategories(std::initializer_list<Category> cats);

        Builder& defaultCategories() {
            auto def = {
                AllSupported, 
                All,
                Video, 
                Audio, 
                Image, 
                Other, 
                User
            };

            return setCategories(def);
        }

        QString toQFilter() const;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QList<KFileFilter> toKFilter() const;
#else
        QString toKFilter() const;
#endif

        QStringList toExtensionsList() const;

    private:
        std::vector<Category> m_types;
    };

} // namespace FileFilter
