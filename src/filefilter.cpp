/* SPDX-FileCopyrightText: 2024 Pedro Oliva Rodrigues <pedroolivarodrigues@outlook.com>
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#include "filefilter.h"
#include "kdenlivesettings.h"
#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <QMimeDatabase>
#include <map>

#define qsl QStringLiteral
using namespace FileFilter;
using FileFilterDb = std::map<Category, QStringList>;

static FileFilterDb supported_file_exts;

static void initFilterDb() {
    // init mime types
    FileFilterDb supported_mime_types = {
        {Other, {
            qsl("application/x-kdenlivetitle"), 
            qsl("video/mlt-playlist"), 
            qsl("text/plain"), 
            qsl("application/x-kdenlive")
        }},
        {Video, {
            qsl("video/x-flv"),
            qsl("video/x-dv"),
            qsl("video/dv"),
            qsl("video/x-msvideo"),
            qsl("video/x-matroska"),
            qsl("video/mpeg"),
            qsl("video/ogg"),
            qsl("video/x-ms-wmv"),
            qsl("video/mp4"),
            qsl("video/quicktime"),
            qsl("video/webm"),
            qsl("video/3gpp"),
            qsl("video/mp2t")
        }},
        {Audio, {
            qsl("audio/AMR"),
            qsl("audio/x-flac"),
            qsl("audio/x-matroska"),
            qsl("audio/mp4"),
            qsl("audio/mpeg"),
            qsl("audio/x-mp3"),
            qsl("audio/ogg"),
            qsl("audio/x-wav"),
            qsl("audio/x-aiff"),
            qsl("audio/aiff"),
            qsl("application/ogg"),
            qsl("application/mxf"),
            qsl("application/x-shockwave-flash"),
            qsl("audio/ac3"),
            qsl("audio/aac")
        }},
        {Image, {
            qsl("image/gif"),
            qsl("image/jpeg"),
            qsl("image/png"),
            qsl("image/x-tga"),
            qsl("image/x-bmp"),
            qsl("image/tiff"),
            qsl("image/x-xcf"),
            qsl("image/x-xcf-gimp"),
            qsl("image/x-pcx"),
            qsl("image/x-exr"),
            qsl("image/x-portable-pixmap"),
            qsl("application/x-krita"),
            qsl("image/webp"),
            qsl("image/jp2"),
            qsl("image/avif"),
            qsl("image/heif"),
            qsl("image/jxl")
        }}
    };

    // Lottie animations
    bool allowLottie = KdenliveSettings::producerslist().contains("glaxnimate");
    if (allowLottie) {
        supported_mime_types[Other].append(qsl("application/json"));
    }

    // Some newer mimetypes might not be registered on some older Operating Systems, so register manually
    QMap<QString, QString> manualMap = {
        {qsl("image/avif"), qsl("*.avif")},
        {qsl("image/heif"), qsl("*.heif")},
        {qsl("image/x-exr"), qsl("*.exr")},
        {qsl("image/jp2"), qsl("*.jp2")},
        {qsl("image/jxl"), qsl("*.jxl")}
    };

    // init file_exts
    QMimeDatabase mimedb;

    for (const auto& item : supported_mime_types) {
        // TODO: Use C++17 unpacking here
        Category category;
        QStringList mimes;
        std::tie(category, mimes) = item;
        
        for (const auto& name : mimes) {
            QMimeType mime = mimedb.mimeTypeForName(name);
            QStringList globs;

            if (mime.isValid()) {
                globs.append(mime.globPatterns());
            } else if (manualMap.contains(name)) {
                globs.append(manualMap.value(name));
            }

            supported_file_exts[category].append(globs);
        }
    }
}

static QString i18n_label(Category type) {

    switch (type)
    {
    case Audio:
        return i18n("Audio Files");
    case Image:
        return i18n("Image Files");
    case Video:
        return i18n("Video Files");
    case User:
        return i18n("User Files");
    case Other:
        return i18n("Other Files");
    case All:
        return i18n("All Files");
    case AllSupported:
        return i18n("All Supported Files");
    default:
        // 💀💀💀
        return "????";
    }
}

static QStringList query(Category cat)
{
    switch (cat)
    {
    case Audio:
    case Image:
    case Other:
    case Video:
        return supported_file_exts.at(cat);

    case User: {
        const QStringList customs = KdenliveSettings::addedExtensions().split(' ', Qt::SkipEmptyParts);
        QStringList user;

        if (customs.isEmpty()) {
            return {};
        }

        for (const auto& ext : customs) {
            if (ext.startsWith("*.")) {
                user.append(ext);
            } else if (ext.startsWith(".")) {
                user.append(qsl("*") + ext);
            } else if (!ext.startsWith('.')) {
                user.append(qsl("*.") + ext);
            } else {
                qCDebug(KDENLIVE_LOG) << "Unrecognized custom format: " << ext;
            }
        }

        return user;
    }

    case AllSupported: {
        QStringList supported;
        for (auto cat : {Video, Audio, Image, Other}) {
            supported.append(supported_file_exts.at(cat));
        }

        return supported;
    }

    default:
        qCDebug(KDENLIVE_LOG) << "Unrecognized category type: " << (int)cat;
        // fallthrough
    case All:
        return {"*"};
    }
}


QStringList FileFilter::getExtensions()
{
    auto categories = {
        AllSupported, 
        All,
        Video, 
        Audio, 
        Image, 
        Other, 
        User
    };

    auto list = Builder().setCategories(categories).toExtensionsList();
    list.removeDuplicates();
    return list;
}


Builder::Builder() {
    if (supported_file_exts.empty()) {
        initFilterDb();
    }
}

Builder &Builder::setCategories(std::initializer_list<Category> types)
{
    m_types.assign(types);
    return *this;
}

QString Builder::toQFilter() const
{
    QStringList filters;

    for (auto cat : m_types) {
        QString label = i18n_label(cat);
        QStringList extensions = query(cat);

        filters.append(qsl("%1 (%2)").arg(label, extensions.join(' ')));
    }

    return filters.join(";;");
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QList<KFileFilter> Builder::toKFilter() const
{
    QList<KFileFilter> filters;

    for (auto cat : m_types) {
        QString label = i18n_label(cat);
        QStringList extensions = query(cat);

        filters.append(KFileFilter(label, extensions, {}));
    }

    return filters;
}
#else
QString Builder::toKFilter() const
{
    QStringList filters;

    for (auto cat : m_types) {
        QString label = i18n_label(cat);
        QStringList extensions = query(cat);

        filters.append(qsl("%2|%1").arg(label, extensions.join(' ')));
    }
    
    return filters.join("\n");
}
#endif

QStringList Builder::toExtensionsList() const
{
    QStringList allExtensions;

    for (auto cat : m_types) {
        allExtensions.append(query(cat));
    }

    return allExtensions;
}
