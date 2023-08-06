/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "documentchecker.h"
#include "bin/binplaylist.hpp"
#include "bin/projectclip.h"
#include "dcresolvedialog.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "titler/titlewidget.h"
#include "transitions/transitionsrepository.hpp"
#include "xml/xml.hpp"

#include <KLocalizedString>

#include <QCryptographicHash>
#include <QStandardPaths>

QDebug operator<<(QDebug qd, const DocumentChecker::DocumentResource &item)
{
    qd << "Type:" << DocumentChecker::readableNameForMissingType(item.type);
    qd << "Status:" << DocumentChecker::readableNameForMissingStatus(item.status);
    qd << "Original Paths:" << item.originalFilePath;
    qd << "New Path:" << item.newFilePath;
    qd << "clipID:" << item.clipId;
    return qd.maybeSpace();
}

DocumentChecker::DocumentChecker(QUrl url, const QDomDocument &doc)
    : m_url(std::move(url))
    , m_doc(doc)
{

    QDomElement baseElement = m_doc.documentElement();
    m_root = baseElement.attribute(QStringLiteral("root"));
    if (!m_root.isEmpty()) {
        QDir dir(m_root);
        if (!dir.exists()) {
            // Looks like project was moved, try recovering root from current project url
            m_rootReplacement.first = dir.absolutePath() + QDir::separator();
            m_root = m_url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
            baseElement.setAttribute(QStringLiteral("root"), m_root);
            m_root = QDir::cleanPath(m_root) + QDir::separator();
            m_rootReplacement.second = m_root;
        } else {
            m_root = QDir::cleanPath(m_root) + QDir::separator();
        }
    }
}

const QMap<QString, QString> DocumentChecker::getLumaPairs() const
{
    QMap<QString, QString> lumaSearchPairs;
    lumaSearchPairs.insert(QStringLiteral("luma"), QStringLiteral("resource"));
    lumaSearchPairs.insert(QStringLiteral("movit.luma_mix"), QStringLiteral("resource"));
    lumaSearchPairs.insert(QStringLiteral("composite"), QStringLiteral("luma"));
    lumaSearchPairs.insert(QStringLiteral("region"), QStringLiteral("composite.luma"));
    return lumaSearchPairs;
}

const QMap<QString, QString> DocumentChecker::getAssetPairs() const
{
    QMap<QString, QString> assetSearchPairs;
    assetSearchPairs.insert(QStringLiteral("avfilter.lut3d"), QStringLiteral("av.file"));
    assetSearchPairs.insert(QStringLiteral("shape"), QStringLiteral("resource"));
    return assetSearchPairs;
}

bool DocumentChecker::resolveProblemsWithGUI()
{
    if (m_items.size() == 0) {
        return true;
    }
    DCResolveDialog *d = new DCResolveDialog(m_items);
    // d->show(getInfoMessages());
    if (d->exec() == QDialog::Rejected) {
        return false;
    }

    QList<DocumentResource> items = d->getItems();

    QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList chains = m_doc.elementsByTagName(QStringLiteral("chain"));

    QDomNodeList trans = m_doc.elementsByTagName(QStringLiteral("transition"));
    QDomNodeList filters = m_doc.elementsByTagName(QStringLiteral("filter"));

    for (auto item : items) {
        fixMissingItem(item, producers, chains, trans, filters);
        qApp->processEvents();
    }

    QStringList tractorIds;
    QDomNodeList documentTractors = m_doc.elementsByTagName(QStringLiteral("tractor"));
    int max = documentTractors.count();
    for (int i = 0; i < max; ++i) {
        QDomElement tractor = documentTractors.item(i).toElement();
        tractorIds.append(tractor.attribute(QStringLiteral("id")));
    }

    max = producers.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = producers.item(i).toElement();
        fixSequences(e, producers, tractorIds);
    }
    max = chains.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = chains.item(i).toElement();
        fixSequences(e, producers, tractorIds);
    }

    // original doc was modified
    m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
    return true;
}

bool DocumentChecker::hasErrorInProject()
{
    m_items.clear();

    QString storageFolder;
    QDir projectDir(m_url.adjusted(QUrl::RemoveFilename).toLocalFile());
    QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
    for (int i = 0; i < playlists.count(); ++i) {
        if (playlists.at(i).toElement().attribute(QStringLiteral("id")) == BinPlaylist::binPlaylistId) {
            QDomElement mainBinPlaylist = playlists.at(i).toElement();

            // ensure the documentid is valid
            m_documentid = Xml::getXmlProperty(mainBinPlaylist, QStringLiteral("kdenlive:docproperties.documentid"));
            if (m_documentid.isEmpty()) {
                // invalid document id, recreate one
                m_documentid = QString::number(QDateTime::currentMSecsSinceEpoch());
                Xml::setXmlProperty(mainBinPlaylist, QStringLiteral("kdenlive:docproperties.documentid"), m_documentid);
                m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
                m_warnings.append(i18n("The document id of your project was invalid, a new one has been created."));
            }

            // ensure the storage for temp files exists
            storageFolder = Xml::getXmlProperty(mainBinPlaylist, QStringLiteral("kdenlive:docproperties.storagefolder"));
            storageFolder = ensureAbsoultePath(storageFolder);

            if (!storageFolder.isEmpty() && !QFile::exists(storageFolder) && projectDir.exists(m_documentid)) {
                storageFolder = projectDir.absolutePath();
                Xml::setXmlProperty(mainBinPlaylist, QStringLiteral("kdenlive:docproperties.storagefolder"), projectDir.absoluteFilePath(m_documentid));
                m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
            }

            // get bin ids
            m_binEntries = mainBinPlaylist.elementsByTagName(QLatin1String("entry"));
            for (int i = 0; i < m_binEntries.count(); ++i) {
                QDomElement e = m_binEntries.item(i).toElement();
                m_binIds << e.attribute(QStringLiteral("producer"));
            }
            break;
        }
    }

    QDomNodeList documentProducers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList documentChains = m_doc.elementsByTagName(QStringLiteral("chain"));
    QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));

    m_safeImages.clear();
    m_safeFonts.clear();

    QStringList verifiedPaths;
    int max = documentProducers.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentProducers.item(i).toElement();
        verifiedPaths << getMissingProducers(e, entries, verifiedPaths, /*m_missingPaths,*/ storageFolder);
    }
    max = documentChains.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentChains.item(i).toElement();
        verifiedPaths << getMissingProducers(e, entries, verifiedPaths, /*m_missingPaths,*/ storageFolder);
    }

    // Check existence of luma files
    QStringList filesToCheck = getAssetsFiles(m_doc, QStringLiteral("transition"), getLumaPairs());
    for (const QString &lumafile : qAsConst(filesToCheck)) {
        QString filePath = ensureAbsoultePath(lumafile);

        if (QFile::exists(filePath)) {
            // everything is fine, we can stop here
            continue;
        }

        QString lumaName = QFileInfo(filePath).fileName();
        // MLT 7 now generates lumas on the fly, so don't detect these as missing
        if (isMltBuildInLuma(lumaName)) {
            // everything is fine, we can stop here
            continue;
        }

        // check if this was an old format luma, not in correct folder
        QString fixedLuma = filePath.section(QLatin1Char('/'), 0, -2);
        lumaName.prepend(isProfileHD(m_doc) ? QStringLiteral("/HD/") : QStringLiteral("/PAL/"));
        fixedLuma.append(lumaName);

        if (!QFile::exists(fixedLuma)) {
            // Check Kdenlive folder
            fixedLuma = fixLumaPath(filePath);
        }

        if (!QFile::exists(fixedLuma)) {
            // Try to change file extension
            if (filePath.endsWith(QLatin1String(".pgm"))) {
                fixedLuma = filePath.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".png");
            } else if (filePath.endsWith(QLatin1String(".png"))) {
                fixedLuma = filePath.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".pgm");
            }
        }

        DocumentResource item;
        item.type = MissingType::Luma;
        item.originalFilePath = filePath;

        if (QFile::exists(fixedLuma)) {
            item.newFilePath = fixedLuma;
            item.status = MissingStatus::Fixed;
        } else {
            // we have not been able to fix or find the file
            item.status = MissingStatus::Missing;
        }

        if (!itemsContain(item.type, item.originalFilePath, item.status)) {
            m_items.push_back(item);
        }
    }

    // Check for missing transitions (eg. not installed)
    QStringList transtions = getAssetsServiceIds(m_doc, QStringLiteral("transition"));
    for (const QString &id : qAsConst(transtions)) {
        if (!TransitionsRepository::get()->exists(id) && !itemsContain(MissingType::Transition, id, MissingStatus::Remove)) {
            DocumentResource item;
            item.type = MissingType::Transition;
            item.status = MissingStatus::Remove;
            item.originalFilePath = id;
            m_items.push_back(item);
        }
    }

    // Check for missing filter assets
    QStringList assetsToCheck = getAssetsFiles(m_doc, QStringLiteral("filter"), getAssetPairs());
    for (const QString &filterfile : qAsConst(assetsToCheck)) {
        QString filePath = ensureAbsoultePath(filterfile);

        if (!QFile::exists(filePath) && !itemsContain(MissingType::AssetFile, filePath)) {
            DocumentResource item;
            item.type = MissingType::AssetFile;
            item.originalFilePath = filePath;
            item.status = MissingStatus::Missing;
            m_items.push_back(item);
        }
    }

    // Check for missing effects (eg. not installed)
    QStringList filters = getAssetsServiceIds(m_doc, QStringLiteral("filter"));
    for (const QString &id : qAsConst(filters)) {
        if (!EffectsRepository::get()->exists(id) && !itemsContain(MissingType::Effect, id, MissingStatus::Remove)) {
            // m_missingFilters << id;
            DocumentResource item;
            item.type = MissingType::Effect;
            item.status = MissingStatus::Remove;
            item.originalFilePath = id;
            m_items.push_back(item);
        }
    }

    if (m_items.size() == 0) {
        return false;
    }
    return true;
}

DocumentChecker::~DocumentChecker() {}

const QString DocumentChecker::relocateResource(QString sourceResource)
{
    if (m_rootReplacement.first.isEmpty()) {
        return QString();
    }

    if (sourceResource.startsWith(m_rootReplacement.first)) {
        sourceResource.replace(m_rootReplacement.first, m_rootReplacement.second);
        // Use QFileInfo to ensure we also handle directories (for slideshows)
        if (QFileInfo::exists(sourceResource)) {
            return sourceResource;
        }
        return QString();
    }
    // Check if we have a common root, if file has a common ancestor in its path
    QStringList replacedRoot = m_rootReplacement.second.split(QLatin1Char('/'));
    QStringList cutRoot = m_rootReplacement.first.split(QLatin1Char('/'));
    QStringList cutResource = sourceResource.split(QLatin1Char('/'));
    // Find common ancestor
    int ix = 0;
    for (auto &cut : cutRoot) {
        if (!cutResource.isEmpty()) {
            if (cutResource.first() != cut) {
                break;
            }
        } else {
            break;
        }
        cutResource.takeFirst();
        ix++;
    }
    int diff = cutRoot.size() - ix;
    if (diff < replacedRoot.size()) {
        while (diff > 0) {
            replacedRoot.removeLast();
            diff--;
        }
    }
    QString basePath = replacedRoot.join(QLatin1Char('/'));
    basePath.append(QLatin1Char('/'));
    basePath.append(cutResource.join(QLatin1Char('/')));
    qDebug() << "/// RESULTING PATH: " << basePath;
    // Use QFileInfo to ensure we also handle directories (for slideshows)
    if (QFileInfo::exists(basePath)) {
        return basePath;
    }
    return QString();
}

bool DocumentChecker::ensureProducerHasId(QDomElement &producer, const QDomNodeList &entries)
{
    if (!Xml::getXmlProperty(producer, QStringLiteral("kdenlive:id")).isEmpty()) {
        // id is there, everything is fine
        return false;
    }

    // This should not happen, try to recover the producer id
    int max = entries.count();
    QString producerName = producer.attribute(QStringLiteral("id"));
    for (int j = 0; j < max; j++) {
        QDomElement e = entries.item(j).toElement();
        if (e.attribute(QStringLiteral("producer")) == producerName) {
            // Match found
            QString entryName = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
            if (!entryName.isEmpty()) {
                Xml::setXmlProperty(producer, QStringLiteral("kdenlive:id"), entryName);
                return true;
            }
        }
    }
    return false;
}

bool DocumentChecker::ensureProducerIsNotPlaceholder(QDomElement &producer)
{
    QString text = Xml::getXmlProperty(producer, QStringLiteral("text"));
    QString service = Xml::getXmlProperty(producer, QStringLiteral("mlt_service"));

    // Check if this is an invalid clip (project saved with missing source)
    if (service != QLatin1String("qtext") || text != QLatin1String("INVALID")) {
        // This does not seem to be a placeholder for an invalid clip
        return false;
    }

    // Clip saved with missing source: check if source clip is now available
    QString resource = Xml::getXmlProperty(producer, QStringLiteral("warp_resource"));
    if (resource.isEmpty()) {
        resource = Xml::getXmlProperty(producer, QStringLiteral("resource"));
    }
    resource = ensureAbsoultePath(resource);

    if (!QFile::exists(resource)) {
        // The source clip is still not available
        return false;
    }

    // Reset to original service
    Xml::removeXmlProperty(producer, QStringLiteral("text"));
    QString original_service = Xml::getXmlProperty(producer, QStringLiteral("kdenlive:orig_service"));
    if (!original_service.isEmpty()) {
        Xml::setXmlProperty(producer, QStringLiteral("mlt_service"), original_service);
        // We know the original service and recovered it, everything is fine again
        return true;
    }

    // Try to guess service as we do not know it
    QString guessedService;
    if (Xml::hasXmlProperty(producer, QStringLiteral("ttl"))) {
        guessedService = QStringLiteral("qimage");
    } else if (resource.endsWith(QLatin1String(".kdenlivetitle"))) {
        guessedService = QStringLiteral("kdenlivetitle");
    } else if (resource.endsWith(QLatin1String(".kdenlive")) || resource.endsWith(QLatin1String(".mlt"))) {
        guessedService = QStringLiteral("xml");
    } else {
        guessedService = QStringLiteral("avformat");
    }
    Xml::setXmlProperty(producer, QStringLiteral("mlt_service"), guessedService);
    return true;
}

/*void DocumentChecker::setReloadProxy(QDomElement &producer, const QString &realPath)
{
    // Tell Kdenlive to recreate proxy
    producer.setAttribute(QStringLiteral("_replaceproxy"), QStringLiteral("1"));
    // Remove reference to missing proxy
    Xml::setXmlProperty(producer, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));

    // Replace proxy url with real clip in MLT producers
    QString prefix;
    QString originalService = Xml::getXmlProperty(producer, QStringLiteral("kdenlive:original.mlt_service"));
    QString service = Xml::getXmlProperty(producer, QStringLiteral("mlt_service"));
    if (service == QLatin1String("timewarp")) {
        prefix = Xml::getXmlProperty(producer, QStringLiteral("warp_speed"));
        prefix.append(QLatin1Char(':'));
        Xml::setXmlProperty(producer, QStringLiteral("warp_resource"), prefix + realPath);
    } else if (!originalService.isEmpty()) {
        Xml::setXmlProperty(producer, QStringLiteral("mlt_service"), originalService);
    }
    prefix.append(realPath);
    Xml::setXmlProperty(producer, QStringLiteral("resource"), prefix);
}*/

void DocumentChecker::removeProxy(const QDomNodeList &items, const QString &clipId, bool recreate)
{
    QDomElement e;
    for (int i = 0; i < items.count(); ++i) {
        e = items.item(i).toElement();
        QString parentId = getKdenliveClipId(e);
        if (parentId != clipId) {
            continue;
        }
        // Tell Kdenlive to recreate proxy
        if (recreate) {
            e.setAttribute(QStringLiteral("_replaceproxy"), QStringLiteral("1"));
        }
        // Remove reference to missing proxy
        Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));

        // Replace proxy url with real clip in MLT producers
        QString prefix;
        QString originalService = Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.mlt_service"));
        QString originalPath = Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.resource"));
        QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        if (service == QLatin1String("timewarp")) {
            prefix = Xml::getXmlProperty(e, QStringLiteral("warp_speed"));
            prefix.append(QLatin1Char(':'));
            Xml::setXmlProperty(e, QStringLiteral("warp_resource"), prefix + originalPath);
        } else if (!originalService.isEmpty()) {
            Xml::setXmlProperty(e, QStringLiteral("mlt_service"), originalService);
        }
        prefix.append(originalPath);
        Xml::setXmlProperty(e, QStringLiteral("resource"), prefix);
    }
}

void DocumentChecker::checkMissingImagesAndFonts(const QStringList &images, const QStringList &fonts, const QString &id)
{
    for (const QString &img : images) {
        if (m_safeImages.contains(img)) {
            continue;
        }
        if (!QFile::exists(img)) {
            DocumentResource item;
            item.type = MissingType::TitleImage;
            item.status = MissingStatus::Missing;
            item.originalFilePath = img;
            item.clipId = id;
            m_items.push_back(item);

            const QString relocated = relocateResource(img);
            if (!relocated.isEmpty()) {
                item.status = MissingStatus::Fixed;
                item.newFilePath = relocated;
            }
        } else {
            m_safeImages.append(img);
        }
    }
    for (const QString &fontelement : fonts) {
        if (m_safeFonts.contains(fontelement) || itemsContain(MissingType::TitleFont, fontelement)) {
            continue;
        }
        QFont f(fontelement);
        if (fontelement != QFontInfo(f).family()) {
            DocumentResource item;
            item.type = MissingType::TitleFont;
            item.originalFilePath = fontelement;
            item.status = MissingStatus::Missing;
            m_items.push_back(item);
        } else {
            m_safeFonts.append(fontelement);
        }
    }
}

QString DocumentChecker::getMissingProducers(QDomElement &e, const QDomNodeList &entries, const QStringList &verifiedPaths, const QString &storageFolder)
{
    QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
    QStringList serviceToCheck = {QStringLiteral("kdenlivetitle"), QStringLiteral("qimage"), QStringLiteral("pixbuf"), QStringLiteral("timewarp"),
                                  QStringLiteral("framebuffer"),   QStringLiteral("xml"),    QStringLiteral("qtext"),  QStringLiteral("tractor")};
    if (!service.startsWith(QLatin1String("avformat")) && !serviceToCheck.contains(service)) {
        return QString();
    }

    ensureProducerHasId(e, entries);

    if (ensureProducerIsNotPlaceholder(e)) {
        return QString();
    }

    bool isBinClip = m_binIds.contains(e.attribute(QLatin1String("id")));

    if (service == QLatin1String("qtext")) {
        checkMissingImagesAndFonts(QStringList(), QStringList(Xml::getXmlProperty(e, QStringLiteral("family"))), e.attribute(QStringLiteral("id")));
        return QString();
    } else if (service == QLatin1String("kdenlivetitle")) {
        // TODO: Check if clip template is missing (xmltemplate) or hash changed
        QString xml = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
        QStringList images = TitleWidget::extractImageList(xml);
        QStringList fonts = TitleWidget::extractFontList(xml);
        checkMissingImagesAndFonts(images, fonts, Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")));
        return QString();
    }

    QString clipId = getKdenliveClipId(e);
    QString resource = getProducerResource(e);
    ClipType::ProducerType clipType = getClipType(service, resource);

    int index = itemIndexByClipId(clipId);
    if (index > -1) {
        if (m_items[index].hash.isEmpty()) {
            m_items[index].hash = Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash"));
            m_items[index].fileSize = Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size"));
        }
    }

    auto checkClip = [this, clipId, clipType](QDomElement &e, const QString &resource) {
        if (isSequenceWithSpeedEffect(e)) {
            // This is a missing timeline sequence clip with speed effect, trigger recreate on opening
            Xml::setXmlProperty(e, QStringLiteral("_rebuild"), QStringLiteral("1"));
            // missingPaths.append(resource);
        } else {
            DocumentResource item;
            item.status = MissingStatus::Missing;
            item.clipId = clipId;
            item.clipType = clipType;
            item.originalFilePath = resource;
            item.type = MissingType::Clip;
            item.hash = Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash"));
            item.fileSize = Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size"));

            QString relocated;
            if (clipType == ClipType::SlideShow) {
                // Strip filename
                relocated = QFileInfo(resource).absolutePath();
            } else {
                relocated = resource;
            }
            relocated = relocateResource(relocated);
            if (!relocated.isEmpty()) {
                if (clipType == ClipType::SlideShow) {
                    item.newFilePath = QDir(relocated).absoluteFilePath(QFileInfo(resource).fileName());
                } else {
                    item.newFilePath = relocated;
                }
                item.newFilePath = relocated;
                item.status = MissingStatus::Fixed;
            }
            m_items.push_back(item);
        }
    };

    if (verifiedPaths.contains(resource)) {
        // Don't check same url twice (for example track producers)
        return QString();
    }
    QString producerResource = resource;
    QString proxy = Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy"));
    // TODO: should this only apply to bin clips (isBinClip)
    if (!proxy.isEmpty()) {
        bool proxyFound = true;
        proxy = ensureAbsoultePath(proxy);
        if (!QFile::exists(proxy)) {
            // Missing clip found
            // Check if proxy exists in current storage folder
            bool fixed = false;
            if (!storageFolder.isEmpty()) {
                QDir dir(storageFolder + QStringLiteral("/proxy/"));
                if (dir.exists(QFileInfo(proxy).fileName())) {
                    QString updatedPath = dir.absoluteFilePath(QFileInfo(proxy).fileName());
                    DocumentResource item;
                    item.clipId = clipId;
                    item.clipType = clipType;
                    item.status = MissingStatus::Fixed;
                    item.type = MissingType::Proxy;
                    item.originalFilePath = proxy;
                    item.newFilePath = updatedPath;
                    m_items.push_back(item);

                    fixed = true;
                }
            }
            if (!fixed) {
                proxyFound = false;
            }
        }
        QString original = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
        original = ensureAbsoultePath(original);

        // Check for slideshows
        bool slideshow = isSlideshow(original);
        if (slideshow && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
            original = QFileInfo(original).absolutePath();
        }
        DocumentResource item;
        item.clipId = clipId;
        item.clipType = clipType;
        item.status = MissingStatus::Missing;
        if (!QFile::exists(original)) {
            bool resourceFixed = false;
            QString movedOriginal = relocateResource(original);
            if (!movedOriginal.isEmpty()) {
                if (slideshow) {
                    movedOriginal = QDir(movedOriginal).absoluteFilePath(QFileInfo(original).fileName());
                }
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:originalurl"), movedOriginal);
                if (!QFile::exists(producerResource)) {
                    Xml::setXmlProperty(e, QStringLiteral("resource"), movedOriginal);
                }
                resourceFixed = true;
                if (proxyFound) {
                    return QString();
                }
            }

            if (!proxyFound) {
                item.originalFilePath = proxy;
                item.type = MissingType::Proxy;

                if (!resourceFixed) {
                    // Neither proxy nor original file found
                    checkClip(e, original);
                }
            } else {
                // clip has proxy but original clip is missing
                item.originalFilePath = original;
                item.type = MissingType::Clip;
                item.status = MissingStatus::MissingButProxy;
                // e.setAttribute(QStringLiteral("_missingsource"), QStringLiteral("1"));
                item.hash = Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash"));
                item.fileSize = Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size"));
                // item.mltService = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
            }
            m_items.push_back(item);
        } else if (!proxyFound) {
            item.originalFilePath = proxy;
            item.type = MissingType::Proxy;
            m_items.push_back(item);
        }
        return resource;
    }

    // Check for slideshows
    QString slidePattern;
    bool slideshow = isSlideshow(resource);
    if (slideshow) {
        if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
            slidePattern = QFileInfo(resource).fileName();
            resource = QFileInfo(resource).absolutePath();
        } else if ((service.startsWith(QLatin1String("avformat")) || service == QLatin1String("timewarp")) && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
            // Fix MLT 6.20 avformat slideshows
            if (service.startsWith(QLatin1String("avformat"))) {
                Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("qimage"));
            }
            slidePattern = QFileInfo(resource).fileName();
            resource = QFileInfo(resource).absolutePath();
        } else {
            slideshow = false;
        }
    }
    if (!QFile::exists(resource)) {
        if (service == QLatin1String("timewarp") && proxy == QLatin1String("-")) {
            // In some corrupted cases, clips with speed effect kept a reference to proxy clip in warp_resource
            QString original = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
            original = ensureAbsoultePath(original);
            if (original != resource && QFile::exists(original)) {
                // Fix timewarp producer
                Xml::setXmlProperty(e, QStringLiteral("warp_resource"), original);
                Xml::setXmlProperty(e, QStringLiteral("resource"), Xml::getXmlProperty(e, QStringLiteral("warp_speed")) + QStringLiteral(":") + original);
                return original;
            }
        }
        bool isPreviewChunk = QFileInfo(resource).absolutePath().endsWith(QString("/%1/preview").arg(m_documentid));
        // Missing clip found, make sure to omit timeline preview
        if (!isPreviewChunk) {
            checkClip(e, resource);
        }
    } else if (isBinClip &&
               (service.startsWith(QLatin1String("avformat")) || slideshow || service == QLatin1String("qimage") || service == QLatin1String("pixbuf"))) {
        // Check if file changed
        const QByteArray hash = Xml::getXmlProperty(e, "kdenlive:file_hash").toLatin1();
        if (!hash.isEmpty()) {
            const QByteArray fileData =
                slideshow ? ProjectClip::getFolderHash(QDir(resource), slidePattern).toHex() : ProjectClip::calculateHash(resource).first.toHex();
            if (hash != fileData) {
                if (slideshow) {
                    // For slideshow clips, silently upgrade hash
                    Xml::setXmlProperty(e, "kdenlive:file_hash", fileData);
                } else {
                    // Clip was changed, notify and trigger clip reload
                    Xml::removeXmlProperty(e, "kdenlive:file_hash");
                    DocumentResource item;
                    item.originalFilePath = resource;
                    item.clipId = clipId;
                    item.clipType = clipType;
                    item.type = MissingType::Clip;
                    item.status = MissingStatus::Reload;
                    m_items.push_back(item);
                }
            }
        }
    }
    // Make sure we don't query same path twice
    return producerResource;
}

QString DocumentChecker::fixLumaPath(const QString &file)
{
    QDir searchPath(KdenliveSettings::mltpath());
    QString fname = QFileInfo(file).fileName();
    if (file.contains(QStringLiteral("PAL"))) {
        searchPath.cd(QStringLiteral("../lumas/PAL"));
    } else {
        searchPath.cd(QStringLiteral("../lumas/NTSC"));
    }
    QFileInfo result(searchPath, fname);
    if (result.exists()) {
        return result.filePath();
    }
    // try to find luma in application path
    searchPath.setPath(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
    searchPath.cd(QStringLiteral("data/"));
#else
    searchPath.cd(QStringLiteral("../share/kdenlive/"));
#endif
    if (file.contains(QStringLiteral("/PAL"))) {
        searchPath.cd(QStringLiteral("lumas/PAL/"));
    } else {
        searchPath.cd(QStringLiteral("lumas/HD/"));
    }
    result.setFile(searchPath, fname);
    if (result.exists()) {
        return result.filePath();
    }
    // Try in Kdenlive's standard KDE path
    QString res = QStandardPaths::locate(QStandardPaths::AppDataLocation, "lumas", QStandardPaths::LocateDirectory);
    if (!res.isEmpty()) {
        searchPath.setPath(res);
        if (file.contains(QStringLiteral("/PAL"))) {
            searchPath.cd(QStringLiteral("PAL"));
        } else {
            searchPath.cd(QStringLiteral("HD"));
        }
        result.setFile(searchPath, fname);
        if (result.exists()) {
            return result.filePath();
        }
    }
    return QString();
}

QString DocumentChecker::searchLuma(const QDir &dir, const QString &file)
{
    // Try in user's chosen folder
    QString result = fixLumaPath(file);
    return result.isEmpty() ? searchPathRecursively(dir, QFileInfo(file).fileName()) : result;
}

QString DocumentChecker::searchPathRecursively(const QDir &dir, const QString &fileName, ClipType::ProducerType type)
{
    QString foundFileName;
    bool patternSlideshow = true;
    QDir searchDir(dir);
    QStringList filesAndDirs;
    qApp->processEvents();
    /*if (m_abortSearch) {
        return QString();
    }*/
    if (type == ClipType::SlideShow) {
        if (fileName.contains(QLatin1Char('%'))) {
            searchDir.setNameFilters({fileName.section(QLatin1Char('%'), 0, -2) + QLatin1Char('*')});
            filesAndDirs = searchDir.entryList(QDir::Files | QDir::Readable);

        } else {
            patternSlideshow = false;
            QString slideDirName = QFileInfo(fileName).dir().dirName();
            searchDir.setNameFilters({slideDirName});
            filesAndDirs = searchDir.entryList(QDir::Dirs | QDir::Readable);
        }
    } else {
        searchDir.setNameFilters({fileName});
        filesAndDirs = searchDir.entryList(QDir::Files | QDir::Readable);
    }
    if (!filesAndDirs.isEmpty()) {
        // File Found
        if (type == ClipType::SlideShow) {
            if (patternSlideshow) {
                return searchDir.absoluteFilePath(fileName);
            } else {
                // mime type slideshow
                searchDir.cd(filesAndDirs.first());
                return searchDir.absoluteFilePath(QFileInfo(fileName).fileName());
            }
        } else {
            return searchDir.absoluteFilePath(filesAndDirs.first());
        }
    }
    searchDir.setNameFilters(QStringList());
    filesAndDirs = searchDir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
        foundFileName = searchPathRecursively(searchDir.absoluteFilePath(filesAndDirs.at(i)), fileName, type);
        if (!foundFileName.isEmpty()) {
            break;
        }
    }
    return foundFileName;
}

QString DocumentChecker::searchDirRecursively(const QDir &dir, const QString &matchHash, const QString &fullName)
{
    qApp->processEvents();
    /*if (m_abortSearch) {
        return QString();
    }*/
    // Q_EMIT showScanning(i18n("Scanning %1", dir.absolutePath()));
    QString fileName = QFileInfo(fullName).fileName();
    // Check main dir
    QString fileHash = ProjectClip::getFolderHash(dir, fileName).toHex();
    if (fileHash == matchHash) {
        return dir.absoluteFilePath(fileName);
    }
    // Search subfolders
    const QStringList subDirs = dir.entryList(QDir::AllDirs | QDir::NoDot | QDir::NoDotDot);
    for (const QString &sub : subDirs) {
        QDir subFolder(dir.absoluteFilePath(sub));
        fileHash = ProjectClip::getFolderHash(subFolder, fileName).toHex();
        if (fileHash == matchHash) {
            return subFolder.absoluteFilePath(fileName);
        }
    }
    /*if (m_abortSearch) {
        return QString();
    }*/
    // Search inside subfolders
    for (const QString &sub : subDirs) {
        QDir subFolder(dir.absoluteFilePath(sub));
        const QStringList subSubDirs = subFolder.entryList(QDir::AllDirs | QDir::NoDot | QDir::NoDotDot);
        for (const QString &subsub : subSubDirs) {
            QDir subDir(subFolder.absoluteFilePath(subsub));
            QString result = searchDirRecursively(subDir, matchHash, fullName);
            if (!result.isEmpty()) {
                return result;
            }
        }
    }
    return QString();
}

QString DocumentChecker::searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash, const QString &fileName)
{
    if (matchSize.isEmpty() && matchHash.isEmpty()) {
        return searchPathRecursively(dir, QUrl::fromLocalFile(fileName).fileName());
    }
    QString foundFileName;
    QByteArray fileData;
    QByteArray fileHash;
    QStringList filesAndDirs = dir.entryList(QDir::Files | QDir::Readable);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
        qApp->processEvents();
        /*if (m_abortSearch) {
            return QString();
        }*/
        QFile file(dir.absoluteFilePath(filesAndDirs.at(i)));
        if (QString::number(file.size()) == matchSize) {
            if (file.open(QIODevice::ReadOnly)) {
                /*
                 * 1 MB = 1 second per 450 files (or faster)
                 * 10 MB = 9 seconds per 450 files (or faster)
                 */
                if (file.size() > 1000000 * 2) {
                    fileData = file.read(1000000);
                    if (file.seek(file.size() - 1000000)) {
                        fileData.append(file.readAll());
                    }
                } else {
                    fileData = file.readAll();
                }
                file.close();
                fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
                if (QString::fromLatin1(fileHash.toHex()) == matchHash) {
                    return file.fileName();
                }
            }
        }
        ////qCDebug(KDENLIVE_LOG) << filesAndDirs.at(i) << file.size() << fileHash.toHex();
    }
    filesAndDirs = dir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
        foundFileName = searchFileRecursively(dir.absoluteFilePath(filesAndDirs.at(i)), matchSize, matchHash, fileName);
        if (!foundFileName.isEmpty()) {
            break;
        }
    }
    return foundFileName;
}

QString DocumentChecker::ensureAbsoultePath(QString filepath)
{
    if (!filepath.isEmpty() && QFileInfo(filepath).isRelative()) {
        filepath.prepend(m_root);
    }
    return filepath;
}

QStringList DocumentChecker::getAssetsFiles(const QDomDocument &doc, const QString &tagName, const QMap<QString, QString> &searchPairs)
{
    QStringList files;

    QDomNodeList assets = doc.elementsByTagName(tagName);
    int max = assets.count();
    for (int i = 0; i < max; ++i) {
        QDomElement asset = assets.at(i).toElement();
        QString service = Xml::getXmlProperty(asset, QStringLiteral("kdenlive_id"));
        if (service.isEmpty()) {
            service = Xml::getXmlProperty(asset, QStringLiteral("mlt_service"));
        }
        if (searchPairs.contains(service)) {
            const QString filepath = Xml::getXmlProperty(asset, searchPairs.value(service));
            if (!filepath.isEmpty()) {
                files << filepath;
            }
        }
    }
    files.removeDuplicates();
    return files;
}

QStringList DocumentChecker::getAssetsServiceIds(const QDomDocument &doc, const QString &tagName)
{
    QDomNodeList filters = doc.elementsByTagName(tagName);
    int max = filters.count();
    QStringList services;
    for (int i = 0; i < max; ++i) {
        QDomElement filter = filters.at(i).toElement();
        QString service = Xml::getXmlProperty(filter, QStringLiteral("kdenlive_id"));
        if (service.isEmpty()) {
            service = Xml::getXmlProperty(filter, QStringLiteral("mlt_service"));
        }
        services << service;
    }
    services.removeDuplicates();
    return services;
}

bool DocumentChecker::isMltBuildInLuma(const QString &lumaName)
{
    // Since version 7 MLT contains build-in lumas named luma01.pgm to luma22.pgm
    static const QRegularExpression regex(QRegularExpression::anchoredPattern(R"(luma([0-9]{2})\.pgm)"));
    QRegularExpressionMatch match = regex.match(lumaName);
    if (match.hasMatch() && match.captured(1).toInt() > 0 && match.captured(1).toInt() < 23) {
        return true;
    }
    return false;
}

// TODO remove?
void DocumentChecker::fixMissingSource(const QString &id, const QDomNodeList &producers, const QDomNodeList &chains)
{
    QDomElement e;
    for (int i = 0; i < producers.count(); ++i) {
        e = producers.item(i).toElement();
        QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (parentId == id) {
            // Fix clip
            e.removeAttribute(QStringLiteral("_missingsource"));
        }
    }
    for (int i = 0; i < chains.count(); ++i) {
        e = chains.item(i).toElement();
        QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (parentId == id) {
            // Fix clip
            e.removeAttribute(QStringLiteral("_missingsource"));
        }
    }
}

QStringList DocumentChecker::fixSequences(QDomElement &e, const QDomNodeList &producers, const QStringList &tractorIds)
{
    QStringList fixedSequences;
    QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
    bool isBinClip = m_binIds.contains(e.attribute(QLatin1String("id")));
    QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));

    if (!(isBinClip && service == QLatin1String("tractor") && resource.endsWith(QLatin1String("tractor>")))) {
        // This is not a broken sequence clip (bug in Kdenlive 23.04.0)
        // nothing to fix, quit
        return {};
    }

    const QString brokenId = e.attribute(QStringLiteral("id"));
    const QString brokenUuid = Xml::getXmlProperty(e, QStringLiteral("kdenlive:uuid"));
    // Check that we have the original clip somewhere in the producers list
    if (brokenId != brokenUuid && tractorIds.contains(brokenUuid)) {
        // Replace bin clip entry
        for (int i = 0; i < m_binEntries.count(); ++i) {
            QDomElement e = m_binEntries.item(i).toElement();
            if (e.attribute(QStringLiteral("producer")) == brokenId) {
                // Match
                e.setAttribute(QStringLiteral("producer"), brokenUuid);
                fixedSequences.append(brokenId);
                return fixedSequences;
            }
        }
    } else {
        // entry not found, this is a more complex recovery:
        // 1. Change tag to tractor
        // 2. Reinsert all tractor as tracks
        // 3. Move the node just before main_bin to ensure its children tracks are defined before it
        //    e.setTagName(QStringLiteral("tractor"));
        // 4. Xml::removeXmlProperty(e, QStringLiteral("resource"));

        if (!e.elementsByTagName(QStringLiteral("track")).isEmpty()) {
            return {};
        }

        // Change tag, add tracks and move to the end of the document (just before the main_bin)
        QDomNodeList tracks = m_doc.elementsByTagName(QStringLiteral("track"));

        QStringList insertedTractors;
        for (int k = 0; k < tracks.count(); ++k) {
            // Collect names of already inserted tractors / playlists
            QDomElement prod = tracks.item(k).toElement();
            insertedTractors << prod.attribute(QStringLiteral("producer"));
        }
        // Tracks must be inserted before transitions / filters
        QDomNode lastProperty = e.lastChildElement(QStringLiteral("property"));
        if (lastProperty.isNull()) {
            lastProperty = e.firstChildElement();
        }
        // Find black producer id
        for (int k = 0; k < producers.count(); ++k) {
            QDomElement prod = producers.item(k).toElement();
            if (Xml::hasXmlProperty(prod, QStringLiteral("kdenlive:playlistid"))) {
                // Match, we found black track producer
                QDomElement tk = m_doc.createElement(QStringLiteral("track"));
                tk.setAttribute(QStringLiteral("producer"), prod.attribute(QStringLiteral("id")));
                lastProperty = e.insertAfter(tk, lastProperty);
                break;
            }
        }
        // Insert real tracks
        QDomNodeList tractors = m_doc.elementsByTagName(QStringLiteral("tractor"));
        for (int j = 0; j < tractors.count(); ++j) {
            QDomElement current = tractors.item(j).toElement();
            // Check all non used tractors and attach them as tracks
            if (!Xml::hasXmlProperty(current, QStringLiteral("kdenlive:projectTractor")) && !insertedTractors.contains(current.attribute("id"))) {
                QDomElement tk = m_doc.createElement(QStringLiteral("track"));
                tk.setAttribute(QStringLiteral("producer"), current.attribute(QStringLiteral("id")));
                lastProperty = e.insertAfter(tk, lastProperty);
            }
        }

        QDomNode brokenSequence = m_doc.documentElement().removeChild(e);
        QDomElement fixedSequence = brokenSequence.toElement();
        fixedSequence.setTagName(QStringLiteral("tractor"));
        Xml::removeXmlProperty(fixedSequence, QStringLiteral("resource"));
        Xml::removeXmlProperty(fixedSequence, QStringLiteral("mlt_service"));

        QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
        for (int p = 0; p < playlists.count(); ++p) {
            if (playlists.at(p).toElement().attribute(QStringLiteral("id")) == BinPlaylist::binPlaylistId) {
                QDomNode mainBinPlaylist = playlists.at(p);
                m_doc.documentElement().insertBefore(brokenSequence, mainBinPlaylist);
            }
        }

        fixedSequences.append(brokenId);
        return fixedSequences;
    }
    return fixedSequences;
}

void DocumentChecker::fixProxyClip(const QDomNodeList &items, const QString &id, const QString &oldUrl, const QString &newUrl)
{
    QDomElement e;
    for (int i = 0; i < items.count(); ++i) {
        e = items.item(i).toElement();
        QString parentId = getKdenliveClipId(e);
        if (parentId != id) {
            continue;
        }
        // Fix clip
        QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
        bool timewarp = false;
        if (Xml::getXmlProperty(e, QStringLiteral("mlt_service")) == QLatin1String("timewarp")) {
            timewarp = true;
            resource = Xml::getXmlProperty(e, QStringLiteral("warp_resource"));
        }
        if (resource == oldUrl) {
            if (timewarp) {
                Xml::setXmlProperty(e, QStringLiteral("resource"), Xml::getXmlProperty(e, QStringLiteral("warp_speed")) + ":" + newUrl);
                Xml::setXmlProperty(e, QStringLiteral("warp_resource"), newUrl);
            } else {
                Xml::setXmlProperty(e, QStringLiteral("resource"), newUrl);
            }
        }
        if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy")).isEmpty()) {
            // Only set originalurl on master producer
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), newUrl);
        }
    }
}

void DocumentChecker::fixTitleImage(QDomElement &e, const QString &oldPath, const QString &newPath)
{
    QDomNodeList properties = e.childNodes();
    QDomElement property;
    for (int j = 0; j < properties.count(); ++j) {
        property = properties.item(j).toElement();
        if (property.attribute(QStringLiteral("name")) == QLatin1String("xmldata")) {
            QString xml = property.firstChild().nodeValue();
            xml.replace(oldPath, newPath);
            property.firstChild().setNodeValue(xml);
            break;
        }
    }
}

void DocumentChecker::fixTitleFont(const QDomNodeList &producers, const QString &oldFont, const QString &newFont)
{
    QDomElement e;
    for (int i = 0; i < producers.count(); ++i) {
        e = producers.item(i).toElement();
        QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        // Fix clip
        if (service == QLatin1String("kdenlivetitle")) {
            QString xml = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(xml);
            if (fonts.contains(oldFont)) {
                xml.replace(QString("font=\"%1\"").arg(oldFont), QString("font=\"%1\"").arg(newFont));
                Xml::setXmlProperty(e, QStringLiteral("xmldata"), xml);
                Xml::setXmlProperty(e, QStringLiteral("force_reload"), QStringLiteral("2"));
                Xml::setXmlProperty(e, QStringLiteral("_fullreload"), QStringLiteral("2"));
            }
        }
    }
}

void DocumentChecker::fixAssetResource(const QDomNodeList &assets, const QMap<QString, QString> &searchPairs, const QString &oldPath, const QString &newPath)
{
    for (int i = 0; i < assets.count(); ++i) {
        QDomElement asset = assets.at(i).toElement();

        QString service = Xml::getXmlProperty(asset, QStringLiteral("mlt_service"));
        if (searchPairs.contains(service)) {
            QString currentPath = Xml::getXmlProperty(asset, searchPairs.value(service));
            if (!currentPath.isEmpty() && ensureAbsoultePath(currentPath) == oldPath) {
                Xml::setXmlProperty(asset, searchPairs.value(service), newPath);
            }
        }
    }
}

void DocumentChecker::usePlaceholderForClip(const QDomNodeList &items, const QString &clipId)
{
    // items: chains or producers

    QDomElement e;
    for (int i = items.count() - 1; i >= 0; --i) {
        // Setting the tag name (see below) might change it and this will remove the item from the original list, so we need to parse in reverse order
        e = items.item(i).toElement();
        if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == clipId) {
            // Fix clip
            Xml::setXmlProperty(e, QStringLiteral("_placeholder"), QStringLiteral("1"));
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:orig_service"), Xml::getXmlProperty(e, QStringLiteral("mlt_service")));

            // In MLT 7.14/15, link_swresample crashes on invalid avformat clips,
            // so switch to producer instead of chain to use filter_swresample.
            // If we have an producer already, it obviously makes no difference.
            e.setTagName(QStringLiteral("producer"));
        }
    }
}

void DocumentChecker::removeAssetsById(QDomDocument &doc, const QString &tagName, const QStringList &idsToDelete)
{
    if (idsToDelete.isEmpty()) {
        return;
    }

    QDomNodeList assets = doc.elementsByTagName(tagName);
    for (int i = 0; i < assets.count(); ++i) {
        QDomElement asset = assets.item(i).toElement();
        QString service = Xml::getXmlProperty(asset, QStringLiteral("kdenlive_id"));
        if (service.isEmpty()) {
            service = Xml::getXmlProperty(asset, QStringLiteral("mlt_service"));
        }
        if (idsToDelete.contains(service)) {
            // Remove asset
            asset.parentNode().removeChild(asset);
            --i;
        }
    }
}

void DocumentChecker::fixClip(const QDomNodeList &items, const QString &clipId, const QString &newPath)
{
    QDomElement e;
    // Changing the tag name (below) will remove the producer from the list, so we need to parse in reverse order
    for (int i = items.count() - 1; i >= 0; --i) {
        e = items.item(i).toElement();

        if (getKdenliveClipId(e) != clipId) {
            continue;
        }

        QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        QString updatedPath = newPath;

        if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl")).isEmpty()) {
            // Only set originalurl on master producer
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:originalurl"), newPath);
        }
        if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.resource")).isEmpty()) {
            // Only set original.resource on master producer
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:original.resource"), newPath);
        }
        if (service == QLatin1String("timewarp")) {
            Xml::setXmlProperty(e, QStringLiteral("warp_resource"), updatedPath);
            updatedPath.prepend(Xml::getXmlProperty(e, QStringLiteral("warp_speed")) + QLatin1Char(':'));
        }
        if (service.startsWith(QLatin1String("avformat")) && e.tagName() == QLatin1String("producer")) {
            e.setTagName(QStringLiteral("chain"));
        }

        Xml::setXmlProperty(e, QStringLiteral("resource"), updatedPath);
    }
}

void DocumentChecker::removeClip(const QDomNodeList &producers, const QDomNodeList &chains, const QDomNodeList &playlists, const QString &clipId)
{
    QDomElement e;
    // remove the clips producer
    for (int i = 0; i < producers.count(); ++i) {
        e = producers.item(i).toElement();
        if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == clipId) {
            // Mark clip for deletion
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:remove"), QStringLiteral("1"));
        }
    }

    for (int i = 0; i < chains.count(); ++i) {
        e = chains.item(i).toElement();
        if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == clipId) {
            // Mark clip for deletion
            Xml::setXmlProperty(e, QStringLiteral("kdenlive:remove"), QStringLiteral("1"));
        }
    }

    // also remove all instances of the clip in playlists
    for (int i = 0; i < playlists.count(); ++i) {
        QDomNodeList entries = playlists.at(i).toElement().elementsByTagName(QStringLiteral("entry"));
        for (int j = 0; j < entries.count(); ++j) {
            e = entries.item(j).toElement();
            if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == clipId) {
                // Mark clip for deletion
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:remove"), QStringLiteral("1"));
            }
        }
    }
}

void DocumentChecker::fixMissingItem(const DocumentChecker::DocumentResource &resource, const QDomNodeList &producers, const QDomNodeList &chains,
                                     const QDomNodeList &trans, const QDomNodeList &filters)
{
    qDebug() << "==== FIXING PRODUCER WITH ID: " << resource.clipId;

    /*if (resource.type == MissingType::Sequence) {
        // Already processed
        return;
    }*/

    QDomElement e;
    if (resource.type == MissingType::TitleImage) {
        // Title clips are not embeded in chains
        // edit images embedded in titles
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            QString parentId = getKdenliveClipId(e);
            if (parentId == resource.clipId) {
                fixTitleImage(e, resource.originalFilePath, resource.newFilePath);
            }
        }
    } else if (resource.type == MissingType::Clip) {
        if (resource.status == MissingStatus::Fixed) {
            // edit clip url
            fixClip(chains, resource.clipId, resource.newFilePath);
            fixClip(producers, resource.clipId, resource.newFilePath);
        } else if (resource.status == MissingStatus::Remove) {
            QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
            removeClip(producers, chains, playlists, resource.clipId);
        } else if (resource.status == MissingStatus::Placeholder /*child->data(0, statusRole).toInt() == CLIPPLACEHOLDER*/) {
            usePlaceholderForClip(producers, resource.clipId);
            usePlaceholderForClip(chains, resource.clipId);
        }
    } else if (resource.type == MissingType::Proxy) {
        if (resource.status == MissingStatus::Fixed) {
            fixProxyClip(producers, resource.clipId, resource.originalFilePath, resource.newFilePath);
            fixProxyClip(chains, resource.clipId, resource.originalFilePath, resource.newFilePath);
        } else if (resource.status == MissingStatus::Reload) {
            removeProxy(producers, resource.clipId, true);
            removeProxy(chains, resource.clipId, true);
        } else if (resource.status == MissingStatus::Remove) {
            removeProxy(producers, resource.clipId, false);
            removeProxy(chains, resource.clipId, false);
        }

    } else if (resource.type == MissingType::TitleFont) {
        // Parse all title producers
        fixTitleFont(producers, resource.originalFilePath, resource.newFilePath);
    } else if (resource.type == MissingType::Luma) {
        QString newPath = resource.newFilePath;
        if (resource.status == MissingStatus::Remove) {
            newPath.clear();
        }
        fixAssetResource(trans, getLumaPairs(), resource.originalFilePath, newPath);
    } else if (resource.type == MissingType::AssetFile) {
        QString newPath = resource.newFilePath;
        if (resource.status == MissingStatus::Remove) {
            newPath.clear();
        }
        fixAssetResource(filters, getAssetPairs(), resource.originalFilePath, newPath);
    } else if (resource.type == MissingType::Effect && resource.status == MissingStatus::Remove) {
        removeAssetsById(m_doc, QStringLiteral("filter"), {resource.originalFilePath});
    } else if (resource.type == MissingType::Transition && resource.status == MissingStatus::Remove) {
        removeAssetsById(m_doc, QStringLiteral("transition"), {resource.originalFilePath});
    }
}

ClipType::ProducerType DocumentChecker::getClipType(const QString &service, const QString &resource)
{
    ClipType::ProducerType type = ClipType::Unknown;
    if (service.startsWith(QLatin1String("avformat")) || service == QLatin1String("framebuffer") || service == QLatin1String("timewarp")) {
        type = ClipType::AV;
    } else if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
        bool slideshow = isSlideshow(resource);
        if (slideshow) {
            type = ClipType::SlideShow;
        } else {
            type = ClipType::Image;
        }
    } else if (service == QLatin1String("mlt") || service == QLatin1String("xml")) {
        type = ClipType::Playlist;
    }
    return type;
}

QString DocumentChecker::getProducerResource(const QDomElement &producer)
{
    QString service = Xml::getXmlProperty(producer, QStringLiteral("mlt_service"));
    QString resource = Xml::getXmlProperty(producer, QStringLiteral("resource"));
    if (resource.isEmpty()) {
        return QString();
    }
    if (service == QLatin1String("timewarp")) {
        // slowmotion clip, trim speed info
        resource = Xml::getXmlProperty(producer, QStringLiteral("warp_resource"));
    } else if (service == QLatin1String("framebuffer")) {
        // slowmotion clip, trim speed info
        resource.section(QLatin1Char('?'), 0, 0);
    }
    return ensureAbsoultePath(resource);
}

QString DocumentChecker::getKdenliveClipId(const QDomElement &producer)
{
    QString clipId = Xml::getXmlProperty(producer, QStringLiteral("kdenlive:id"));
    if (clipId.isEmpty()) {
        // Older project file format
        clipId = producer.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
    }
    return clipId;
}

QString DocumentChecker::readableNameForClipType(ClipType::ProducerType type)
{
    switch (type) {
    case ClipType::AV:
        return i18n("Video clip");
    case ClipType::SlideShow:
        return i18n("Slideshow clip");
    case ClipType::Image:
        return i18n("Image clip");
    case ClipType::Playlist:
        return i18n("Playlist clip");
    case ClipType::Text:
        return i18n("Title Image"); // ?
    case ClipType::Unknown:
        return i18n("Unknown");
    default:
        return {};
    }
}

QString DocumentChecker::readableNameForMissingType(MissingType type)
{
    switch (type) {
    case MissingType::Clip:
        return i18n("Clip");
    case MissingType::TitleFont:
        return i18n("Title Font");
    case MissingType::TitleImage:
        return i18n("Title Image");
    case MissingType::Luma:
        return i18n("Luma file");
    case MissingType::AssetFile:
        return i18n("Asset file");
    case MissingType::Proxy:
        return i18n("Proxy clip");
    case MissingType::Effect:
        return i18n("Effect");
    case MissingType::Transition:
        return i18n("Transition");
    default:
        return i18n("Unknown");
    }
}

QString DocumentChecker::readableNameForMissingStatus(MissingStatus type)
{
    switch (type) {
    case MissingStatus::Fixed:
        return i18n("Fixed");
    case MissingStatus::Reload:
        return i18n("Reload");
    case MissingStatus::Missing:
        return i18n("Missing");
    case MissingStatus::MissingButProxy:
        return i18n("Missing, but proxy available");
    case MissingStatus::Placeholder:
        return i18n("Placeholder");
    case MissingStatus::Remove:
        return i18n("Remove");
    default:
        return i18n("Unknown");
    }
}

// TODO: remove?
QStringList DocumentChecker::getInfoMessages()
{
    QStringList messages;
    if (itemsContain(MissingType::Luma) || itemsContain(MissingType::AssetFile) || itemsContain(MissingType::Clip)) {
        messages.append(i18n("The project file contains missing clips or files."));
    }
    if (itemsContain(MissingType::Proxy)) {
        messages.append(i18n("Missing proxies can be recreated on opening."));
    }
    // TODO
    /*if (!m_missingSources.isEmpty()) {
        messages.append(i18np("The project file contains a missing clip, you can still work with its proxy.",
                              "The project file contains %1 missing clips, you can still work with their proxies.", m_missingSources.count()));
    }
    if (!m_changedClips.isEmpty()) {
        messages.append(i18np("The project file contains one modified clip, it will be reloaded.",
                              "The project file contains %1 modified clips, they will be reloaded.", m_changedClips.count()));
    }*/
    return messages;
}

bool DocumentChecker::itemsContain(MissingType type, const QString &path, MissingStatus status)
{
    for (auto item : m_items) {
        if (item.status != status) {
            continue;
        }
        if (type != item.type) {
            continue;
        }
        if (item.originalFilePath == path || path.isEmpty()) {
            return true;
        }
    }
    return false;
}

int DocumentChecker::itemIndexByClipId(const QString &clipId)
{
    for (std::size_t i = 0; i < m_items.size(); i++) {
        if (m_items[i].clipId == clipId) {
            return i;
        }
    }
    return -1;
}

bool DocumentChecker::isSlideshow(const QString &resource)
{
    return resource.contains(QStringLiteral("/.all.")) || resource.contains(QStringLiteral("\\.all.")) || resource.contains(QLatin1Char('?')) ||
           resource.contains(QLatin1Char('%'));
}

bool DocumentChecker::isProfileHD(const QDomDocument &doc)
{
    QDomElement profile = doc.documentElement().firstChildElement(QStringLiteral("profile"));
    if (!profile.isNull()) {
        if (profile.attribute(QStringLiteral("width")).toInt() < 1000) {
            return false;
        }
    }
    return true;
}

bool DocumentChecker::isSequenceWithSpeedEffect(const QDomElement &producer)
{
    QString service = Xml::getXmlProperty(producer, QStringLiteral("mlt_service"));
    QString resource = getProducerResource(producer);

    bool isSequence = resource.endsWith(QLatin1String(".mlt")) && resource.contains(QLatin1String("/sequences/"));

    QVector<QDomNode> links = Xml::getDirectChildrenByTagName(producer, QStringLiteral("link"));
    bool isTimeremap = service == QLatin1String("xml") && !links.isEmpty() &&
                       Xml::getXmlProperty(links.first().toElement(), QStringLiteral("mlt_service")) == QLatin1String("timeremap");

    return isSequence && (service == QLatin1String("timewarp") || isTimeremap);
}
