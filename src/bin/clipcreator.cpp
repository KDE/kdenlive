/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipcreator.hpp"
#include "bin/bin.h"
#include "core.h"
#include "dialogs/clipcreationdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "klocalizedstring.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "projectitemmodel.h"
#include "titler/titledocument.h"
#include "utils/devices.hpp"
#include "xml/xml.hpp"
#include <KMessageBox>
#include <QApplication>
#include <QDomDocument>
#include <QMimeDatabase>
#include <QProgressDialog>
#include <utility>

namespace {
QDomElement createProducer(QDomDocument &xml, ClipType::ProducerType type, const QString &resource, const QString &name, int duration, const QString &service)
{
    QDomElement prod = xml.createElement(QStringLiteral("producer"));
    xml.appendChild(prod);
    prod.setAttribute(QStringLiteral("type"), int(type));
    if (type == ClipType::Timeline) {
        QUuid uuid = QUuid::createUuid();
        qDebug() << "::: CREATED XML PLAYLIST UUID: " << uuid;
        prod.setAttribute(QStringLiteral("kdenlive:uuid"), uuid.toString());
    }
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
                                     const std::shared_ptr<ProjectItemModel> &model)
{
    QDomDocument xml;
    auto prod = createProducer(xml, ClipType::Text, QString(), name, duration, QStringLiteral("kdenlivetitle"));
    Xml::addXmlProperties(prod, properties);

    QString id;
    std::function<void(const QString &)> callBack = [](const QString &binId) { pCore->activeBin()->selectClipById(binId); };
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create title clip"), callBack);
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createColorClip(const QString &color, int duration, const QString &name, const QString &parentFolder,
                                     const std::shared_ptr<ProjectItemModel> &model)
{
    QDomDocument xml;

    auto prod = createProducer(xml, ClipType::Color, color, name, duration, QStringLiteral("color"));

    QString id;
    std::function<void(const QString &)> callBack = [](const QString &binId) { pCore->activeBin()->selectClipById(binId); };
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create color clip"), callBack);
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createPlaylistClip(const QString &name, std::pair<int, int> tracks, const QString &parentFolder,
                                        const std::shared_ptr<ProjectItemModel> &model)
{
    const QUuid uuid = QUuid::createUuid();
    /*QDomDocument xml = pCore->currentDoc()->createEmptyDocument(2, 2, false);
    std::shared_ptr<Mlt::Producer> timeline2(new Mlt::Producer(pCore->getCurrentProfile()->profile(), "xml-string", xml.toString().toUtf8().constData()));
    Mlt::Service service(timeline2->get_service());
    Mlt::Tractor tc1(service);
    tc1.lock();*/
    std::shared_ptr<Mlt::Tractor> timeline(new Mlt::Tractor(pCore->getCurrentProfile()->profile()));
    timeline->lock();
    // Audio tracks
    for (int ix = 0; ix < tracks.first; ix++) {
        Mlt::Playlist pl(pCore->getCurrentProfile()->profile());
        timeline->insert_track(pl, ix);
        timeline->track(ix)->set("kdenlive:audio_track", 1);
    }
    for (int ix = tracks.first; ix < (tracks.first + tracks.second); ix++) {
        Mlt::Playlist pl(pCore->getCurrentProfile()->profile());
        timeline->insert_track(pl, ix);
        // Audio tracks
    }
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(timeline->get_producer()));
    prod->set("id", uuid.toString().toUtf8().constData());
    prod->set("kdenlive:uuid", uuid.toString().toUtf8().constData());
    prod->set("kdenlive:clipname", name.toUtf8().constData());
    prod->set("kdenlive:duration", 1);
    prod->set("kdenlive:clip_type", ClipType::Timeline);
    QString id;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Create the timelines folder to store timeline clips
    QString folderId;
    bool res = false;
    if (parentFolder == QLatin1String("-1")) {
        // Create timeline folder
        folderId = model->getFolderIdByName(i18n("Timelines"));
        if (folderId.isEmpty()) {
            res = model->requestAddFolder(folderId, i18n("Timelines"), QStringLiteral("-1"), undo, redo);
        } else {
            res = true;
        }
    }
    if (!res) {
        folderId = parentFolder;
    }
    res = model->requestAddBinClip(id, std::move(prod), folderId, undo, redo);
    if (res) {
        // Open playlist timeline
        qDebug() << "::: CREATED PLAYLIST WITH UUID: " << uuid << ", ID: " << id;
        pCore->projectManager()->openTimeline(id, uuid);
    }
    pCore->pushUndo(undo, redo, i18n("Create playlist clip"));
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createPlaylistClip(const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> producer,
                                        const QMap<QString, QString> mainProperties)
{
    QString id;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    QMapIterator<QString, QString> i(mainProperties);
    while (i.hasNext()) {
        i.next();
        producer->set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
    }
    QString folderId;
    bool res = false;
    if (parentFolder == QLatin1String("-1")) {
        // Create timeline folder
        folderId = model->getFolderIdByName(i18n("Timelines"));
        if (folderId.isEmpty()) {
            res = model->requestAddFolder(folderId, i18n("Timelines"), QStringLiteral("-1"), undo, redo);
        } else {
            res = true;
        }
    }
    if (!res) {
        folderId = parentFolder;
    }
    res = model->requestAddBinClip(id, producer, folderId, undo, redo);
    pCore->pushUndo(undo, redo, i18n("Create playlist clip"));
    return res ? id : QStringLiteral("-1");
}

QDomDocument ClipCreator::getXmlFromUrl(const QString &path)
{
    QDomDocument xml;
    QUrl fileUrl = QUrl::fromLocalFile(path);
    if (fileUrl.matches(pCore->currentDoc()->url(), QUrl::RemoveScheme | QUrl::NormalizePathSegments)) {
        // Cannot embed a project in itself
        KMessageBox::error(QApplication::activeWindow(), i18n("You cannot add a project inside itself."), i18n("Cannot create clip"));
        return xml;
    }
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForUrl(fileUrl);

    QDomElement prod;
    qDebug() << "=== GOT DROPPED MIME: " << type.name();
    if (type.name().startsWith(QLatin1String("image/")) && !type.name().contains(QLatin1String("image/gif"))) {
        int duration = pCore->getDurationFromString(KdenliveSettings::image_duration());
        prod = createProducer(xml, ClipType::Image, path, QString(), duration, QString());
    } else if (type.inherits(QStringLiteral("application/x-kdenlivetitle"))) {
        // opening a title file
        QDomDocument txtdoc(QStringLiteral("titledocument"));
        if (!Xml::docContentFromFile(txtdoc, path, false)) {
            return QDomDocument();
        }
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
        prod = createProducer(xml, ClipType::Text, path, QString(), -1, QString());
        QString titleData = txtdoc.toString();
        prod.setAttribute(QStringLiteral("xmldata"), titleData);
    } else {
        // it is a "normal" file, just use a producer
        prod = xml.createElement(QStringLiteral("producer"));
        xml.appendChild(prod);
        QMap<QString, QString> properties;
        properties.insert(QStringLiteral("resource"), path);
        Xml::addXmlProperties(prod, properties);
    }
    return xml;
}

QString ClipCreator::createClipFromFile(const QString &path, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model, Fun &undo, Fun &redo,
                                        const std::function<void(const QString &)> &readyCallBack)
{
    qDebug() << "/////////// createClipFromFile" << path << parentFolder;
    QDomDocument xml = getXmlFromUrl(path);
    if (xml.isNull()) {
        return QStringLiteral("-1");
    }
    qDebug() << "/////////// final xml" << xml.toString();
    QString id;
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, undo, redo, readyCallBack);
    return res ? id : QStringLiteral("-1");
}

bool ClipCreator::createClipFromFile(const QString &path, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    auto id = ClipCreator::createClipFromFile(path, parentFolder, std::move(model), undo, redo);
    bool ok = (id != QStringLiteral("-1"));
    if (ok) {
        pCore->pushUndo(undo, redo, i18nc("@action", "Add clip"));
    }
    return ok;
}

QString ClipCreator::createSlideshowClip(const QString &path, int duration, const QString &name, const QString &parentFolder,
                                         const std::unordered_map<QString, QString> &properties, const std::shared_ptr<ProjectItemModel> &model)
{
    QDomDocument xml;

    auto prod = createProducer(xml, ClipType::SlideShow, path, name, duration, QString());
    Xml::addXmlProperties(prod, properties);

    QString id;
    std::function<void(const QString &)> callBack = [](const QString &binId) { pCore->activeBin()->selectClipById(binId); };
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create slideshow clip"), callBack);
    return res ? id : QStringLiteral("-1");
}

QString ClipCreator::createTitleTemplate(const QString &path, const QString &text, const QString &name, const QString &parentFolder,
                                         const std::shared_ptr<ProjectItemModel> &model)
{
    QDomDocument xml;

    // We try to retrieve duration for template
    int duration = 0;
    QDomDocument titledoc;
    if (Xml::docContentFromFile(titledoc, path, false)) {
        if (titledoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
            duration = titledoc.documentElement().attribute(QStringLiteral("duration")).toInt();
        } else {
            // keep some time for backwards compatibility - 26/12/12
            duration = titledoc.documentElement().attribute(QStringLiteral("out")).toInt();
        }
    }

    // Duration not found, we fall-back to defaults
    if (duration == 0) {
        duration = pCore->getDurationFromString(KdenliveSettings::title_duration());
    }
    auto prod = createProducer(xml, ClipType::TextTemplate, path, name, duration, QString());
    if (!text.isEmpty()) {
        prod.setAttribute(QStringLiteral("templatetext"), text);
    }

    QString id;
    std::function<void(const QString &)> callBack = [](const QString &binId) { pCore->activeBin()->selectClipById(binId); };
    bool res = model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create title template"), callBack);
    return res ? id : QStringLiteral("-1");
}

const QString ClipCreator::createClipsFromList(const QList<QUrl> &list, bool checkRemovable, const QString &parentFolder,
                                               const std::shared_ptr<ProjectItemModel> &model, Fun &undo, Fun &redo, bool topLevel)
{
    QString createdItem;
    // Check for duplicates
    QList<QUrl> cleanList;
    QStringList duplicates;
    bool firstClip = topLevel;
    const QUuid uuid = model->uuid();
    pCore->bin()->shouldCheckProfile = (KdenliveSettings::default_profile().isEmpty() || KdenliveSettings::checkfirstprojectclip()) && pCore->bin()->isEmpty();
    for (const QUrl &url : list) {
        if (!pCore->projectItemModel()->urlExists(url.toLocalFile()) || QFileInfo(url.toLocalFile()).isDir()) {
            cleanList << url;
        } else {
            duplicates << url.toLocalFile();
        }
    }
    if (!duplicates.isEmpty()) {
        if (KMessageBox::warningYesNoList(QApplication::activeWindow(),
                                          i18n("The following clips are already inserted in the project. Do you want to duplicate them?"),
                                          duplicates) == KMessageBox::Yes) {
            cleanList = list;
        }
    }

    qDebug() << "/////////// creatclipsfromlist" << cleanList << checkRemovable << parentFolder;
    bool created = false;
    QMimeDatabase db;
    bool removableProject = checkRemovable ? isOnRemovableDevice(pCore->currentDoc()->projectDataFolder()) : false;
    int urlsCount = cleanList.count();
    int current = 0;
    for (const QUrl &file : qAsConst(cleanList)) {
        current++;
        if (model->uuid() != uuid) {
            // Project was closed, abort
            pCore->displayMessage(QString(), OperationCompletedMessage, 100);
            return QString();
        }
        if (!QFile::exists(file.toLocalFile())) {
            continue;
        }
        if (urlsCount > 3) {
            pCore->displayMessage(i18n("Loading clips"), ProcessingJobMessage, int(100 * current / urlsCount));
        }
        QFileInfo info(file.toLocalFile());
        if (info.isDir()) {
            // user dropped a folder, import its files
            QDir dir(file.toLocalFile());
            bool ok = false;
            QDir thumbFolder = pCore->currentDoc()->getCacheDir(CacheAudio, &ok);
            if (ok && thumbFolder == dir) {
                // Do not try to import our thumbnail folder
                continue;
            }
            thumbFolder = pCore->currentDoc()->getCacheDir(CacheThumbs, &ok);
            if (ok && thumbFolder == dir) {
                // Do not try to import our thumbnail folder
                continue;
            }
            thumbFolder = pCore->currentDoc()->getCacheDir(CacheProxy, &ok);
            if (ok && thumbFolder == dir) {
                // Do not try to import our thumbnail folder
                continue;
            }
            thumbFolder = pCore->currentDoc()->getCacheDir(CachePreview, &ok);
            if (ok && thumbFolder == dir) {
                // Do not try to import our thumbnail folder
                continue;
            }
            QString folderId;
            Fun local_undo = []() { return true; };
            Fun local_redo = []() { return true; };
            QStringList subfolders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            dir.setNameFilters(ClipCreationDialog::getExtensions());
            QStringList result = dir.entryList(QDir::Files);
            QList<QUrl> folderFiles;
            for (const QString &path : qAsConst(result)) {
                QUrl url = QUrl::fromLocalFile(dir.absoluteFilePath(path));
                folderFiles.append(url);
            }
            if (folderFiles.isEmpty()) {
                QList<QUrl> sublist;
                for (const QString &sub : qAsConst(subfolders)) {
                    QUrl url = QUrl::fromLocalFile(dir.absoluteFilePath(sub));
                    if (!list.contains(url)) {
                        sublist << url;
                    }
                }
                if (!sublist.isEmpty()) {
                    if (!KdenliveSettings::ignoresubdirstructure() || topLevel) {
                        // Create main folder
                        bool folderCreated = pCore->projectItemModel()->requestAddFolder(folderId, dir.dirName(), parentFolder, local_undo, local_redo);
                        if (!folderCreated) {
                            continue;
                        }
                    } else {
                        folderId = parentFolder;
                    }

                    createdItem = folderId;
                    // load subfolders
                    const QString clipId = createClipsFromList(sublist, checkRemovable, folderId, model, undo, redo, false);
                    if (createdItem.isEmpty() && clipId != QLatin1String("-1")) {
                        createdItem = clipId;
                    }
                }
            } else {
                if (!KdenliveSettings::ignoresubdirstructure() || topLevel) {
                    // Create main folder
                    bool folderCreated = pCore->projectItemModel()->requestAddFolder(folderId, dir.dirName(), parentFolder, local_undo, local_redo);
                    if (!folderCreated) {
                        continue;
                    }
                } else {
                    folderId = parentFolder;
                }
                createdItem = folderId;
                const QString clipId = createClipsFromList(folderFiles, checkRemovable, folderId, model, local_undo, local_redo, false);
                if (clipId.isEmpty() || clipId == QLatin1String("-1")) {
                    local_undo();
                } else {
                    if (createdItem.isEmpty()) {
                        createdItem = clipId;
                    }
                    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo)
                }
                // Check subfolders
                QList<QUrl> sublist;
                for (const QString &sub : qAsConst(subfolders)) {
                    QUrl url = QUrl::fromLocalFile(dir.absoluteFilePath(sub));
                    if (!list.contains(url)) {
                        sublist << url;
                    }
                }
                if (!sublist.isEmpty()) {
                    // load subfolders
                    createClipsFromList(sublist, checkRemovable, folderId, model, undo, redo, false);
                }
            }
        } else {
            // file is not a directory
            if (checkRemovable && isOnRemovableDevice(file) && !removableProject) {
                int answer = KMessageBox::warningContinueCancel(QApplication::activeWindow(),
                                                                i18n("Clip <b>%1</b><br /> is on a removable device, will not be available when device is "
                                                                     "unplugged or mounted at a different position.\nYou "
                                                                     "may want to copy it first to your hard-drive. Would you like to add it anyways?",
                                                                     file.path()),
                                                                i18n("Removable device"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                                                                QStringLiteral("confirm_removable_device"));

                if (answer == KMessageBox::Cancel) continue;
            }
            std::function<void(const QString &)> callBack = [](const QString &) {};
            if (firstClip) {
                callBack = [](const QString &binId) { pCore->activeBin()->selectClipById(binId); };
                firstClip = false;
            }
            if (model->uuid() != uuid) {
                // Project was closed, abort
                pCore->displayMessage(QString(), OperationCompletedMessage, 100);
                return QString();
            }
            const QString clipId = ClipCreator::createClipFromFile(file.toLocalFile(), parentFolder, model, undo, redo, callBack);
            if (createdItem.isEmpty() && clipId != QLatin1String("-1")) {
                createdItem = clipId;
            }
        }
        qApp->processEvents();
    }
    pCore->displayMessage(i18n("Loading done"), OperationCompletedMessage, 100);
    qDebug() << "/////////// creatclipsfromlist return" << created;
    return createdItem == QLatin1String("-1") ? QString() : createdItem;
}

const QString ClipCreator::createClipsFromList(const QList<QUrl> &list, bool checkRemovable, const QString &parentFolder,
                                               std::shared_ptr<ProjectItemModel> model)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    const QString id = ClipCreator::createClipsFromList(list, checkRemovable, parentFolder, std::move(model), undo, redo);
    if (!id.isEmpty()) {
        pCore->pushUndo(undo, redo, i18np("Add clip", "Add clips", list.size()));
    }
    return id;
}
