/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderpresetrepository.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "renderpresetmodel.hpp"
#include "xml/xml.hpp"
#include <KLocalizedString>
#include <KMessageBox>
#include <QDir>
#include <QInputDialog>
#include <QStandardPaths>
#include <algorithm>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltProfile.h>

std::unique_ptr<RenderPresetRepository> RenderPresetRepository::instance;
std::once_flag RenderPresetRepository::m_onceFlag;
std::vector<std::pair<int, QString>> RenderPresetRepository::colorProfiles{{601, QStringLiteral("ITU-R BT.601")},
                                                                           {709, QStringLiteral("ITU-R BT.709")},
                                                                           {240, QStringLiteral("SMPTE ST240")},
                                                                           {9, QStringLiteral("ITU-R BT.2020")},
                                                                           {10, QStringLiteral("ITU-R BT.2020")}};
QStringList RenderPresetRepository::m_acodecsList;
QStringList RenderPresetRepository::m_vcodecsList;
QStringList RenderPresetRepository::m_supportedFormats;
static std::map<QString, QString> categoryMap{{QStringLiteral("Generic (HD for web, mobile devices...)"), QStringLiteral("generic")},
                                              {QStringLiteral("Ultra-High Definition (4K)"), QStringLiteral("4k")},
                                              {QStringLiteral("Video with Alpha"), QStringLiteral("alpha")},
                                              {QStringLiteral("Old-TV definition (DVD...)"), QStringLiteral("sd")},
                                              {QStringLiteral("Hardware Accelerated (experimental)"), QStringLiteral("hw")},
                                              {QStringLiteral("Audio only"), QStringLiteral("audio")},
                                              {QStringLiteral("Images sequence"), QStringLiteral("image")},
                                              {QStringLiteral("Custom"), QStringLiteral("custom")},
                                              {QStringLiteral("10 Bit"), QStringLiteral("10bit")},
                                              {QStringLiteral("Lossless/HQ"), QStringLiteral("lossless")}};

RenderPresetRepository::RenderPresetRepository()
{
    refresh();
}

std::unique_ptr<RenderPresetRepository> &RenderPresetRepository::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new RenderPresetRepository()); });
    return instance;
}

// static
void RenderPresetRepository::checkCodecs(bool forceRefresh)
{
    if (!(m_acodecsList.isEmpty() || m_vcodecsList.isEmpty() || m_supportedFormats.isEmpty() || forceRefresh)) {
        return;
    }
    Mlt::Profile p;
    auto *consumer = new Mlt::Consumer(p, "avformat");
    if (consumer) {
        consumer->set("vcodec", "list");
        consumer->set("acodec", "list");
        consumer->set("f", "list");
        consumer->start();
        consumer->stop();
        m_vcodecsList.clear();
        Mlt::Properties vcodecs(mlt_properties(consumer->get_data("vcodec")));
        m_vcodecsList.reserve(vcodecs.count());
        for (int i = 0; i < vcodecs.count(); ++i) {
            m_vcodecsList << QString(vcodecs.get(i));
        }
        m_acodecsList.clear();
        Mlt::Properties acodecs(mlt_properties(consumer->get_data("acodec")));
        m_acodecsList.reserve(acodecs.count());
        for (int i = 0; i < acodecs.count(); ++i) {
            m_acodecsList << QString(acodecs.get(i));
        }
        m_supportedFormats.clear();
        Mlt::Properties formats(mlt_properties(consumer->get_data("f")));
        m_supportedFormats.reserve(formats.count());
        for (int i = 0; i < formats.count(); ++i) {
            m_supportedFormats << QString(formats.get(i));
        }
        delete consumer;
    }
}

void RenderPresetRepository::refresh(bool fullRefresh)
{
    QWriteLocker locker(&m_mutex);

    if (fullRefresh) {
        // Reset all profiles
        m_profiles.clear();
        m_profileCategories.clear();
    }
    // Ensure MLT presets categories exist
    m_profileCategories = {{QStringLiteral("custom"), i18nc("Custom render preset category", "Custom")},
                           {QStringLiteral("10bit"), i18nc("10 Bit color depth category", "10 Bit")},
                           {QStringLiteral("lossless"), i18nc("Lossless rendering category", "Lossless/HQ")}};

    // Profiles downloaded by KNewStuff
    QString exportFolder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/export/");
    QDir directory(exportFolder);
    QStringList fileList = directory.entryList({QStringLiteral("*.xml")}, QDir::Files);

    // Parse our xml profile
    QString exportFile = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("export/profiles.xml"));
    parseFile(exportFile, false);

    // Parse some MLT's profiles
    parseMltPresets();

    // Parse files downloaded with KNewStuff
    for (const QString &filename : std::as_const(fileList)) {
        parseFile(directory.absoluteFilePath(filename), true);
    }

    // Parse customprofiles.xml last so custom profiles always override
    // all other profiles
    if (directory.exists(QStringLiteral("customprofiles.xml"))) {
        parseFile(directory.absoluteFilePath(QStringLiteral("customprofiles.xml")), true);
        // no need to parse this again
        fileList.removeAll(QStringLiteral("customprofiles.xml"));
    }
}

void RenderPresetRepository::parseFile(const QString &exportFile, bool editable)
{
    QDomDocument doc;
    if (!Xml::docContentFromFile(doc, exportFile, false)) {
        return;
    }
    QDomElement documentElement;
    QDomNodeList groups = doc.elementsByTagName(QStringLiteral("group"));

    if (editable || groups.isEmpty()) {
        QDomElement profiles = doc.documentElement();
        if (editable && profiles.attribute(QStringLiteral("version"), nullptr).toInt() < 1) {
            // this is an old profile version, update it
            QDomDocument newdoc;
            QDomElement newprofiles = newdoc.createElement(QStringLiteral("profiles"));
            newprofiles.setAttribute(QStringLiteral("version"), 1);
            newdoc.appendChild(newprofiles);
            QDomNodeList profilelist = doc.elementsByTagName(QStringLiteral("profile"));
            for (int i = 0; i < profilelist.count(); ++i) {
                QString category = i18nc("Category Name", "Custom");
                QString ext;
                QDomNode parent = profilelist.at(i).parentNode();
                if (!parent.isNull()) {
                    QDomElement parentNode = parent.toElement();
                    if (parentNode.hasAttribute(QStringLiteral("name"))) {
                        category = parentNode.attribute(QStringLiteral("name"));
                    }
                    ext = parentNode.attribute(QStringLiteral("extension"));
                }
                if (!profilelist.at(i).toElement().hasAttribute(QStringLiteral("category"))) {
                    profilelist.at(i).toElement().setAttribute(QStringLiteral("category"), category);
                }
                if (!ext.isEmpty()) {
                    profilelist.at(i).toElement().setAttribute(QStringLiteral("extension"), ext);
                }
                QDomNode n = profilelist.at(i).cloneNode();
                newprofiles.appendChild(newdoc.importNode(n, true));
            }
            QFile file(exportFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                KMessageBox::error(nullptr, i18n("Unable to write to file %1", exportFile));
                return;
            }
            QTextStream out(&file);
            out << newdoc.toString();
            file.close();
            // now that we fixed the file, run this function again
            parseFile(exportFile, editable);
            return;
        }

        QDomNode node = doc.elementsByTagName(QStringLiteral("profile")).at(0);
        if (node.isNull()) {
            return;
        }
        int count = 1;
        while (!node.isNull()) {
            QDomElement profile = node.toElement();
            QString category = profile.attribute(QStringLiteral("category"));
            if (!m_profileCategories.contains(category)) {
                // Probably an old profile version without group id, try to map
                if (!category.isEmpty() && categoryMap.count(category) > 0) {
                    category = categoryMap.at(category);
                }
                if (!category.isEmpty() && !m_profileCategories.contains(category)) {
                    // Check if the category name was translated
                    for (auto &c : categoryMap) {
                        if (i18n(c.first.toUtf8().constData()) == category) {
                            category = c.second;
                            break;
                        }
                    }
                }
                if (category.isEmpty()) {
                    category = QStringLiteral("custom");
                }
            }
            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(profile, exportFile, editable, category));
            if (m_profiles.count(model->name()) == 0) {
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            } else if (editable) {
                // Editable items should override existing items
                m_profiles.erase(model->name());
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            }
            node = doc.elementsByTagName(QStringLiteral("profile")).at(count);
            count++;
        }

        return;
    }

    int i = 0;

    while (!groups.item(i).isNull()) {
        documentElement = groups.item(i).toElement();
        QString groupId = documentElement.attribute("id");
        if (!m_profileCategories.contains(groupId)) {
            QString groupName = Xml::getSubTagContent(documentElement, QStringLiteral("name"));
            if (groupName.isEmpty()) {
                groupName = i18nc("Custom render preset category name", "Custom");
            } else {
                // Ensure render categories are translated
                groupName = i18n(groupName.toUtf8().constData());
            }
            m_profileCategories.insert(groupId, groupName);
        }
        QString renderer = documentElement.attribute(QStringLiteral("renderer"), QString());

        QDomNode n = groups.item(i).firstChild();
        while (!n.isNull()) {
            if (n.toElement().tagName() != QLatin1String("profile")) {
                n = n.nextSibling();
                continue;
            }
            QDomElement profile = n.toElement();
            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(profile, exportFile, editable, groupId, renderer));
            if (m_profiles.count(model->name()) == 0) {
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            }
            n = n.nextSibling();
        }
        ++i;
    }
}

QMap<QString, QString> RenderPresetRepository::getAllCategories()
{
    return m_profileCategories;
}

void RenderPresetRepository::parseMltPresets()
{
    QDir root(KdenliveSettings::mltpath());
    if (!root.cd(QStringLiteral("../presets/consumer/avformat"))) {
        // Cannot find MLT's presets directory
        qCWarning(KDENLIVE_LOG) << " / / / WARNING, cannot find MLT's preset folder";
        return;
    }
    if (root.exists(QStringLiteral("lossless"))) {
        QDir baseDir = root.absoluteFilePath(QStringLiteral("lossless"));
        const QStringList profiles = baseDir.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : profiles) {
            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("lossless"), baseDir.absoluteFilePath(prof), prof,
                                                                           QStringLiteral("properties=lossless/%1").arg(prof), true));
            if (m_profiles.count(model->name()) == 0) {
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            }
        }
    }
    if (root.exists(QStringLiteral("ten_bit"))) {
        QString groupName = i18n("10 Bit");
        QDir baseDir = root.absoluteFilePath(QStringLiteral("ten_bit"));
        const QStringList profiles = baseDir.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : profiles) {
            std::unique_ptr<RenderPresetModel> model(
                new RenderPresetModel(QStringLiteral("10bit"), baseDir.absoluteFilePath(prof), prof, QStringLiteral("properties=ten_bit/%1").arg(prof), true));
            if (m_profiles.count(model->name()) == 0) {
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            }
        }
    }
    if (root.exists(QStringLiteral("stills"))) {
        QString groupName = i18n("Images sequence");
        QDir baseDir = root.absoluteFilePath(QStringLiteral("stills"));
        QStringList profiles = baseDir.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : std::as_const(profiles)) {
            std::unique_ptr<RenderPresetModel> model(
                new RenderPresetModel(QStringLiteral("image"), baseDir.absoluteFilePath(prof), prof, QStringLiteral("properties=stills/%1").arg(prof), false));
            m_profiles.insert(std::make_pair(model->name(), std::move(model)));
        }
        // Add GIF as image sequence
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("image"), root.absoluteFilePath(QStringLiteral("GIF")),
                                                                       QStringLiteral("GIF"), QStringLiteral("properties=GIF"), false));
        if (m_profiles.count(model->name()) == 0) {
            m_profiles.insert(std::make_pair(model->name(), std::move(model)));
        }
    }
}

QVector<QString> RenderPresetRepository::getAllPresets() const
{
    QReadLocker locker(&m_mutex);

    QVector<QString> list;
    std::transform(m_profiles.begin(), m_profiles.end(), std::inserter(list, list.begin()), [](const auto &corresp) { return corresp.first; });
    std::sort(list.begin(), list.end());
    return list;
}

std::unique_ptr<RenderPresetModel> &RenderPresetRepository::getPreset(const QString &name)
{
    QReadLocker locker(&m_mutex);
    if (!presetExists(name)) {
        // TODO
        // qCWarning(KDENLIVE_LOG) << "//// WARNING: profile not found: " << path << ". Returning default profile instead.";
        /*QString default_profile = KdenliveSettings::default_profile();
        if (default_profile.isEmpty()) {
            default_profile = QStringLiteral("dv_pal");
        }
        if (m_profiles.count(default_profile) == 0) {
            qCWarning(KDENLIVE_LOG) << "//// WARNING: default profile not found: " << default_profile << ". Returning random profile instead.";
            return (*(m_profiles.begin())).second;
        }
        return m_profiles.at(default_profile);*/
    }
    return m_profiles.at(name);
}

bool RenderPresetRepository::presetExists(const QString &name) const
{
    QReadLocker locker(&m_mutex);
    return m_profiles.count(name) > 0;
}

const QString RenderPresetRepository::savePreset(RenderPresetModel *preset, bool editMode, const QString &oldName)
{

    QDomElement newPreset = preset->toXml();

    QDomDocument doc;

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/export/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QString fileName(dir.absoluteFilePath(QStringLiteral("customprofiles.xml")));
    if (dir.exists(QStringLiteral("customprofiles.xml")) && !Xml::docContentFromFile(doc, fileName, false)) {
        KMessageBox::error(nullptr, i18n("Cannot read file %1", fileName));
        return {};
    }

    QDomElement documentElement;
    QDomElement profiles = doc.documentElement();
    if (profiles.isNull() || profiles.tagName() != QLatin1String("profiles")) {
        doc.clear();
        profiles = doc.createElement(QStringLiteral("profiles"));
        profiles.setAttribute(QStringLiteral("version"), 1);
        doc.appendChild(profiles);
    }
    int version = profiles.attribute(QStringLiteral("version"), nullptr).toInt();
    if (version < 1) {
        doc.clear();
        profiles = doc.createElement(QStringLiteral("profiles"));
        profiles.setAttribute(QStringLiteral("version"), 1);
        doc.appendChild(profiles);
    }

    QDomNodeList profilelist = doc.elementsByTagName(QStringLiteral("profile"));
    // Check existing profiles
    QStringList existingProfileNames;
    int i = 0;
    while (!profilelist.item(i).isNull()) {
        documentElement = profilelist.item(i).toElement();
        QString profileName = documentElement.attribute(QStringLiteral("name"));
        existingProfileNames << profileName;
        i++;
    }

    QString newPresetName = preset->name();
    while (existingProfileNames.contains(newPresetName)) {
        QString updatedPresetName = newPresetName;
        if (!editMode) {
            bool ok;
            updatedPresetName = QInputDialog::getText(nullptr, i18n("Preset already exists"),
                                                      i18n("This preset name already exists. Change the name if you do not want to overwrite it."),
                                                      QLineEdit::Normal, newPresetName, &ok);
            if (!ok) {
                return {};
            }
        }

        if (updatedPresetName == newPresetName) {
            // remove previous profile
            int ix = existingProfileNames.indexOf(newPresetName);
            profiles.removeChild(profilelist.item(ix));
            existingProfileNames.removeAt(ix);
            break;
        }
        newPresetName = updatedPresetName;
        newPreset.setAttribute(QStringLiteral("name"), newPresetName);
    }
    if (editMode && !oldName.isEmpty() && existingProfileNames.contains(oldName)) {
        profiles.removeChild(profilelist.item(existingProfileNames.indexOf(oldName)));
    }

    profiles.appendChild(newPreset);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::error(nullptr, i18n("Cannot open file %1", file.fileName()));
        return {};
    }
    QTextStream out(&file);
    out << doc.toString();
    if (file.error() != QFile::NoError) {
        KMessageBox::error(nullptr, i18n("Cannot write to file %1", file.fileName()));
        file.close();
        return {};
    }
    file.close();
    refresh(true);
    return newPresetName;
}

bool RenderPresetRepository::deletePreset(const QString &name, bool dontRefresh)
{
    // TODO: delete a profile installed by KNewStuff the easy way
    /*
    QString edit = m_view.formats->currentItem()->data(EditableRole).toString();
    if (!edit.endsWith(QLatin1String("customprofiles.xml"))) {
        // This is a KNewStuff installed file, process through KNS
        KNS::Engine engine(0);
        if (engine.init("kdenlive_render.knsrc")) {
            KNS::Entry::List entries;
        }
        return;
    }*/

    if (!getPreset(name)->editable()) {
        return false;
    }

    QString exportFile = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/export/customprofiles.xml");
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file);
    file.close();

    QDomElement documentElement;
    QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
    if (profiles.isEmpty()) {
        return false;
    }
    int i = 0;
    QString profileName;
    while (!profiles.item(i).isNull()) {
        documentElement = profiles.item(i).toElement();
        profileName = documentElement.attribute(QStringLiteral("name"));
        if (profileName == name) {
            doc.documentElement().removeChild(profiles.item(i));
            break;
        }
        ++i;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::error(nullptr, i18n("Unable to write to file %1", exportFile));
        return false;
    }
    QTextStream out(&file);
    out << doc.toString();
    if (file.error() != QFile::NoError) {
        KMessageBox::error(nullptr, i18n("Cannot write to file %1", exportFile));
        file.close();
        return false;
    }
    file.close();

    if (!dontRefresh) {
        refresh(true);
    }
    return true;
}
