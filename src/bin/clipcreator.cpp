/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "clipcreator.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "klocalizedstring.h"
#include "macros.hpp"
#include "projectitemmodel.h"
#include "titler/titledocument.h"
#include "utils/devices.hpp"
#include "xml/xml.hpp"
#include <KMessageBox>
#include <QApplication>
#include <QDomDocument>
#include <QMimeDatabase>
#include <QWindow>

namespace {
QDomElement createProducer(QDomDocument &xml, ClipType type, const QString &resource, const QString &name, int duration, const QString &service)
{
    QDomElement prod = xml.createElement(QStringLiteral("producer"));
    xml.appendChild(prod);
    prod.setAttribute(QStringLiteral("type"), (int)type);
    prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
    prod.setAttribute(QStringLiteral("length"), duration);
    std::unordered_map<QString, QString> properties;
    properties[QStringLiteral("resource")] = resource;
    if (!name.isEmpty()) {
        properties[QStringLiteral("kdenlive:clipname")] = name;
    }
    if (!service.isEmpty()) {
        properties[QStringLiteral("mlt_service")] = service;
    }
    Xml::addXmlProperties(prod, properties);
    return prod;
}

} // namespace

QString ClipCreator::createTitleClip(const std::unordered_map<QString, QString> &properties, int duration, const QString &name, const QString &parentFolder,
                                     std::shared_ptr<ProjectItemModel> model)
{
    QDomDocument xml;

    auto prod = createProducer(xml, ClipType::Text, QString(), name, duration, QStringLiteral("kdenlivetitle"));
    Xml::addXmlProperties(prod, properties);

    QString id;
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create title clip"));
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createColorClip(const QString &color, int duration, const QString &name, const QString &parentFolder,
                                     std::shared_ptr<ProjectItemModel> model)
{
    QDomDocument xml;

    auto prod = createProducer(xml, ClipType::Color, color, name, duration, QStringLiteral("color"));

    QString id;
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create color clip"));
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createClipFromFile(const QString &path, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model, Fun &undo, Fun &redo)
{
    QDomDocument xml;
    QUrl url(path);
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForUrl(url);

    qDebug() << "/////////// createClipFromFile" << path << parentFolder << url << type.name();
    QDomElement prod;
    if (type.name().startsWith(QLatin1String("image/"))) {
        int duration = pCore->currentDoc()->getFramePos(KdenliveSettings::image_duration());
        prod = createProducer(xml, ClipType::Image, path, QString(), duration, QString());
    } else if (type.inherits(QStringLiteral("application/x-kdenlivetitle"))) {
        // opening a title file
        QDomDocument txtdoc(QStringLiteral("titledocument"));
        QFile txtfile(url.toLocalFile());
        if (txtfile.open(QIODevice::ReadOnly) && txtdoc.setContent(&txtfile)) {
            txtfile.close();
            // extract embedded images
            QDomNodeList items = txtdoc.elementsByTagName(QStringLiteral("content"));
            for (int j = 0; j < items.count(); ++j) {
                QDomElement content = items.item(j).toElement();
                if (content.hasAttribute(QStringLiteral("base64"))) {
                    QString titlesFolder = pCore->currentDoc()->projectDataFolder() + QStringLiteral("/titles/");
                    QString imgPath = TitleDocument::extractBase64Image(titlesFolder, content.attribute(QStringLiteral("base64")));
                    if (!imgPath.isEmpty()) {
                        content.setAttribute(QStringLiteral("url"), imgPath);
                        content.removeAttribute(QStringLiteral("base64"));
                    }
                }
            }
            prod.setAttribute(QStringLiteral("in"), 0);
            int duration = 0;
            if (txtdoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
                duration = txtdoc.documentElement().attribute(QStringLiteral("duration")).toInt();
            } else if (txtdoc.documentElement().hasAttribute(QStringLiteral("out"))) {
                duration = txtdoc.documentElement().attribute(QStringLiteral("out")).toInt();
            }
            if (duration <= 0) {
                duration = pCore->currentDoc()->getFramePos(KdenliveSettings::title_duration()) - 1;
            }
            prod = createProducer(xml, ClipType::Text, path, QString(), duration, QString());
            txtdoc.documentElement().setAttribute(QStringLiteral("duration"), duration);
            QString titleData = txtdoc.toString();
            prod.setAttribute(QStringLiteral("xmldata"), titleData);
        } else {
            txtfile.close();
            return QStringLiteral("-1");
        }
    } else {
        // it is a "normal" file, just use a producer
        prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("resource"), path);
        Xml::addXmlProperties(prod, properties);
        qDebug() << "/////////// normal" << url.toLocalFile() << properties << url;
    }

    qDebug() << "/////////// final xml" << xml.toString();
    QString id;
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, undo, redo);
    return res ? id : QStringLiteral("-1");
}

bool ClipCreator::createClipFromFile(const QString &path, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    auto id = ClipCreator::createClipFromFile(path, parentFolder, model, undo, redo);
    bool ok = (id != QStringLiteral("-1"));
    if (ok) {
        pCore->pushUndo(undo, redo, i18n("Add clip"));
    }
    return ok;
}

QString ClipCreator::createSlideshowClip(const QString &path, int duration, const QString &name, const QString &parentFolder,
                                         const std::unordered_map<QString, QString> &properties, std::shared_ptr<ProjectItemModel> model)
{
    QDomDocument xml;

    auto prod = createProducer(xml, ClipType::SlideShow, path, name, duration, QString());
    Xml::addXmlProperties(prod, properties);

    QString id;
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create slideshow clip"));
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createTitleTemplate(const QString &path, const QString &text, const QString &name, const QString &parentFolder,
                                         std::shared_ptr<ProjectItemModel> model)
{
    QDomDocument xml;

    // We try to retrieve duration for template
    int duration = 0;
    QDomDocument titledoc;
    QFile txtfile(path);
    if (txtfile.open(QIODevice::ReadOnly) && titledoc.setContent(&txtfile)) {
        if (titledoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
            duration = titledoc.documentElement().attribute(QStringLiteral("duration")).toInt();
        } else {
            // keep some time for backwards compatibility - 26/12/12
            duration = titledoc.documentElement().attribute(QStringLiteral("out")).toInt();
        }
    }
    txtfile.close();

    // Duration not found, we fall-back to defaults
    if (duration == 0) {
        duration = pCore->currentDoc()->getFramePos(KdenliveSettings::title_duration());
    }
    auto prod = createProducer(xml, ClipType::TextTemplate, path, name, duration, QString());
    if (!text.isEmpty()) {
        prod.setAttribute(QStringLiteral("templatetext"), text);
    }

    QString id;
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create title template"));
    return res ? id : QStringLiteral("-1");
}

bool ClipCreator::createClipsFromList(const QList<QUrl> &list, bool checkRemovable, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model,
                                      Fun &undo, Fun &redo)
{
    qDebug() << "/////////// creatclipsfromlist" << list << checkRemovable << parentFolder;
    bool created = false;
    QMimeDatabase db;
    for (const QUrl &file : list) {
        QMimeType type = db.mimeTypeForUrl(file);
        if (type.inherits(QLatin1String("inode/directory"))) {
            // user dropped a folder, import its files
            QDir dir(file.path());
            QStringList result = dir.entryList(QDir::Files);
            QList<QUrl> folderFiles;
            for (const QString &path : result) {
                folderFiles.append(QUrl::fromLocalFile(dir.absoluteFilePath(path)));
            }
            QString folderId;
            Fun local_undo = []() { return true; };
            Fun local_redo = []() { return true; };
            bool ok = pCore->projectItemModel()->requestAddFolder(folderId, dir.dirName(), parentFolder, local_undo, local_redo);
            if (ok) {
                ok = createClipsFromList(folderFiles, checkRemovable, folderId, model, local_undo, local_redo);
                if (!ok) {
                    local_undo();
                } else {
                    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
                }
            }
            continue;
        }
        if (checkRemovable && isOnRemovableDevice(file) && !isOnRemovableDevice(pCore->currentDoc()->projectDataFolder())) {
            int answer = KMessageBox::messageBox(
                QApplication::activeWindow(), KMessageBox::DialogType::WarningContinueCancel,
                i18n("Clip <b>%1</b><br /> is on a removable device, will not be available when device is unplugged or mounted at a different position. You "
                     "may want to copy it first to your hard-drive. Would you like to add it anyways?",
                     file.path()),
                i18n("Removable device"), KStandardGuiItem::yes(), KStandardGuiItem::no(), KStandardGuiItem::cancel(), QStringLiteral("removable"));

            if (answer == KMessageBox::Cancel) continue;
        }
        QString id = ClipCreator::createClipFromFile(file.toLocalFile(), parentFolder, model, undo, redo);
        created = created || (id != QStringLiteral("-1"));
    }
    qDebug() << "/////////// creatclipsfromlist return";
    return created;
}

bool ClipCreator::createClipsFromList(const QList<QUrl> &list, bool checkRemovable, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool ok = ClipCreator::createClipsFromList(list, checkRemovable, parentFolder, model, undo, redo);
    if (ok) {
        pCore->pushUndo(undo, redo, i18np("Add clip", "Add clips", list.size()));
    }
    return ok;
}
