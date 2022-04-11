/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderpresetrepository.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "renderpresetmodel.hpp"
#include <KLocalizedString>
#include <KMessageBox>
#include <QDir>
#include <QInputDialog>
#include <QStandardPaths>
#include <algorithm>
#include <mlt++/MltProfile.h>
#include <mlt++/MltConsumer.h>

std::unique_ptr<RenderPresetRepository> RenderPresetRepository::instance;
std::once_flag RenderPresetRepository::m_onceFlag;
std::vector<std::pair<int, QString>> RenderPresetRepository::colorProfiles{
    {601, QStringLiteral("ITU-R BT.601")}, {709, QStringLiteral("ITU-R BT.709")}, {240, QStringLiteral("SMPTE ST240")}, {9, QStringLiteral("ITU-R BT.2020")}, {10, QStringLiteral("ITU-R BT.2020")}};
QStringList RenderPresetRepository::m_acodecsList;
QStringList RenderPresetRepository::m_vcodecsList;
QStringList RenderPresetRepository::m_supportedFormats;

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
        m_groups.clear();
    }

    // Profiles downloaded by KNewStuff
    QString exportFolder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/export/");
    QDir directory(exportFolder);
    QStringList fileList = directory.entryList({QStringLiteral("*.xml")}, QDir::Files);

    // Parse customprofiles.xml always first so custom profiles allways overide
    // profiles downloaded with KNewStuff
    if (directory.exists(QStringLiteral("customprofiles.xml"))) {
        parseFile(directory.absoluteFilePath(QStringLiteral("customprofiles.xml")), true);
        // no need to parse this again
        fileList.removeAll(QStringLiteral("customprofiles.xml"));
    }
    // Parse files downloaded with KNewStuff
    for (const QString &filename : qAsConst(fileList)) {
        parseFile(directory.absoluteFilePath(filename), true);
    }

    // Parse some MLT's profiles
    parseMltPresets();

    // Parse our xml profile
    QString exportFile = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("export/profiles.xml"));
    parseFile(exportFile, false);

    //focusFirstVisibleItem(selectedProfile);
}

void RenderPresetRepository::parseFile(const QString &exportFile, bool editable)
{
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();
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
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                KMessageBox::sorry(nullptr, i18n("Unable to write to file %1", exportFile));
                return;
            }
            QTextStream out(&file);
            out.setCodec("UTF-8");
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

            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(profile, exportFile, editable));

            if (m_profiles.count(model->name()) == 0) {
                m_groups.append(model->groupName());
                m_groups.removeDuplicates();
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
        QString groupName = documentElement.attribute(QStringLiteral("name"), i18nc("Attribute Name", "Custom"));
        QString extension = documentElement.attribute(QStringLiteral("extension"), QString());
        QString renderer = documentElement.attribute(QStringLiteral("renderer"), QString());

        QDomNode n = groups.item(i).firstChild();
        while (!n.isNull()) {
            if (n.toElement().tagName() != QLatin1String("profile")) {
                n = n.nextSibling();
                continue;
            }
            QDomElement profile = n.toElement();

            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(profile, exportFile, editable, groupName, renderer));
            if (m_profiles.count(model->name()) == 0) {
                m_groups.append(model->groupName());
                m_groups.removeDuplicates();
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            }
            n = n.nextSibling();
        }

        ++i;
    }
}

void RenderPresetRepository::parseMltPresets()
{
    QDir root(KdenliveSettings::mltpath());
    if (!root.cd(QStringLiteral("../presets/consumer/avformat"))) {
        // Cannot find MLT's presets directory
        qCWarning(KDENLIVE_LOG) << " / / / WARNING, cannot find MLT's preset folder";
        return;
    }
    if (root.cd(QStringLiteral("lossless"))) {
        QString groupName = i18n("Lossless/HQ");
        const QStringList profiles = root.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : profiles) {
            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(groupName, root.absoluteFilePath(prof), prof, QString("properties=lossless/" + prof), true));
            if (m_profiles.count(model->name()) == 0) {
                m_groups.append(model->groupName());
                m_groups.removeDuplicates();
                m_profiles.insert(std::make_pair(model->name(), std::move(model)));
            }
        }
    }
    if (root.cd(QStringLiteral("../stills"))) {
        QString groupName = i18nc("Category Name", "Images sequence");
        QStringList profiles = root.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : qAsConst(profiles)) {
            std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(groupName, root.absoluteFilePath(prof), prof, QString("properties=stills/" + prof), false));
            m_groups.append(model->groupName());
            m_groups.removeDuplicates();
            m_profiles.insert(std::make_pair(model->name(), std::move(model)));
        }
        // Add GIF as image sequence
        root.cdUp();
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(groupName, root.absoluteFilePath(QStringLiteral("GIF")), QStringLiteral("GIF"), QStringLiteral("properties=GIF"), false));
        if (m_profiles.count(model->name()) == 0) {
            m_groups.append(model->groupName());
            m_groups.removeDuplicates();
            m_profiles.insert(std::make_pair(model->name(), std::move(model)));
        }
    }
}

QVector<QString> RenderPresetRepository::getAllPresets() const
{
    QReadLocker locker(&m_mutex);

    QVector<QString> list;
    std::transform(m_profiles.begin(), m_profiles.end(), std::inserter(list, list.begin()),
                   [&](decltype(*m_profiles.begin()) corresp) { return corresp.first; });
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
    QFile file(dir.absoluteFilePath(QStringLiteral("customprofiles.xml")));
    doc.setContent(&file, false);
    file.close();
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

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::sorry(nullptr, i18n("Cannot open file %1", file.fileName()));
        return {};
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
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
    doc.setContent(&file, false);
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
        KMessageBox::sorry(nullptr, i18n("Unable to write to file %1", exportFile));
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
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
