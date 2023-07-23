/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "documentchecker.h"
#include "bin/binplaylist.hpp"
#include "bin/projectclip.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "kthumb.h"
#include "titler/titlewidget.h"
#include "transitions/transitionsrepository.hpp"

#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentDirs>
#include <KUrlRequesterDialog>

#include "kdenlive_debug.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QTreeWidgetItem>
#include <kurlrequester.h>
#include <utility>

const int hashRole = Qt::UserRole;
const int sizeRole = Qt::UserRole + 1;
const int idRole = Qt::UserRole + 2;
const int statusRole = Qt::UserRole + 3;
const int typeRole = Qt::UserRole + 4;
const int typeOriginalResource = Qt::UserRole + 5;
const int clipTypeRole = Qt::UserRole + 6;

const int CLIPMISSING = 0;
const int CLIPOK = 1;
const int CLIPPLACEHOLDER = 2;
const int PROXYMISSING = 4;
const int SOURCEMISSING = 5;

const int LUMAMISSING = 10;
const int LUMAOK = 11;
const int LUMAPLACEHOLDER = 12;
const int ASSETMISSING = 13;
const int ASSETOK = 14;

enum MISSINGTYPE { TITLE_IMAGE_ELEMENT = 20, TITLE_FONT_ELEMENT = 21, SEQUENCE_ELEMENT = 22 };

DocumentChecker::DocumentChecker(QUrl url, const QDomDocument &doc)
    : m_url(std::move(url))
    , m_doc(doc)
    , m_dialog(nullptr)
    , m_abortSearch(false)
    , m_checkRunning(false)
{
    connect(this, &DocumentChecker::showScanning, [this](const QString &message) {
        m_ui.infoLabel->setText(message);
        m_ui.infoLabel->setVisible(true);
    });
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

bool DocumentChecker::hasErrorInClips()
{
    int max;
    QDomElement baseElement = m_doc.documentElement();
    QString root = baseElement.attribute(QStringLiteral("root"));
    if (!root.isEmpty()) {
        QDir dir(root);
        if (!dir.exists()) {
            // Looks like project was moved, try recovering root from current project url
            m_rootReplacement.first = dir.absolutePath() + QDir::separator();
            root = m_url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
            baseElement.setAttribute(QStringLiteral("root"), root);
            root = QDir::cleanPath(root) + QDir::separator();
            m_rootReplacement.second = root;
        } else {
            root = QDir::cleanPath(root) + QDir::separator();
        }
    }

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
                m_warnings.append(i18n("The document id of your project was invalid, a new one has been created."));
            }

            // ensure the storage for temp files exists
            storageFolder = Xml::getXmlProperty(mainBinPlaylist, QStringLiteral("kdenlive:docproperties.storagefolder"));
            storageFolder = ensureAbsoultePath(root, storageFolder);

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

    // Fill list of project tractors to detect corruptions
    m_tractorsList.clear();
    QDomNodeList documentTractors = m_doc.elementsByTagName(QStringLiteral("tractor"));
    max = documentTractors.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentTractors.item(i).toElement();
        m_tractorsList.append(e.attribute(QStringLiteral("id")));
    }

    QDomNodeList documentProducers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList documentChains = m_doc.elementsByTagName(QStringLiteral("chain"));
    QDomNodeList entries = m_doc.elementsByTagName(QStringLiteral("entry"));

    m_safeImages.clear();
    m_safeFonts.clear();
    m_missingFonts.clear();
    m_changedClips.clear();
    m_fixedSequences.clear();
    QStringList verifiedPaths;
    QStringList missingPaths;
    QStringList serviceToCheck = {QStringLiteral("kdenlivetitle"), QStringLiteral("qimage"), QStringLiteral("pixbuf"), QStringLiteral("timewarp"),
                                  QStringLiteral("framebuffer"),   QStringLiteral("xml"),    QStringLiteral("qtext"),  QStringLiteral("tractor")};
    max = documentProducers.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentProducers.item(i).toElement();
        verifiedPaths << getMissingProducers(e, entries, verifiedPaths, missingPaths, serviceToCheck, root, storageFolder);
    }
    max = documentChains.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentChains.item(i).toElement();
        verifiedPaths << getMissingProducers(e, entries, verifiedPaths, missingPaths, serviceToCheck, root, storageFolder);
    }

    QStringList missingLumas;
    QStringList missingAssets;

    // Check if we have HD profile. This is for to descide which luma files to use.
    QDomElement profile = baseElement.firstChildElement(QStringLiteral("profile"));
    bool hdProfile = true;
    if (!profile.isNull()) {
        if (profile.attribute(QStringLiteral("width")).toInt() < 1000) {
            hdProfile = false;
        }
    }

    QMap<QString, QString> autoFixLuma;

    // Check existence of luma files
    QStringList filesToCheck = getAssetsFiles(m_doc, QStringLiteral("transition"), getLumaPairs());
    for (const QString &lumafile : qAsConst(filesToCheck)) {
        QString filePath = ensureAbsoultePath(root, lumafile);

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
        lumaName.prepend(hdProfile ? QStringLiteral("/HD/") : QStringLiteral("/PAL/"));
        fixedLuma.append(lumaName);
        if (QFile::exists(fixedLuma)) {
            autoFixLuma.insert(filePath, fixedLuma);
            continue;
        }

        // Check Kdenlive folder
        QString res = fixLuma(filePath);
        if (!res.isEmpty() && QFile::exists(res)) {
            autoFixLuma.insert(filePath, res);
            continue;
        }

        // Try to change file extension
        if (filePath.endsWith(QLatin1String(".pgm"))) {
            fixedLuma = filePath.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".png");
        } else if (filePath.endsWith(QLatin1String(".png"))) {
            fixedLuma = filePath.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".pgm");
        }
        if (!fixedLuma.isEmpty() && QFile::exists(fixedLuma)) {
            autoFixLuma.insert(filePath, fixedLuma);
            continue;
        }

        // we have not been able to fix or find the file
        missingLumas.append(lumafile);
    }
    replaceTransitionsLumas(m_doc, autoFixLuma);

    // Check for missing transitions (eg. not installed)
    QStringList transtions = getAssetsServiceIds(m_doc, QStringLiteral("transition"));
    for (const QString &id : qAsConst(transtions)) {
        if (!TransitionsRepository::get()->exists(id)) {
            m_missingTransitions << id;
        }
    }
    m_missingTransitions.removeDuplicates();

    // Delete missing transitions
    removeAssetsById(m_doc, QStringLiteral("filter"), m_missingFilters);

    // Check for missing filter assets
    QStringList assetsToCheck = getAssetsFiles(m_doc, QStringLiteral("filter"), getAssetPairs());
    for (const QString &filterfile : qAsConst(assetsToCheck)) {
        QString filePath = ensureAbsoultePath(root, filterfile);

        if (!QFile::exists(filePath)) {
            missingAssets << filterfile;
        }
    }

    // Check for missing effects (eg. not installed)
    QStringList filters = getAssetsServiceIds(m_doc, QStringLiteral("filter"));
    for (const QString &id : qAsConst(filters)) {
        if (!EffectsRepository::get()->exists(id)) {
            m_missingFilters << id;
        }
    }
    m_missingFilters.removeDuplicates();

    // Delete missing effects
    removeAssetsById(m_doc, QStringLiteral("filter"), m_missingFilters);

    // Abort here if our MainWindow is not built (it means we are doing tests and don't want a dialog to pop up)
    if (pCore->window() == nullptr ||
        (m_missingClips.isEmpty() && missingLumas.isEmpty() && missingAssets.isEmpty() && m_missingProxies.isEmpty() && m_missingSources.isEmpty() &&
         m_missingFonts.isEmpty() && m_missingFilters.isEmpty() && m_missingTransitions.isEmpty() && m_changedClips.isEmpty() && m_fixedSequences.isEmpty())) {
        return false;
    }

    m_dialog = new QDialog();
    m_dialog->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_ui.setupUi(m_dialog);

    for (const QString &l : qAsConst(missingLumas)) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Luma file") << l);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        item->setData(0, idRole, l);
        item->setData(0, statusRole, LUMAMISSING);
    }
    for (const QString &l : qAsConst(missingAssets)) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Asset file") << l);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        item->setData(0, idRole, l);
        item->setData(0, statusRole, ASSETMISSING);
    }
    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_missingClips.isEmpty() && m_missingProxies.isEmpty() && m_missingSources.isEmpty());
    max = m_missingClips.count();
    m_missingProxyIds.clear();
    QMap<QString, QTreeWidgetItem *> itemMap;
    QStringList processedIds;
    for (int i = 0; i < max; ++i) {
        QDomElement e = m_missingClips.at(i).toElement();
        QString clipType;
        ClipType::ProducerType type;
        int status = CLIPMISSING;
        const QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        QString resource;
        QString proxy = Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy"));
        if (proxy.length() > 1) {
            resource = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
        } else {
            resource = service == QLatin1String("timewarp") ? Xml::getXmlProperty(e, QStringLiteral("warp_resource"))
                                                            : Xml::getXmlProperty(e, QStringLiteral("resource"));
        }
        bool slideshow = resource.contains(QStringLiteral("/.all.")) || resource.contains(QStringLiteral("\\.all.")) || resource.contains(QLatin1Char('?')) ||
                         resource.contains(QLatin1Char('%'));
        if (service.startsWith(QLatin1String("avformat")) || service == QLatin1String("framebuffer") || service == QLatin1String("timewarp")) {
            clipType = i18n("Video clip");
            type = ClipType::AV;
        } else if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
            if (slideshow) {
                clipType = i18n("Slideshow clip");
                type = ClipType::SlideShow;
            } else {
                clipType = i18n("Image clip");
                type = ClipType::Image;
            }
        } else if (service == QLatin1String("mlt") || service == QLatin1String("xml")) {
            clipType = i18n("Playlist clip");
            type = ClipType::Playlist;
        } else if (e.tagName() == QLatin1String("missingtitle")) {
            clipType = i18n("Title Image");
            status = TITLE_IMAGE_ELEMENT;
            type = ClipType::Text;
        } else {
            clipType = i18n("Unknown");
            type = ClipType::Unknown;
        }
        // Newer project format
        QString clipId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (!clipId.isEmpty()) {
            if (processedIds.contains(clipId)) {
                if (status != TITLE_IMAGE_ELEMENT && itemMap.value(clipId)->data(0, hashRole).toString().isEmpty()) {
                    QTreeWidgetItem *item = itemMap.value(clipId);
                    item->setData(0, hashRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash")));
                    item->setData(0, sizeRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size")));
                }
                continue;
            }
            processedIds << clipId;
        } else {
            // Older project file format
            clipId = e.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
            if (processedIds.contains(clipId)) {
                if (status != TITLE_IMAGE_ELEMENT && itemMap.value(clipId)->data(0, hashRole).toString().isEmpty()) {
                    QTreeWidgetItem *item = itemMap.value(clipId);
                    item->setData(0, hashRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash")));
                    item->setData(0, sizeRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size")));
                }
                continue;
            }
            processedIds << clipId;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPMISSING);
        item->setData(0, clipTypeRole, int(type));
        item->setData(0, idRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")));
        item->setToolTip(0, i18n("Missing item"));
        itemMap.insert(clipId, item);

        if (status == TITLE_IMAGE_ELEMENT) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
            item->setToolTip(1, e.attribute(QStringLiteral("name")));
            QString imageResource = e.attribute(QStringLiteral("resource"));
            item->setData(0, typeRole, status);
            item->setData(0, typeOriginalResource, e.attribute(QStringLiteral("resource")));
            if (!m_rootReplacement.first.isEmpty()) {
                const QString relocated = relocateResource(imageResource);
                if (!relocated.isEmpty()) {
                    item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                    item->setData(0, statusRole, CLIPOK);
                    item->setToolTip(0, i18n("Relocated item"));
                    imageResource = relocated;
                }
            }
            item->setText(1, imageResource);
        } else {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
            if (QFileInfo(resource).isRelative()) {
                resource.prepend(root);
            }
            item->setData(0, hashRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash")));
            item->setData(0, sizeRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size")));
            if (!m_rootReplacement.first.isEmpty()) {
                QString relocated;
                if (type == ClipType::SlideShow) {
                    // Strip filename
                    relocated = QFileInfo(resource).absolutePath();
                } else {
                    relocated = resource;
                }

                relocated = relocateResource(relocated);
                if (!relocated.isEmpty()) {
                    item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                    item->setData(0, statusRole, CLIPOK);
                    item->setToolTip(0, i18n("Relocated item"));
                    if (type == ClipType::SlideShow) {
                        resource = QDir(relocated).absoluteFilePath(QFileInfo(resource).fileName());
                    } else {
                        resource = relocated;
                    }
                }
            }
            item->setText(1, resource);
        }
    }

    for (const QString &font : qAsConst(m_missingFonts)) {
        QString clipType = i18n("Title Font");
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPPLACEHOLDER);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-information")));
        QString newft = QFontInfo(QFont(font)).family();
        item->setText(1, i18n("%1 will be replaced by %2", font, newft));
        item->setData(0, typeRole, TITLE_FONT_ELEMENT);
    }

    for (const QString &url : qAsConst(m_changedClips)) {
        QString clipType = i18n("Modified Clips");
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPPLACEHOLDER);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-information")));
        item->setText(1, i18n("Clip %1 will be reloaded", url));
        item->setData(0, typeRole, TITLE_FONT_ELEMENT);
    }

    for (const QString &id : qAsConst(m_fixedSequences)) {
        QString clipType = i18n("Fixed Sequence Clips");
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPPLACEHOLDER);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-information")));
        item->setText(1, i18n("Clip %1 will be fixed", id));
        item->setData(0, typeRole, SEQUENCE_ELEMENT);
        item->setData(0, idRole, id);
    }

    QStringList infoLabel;
    if (!m_missingClips.isEmpty()) {
        infoLabel.append(i18n("The project file contains missing clips or files."));
    }
    if (!m_missingFilters.isEmpty()) {
        infoLabel.append(i18np("Missing effect: %2 will be removed from project.", "Missing effects: %2 will be removed from project.",
                               m_missingFilters.count(), m_missingFilters.join(",")));
    }
    if (!m_missingTransitions.isEmpty()) {
        infoLabel.append(i18np("Missing transition: %2 will be removed from project.", "Missing transitions: %2 will be removed from project.",
                               m_missingTransitions.count(), m_missingTransitions.join(",")));
    }
    if (!m_missingProxies.isEmpty()) {
        infoLabel.append(i18n("Missing proxies can be recreated on opening."));
        m_ui.rebuildProxies->setChecked(true);
        connect(m_ui.rebuildProxies, &QCheckBox::stateChanged, this, [this](int state) {
            for (auto e : qAsConst(m_missingProxies)) {
                if (state == Qt::Checked) {
                    e.setAttribute(QStringLiteral("_replaceproxy"), QStringLiteral("1"));
                } else {
                    e.removeAttribute(QStringLiteral("_replaceproxy"));
                }
            }
        });
    } else {
        m_ui.rebuildProxies->setVisible(false);
    }
    if (!m_missingSources.isEmpty()) {
        infoLabel.append(i18np("The project file contains a missing clip, you can still work with its proxy.",
                               "The project file contains %1 missing clips, you can still work with their proxies.", m_missingSources.count()));
    }
    if (!m_changedClips.isEmpty()) {
        infoLabel.append(i18np("The project file contains one modified clip, it will be reloaded.",
                               "The project file contains %1 modified clips, they will be reloaded.", m_changedClips.count()));
    }
    if (!infoLabel.isEmpty()) {
        m_ui.infoLabel->setText(infoLabel.join(QStringLiteral("\n")));
    } else {
        m_ui.infoLabel->setVisible(false);
    }
    m_ui.recursiveSearch->setCheckable(true);
    m_ui.removeSelected->setEnabled(!m_missingClips.isEmpty());
    bool hasMissing = !m_missingClips.isEmpty() || !missingLumas.isEmpty() || !missingAssets.isEmpty() || !m_missingSources.isEmpty();
    m_ui.recursiveSearch->setEnabled(hasMissing);
    m_ui.usePlaceholders->setEnabled(!m_missingClips.isEmpty());
    m_ui.manualSearch->setEnabled(!m_missingClips.isEmpty());

    // Check missing proxies
    max = m_missingProxies.count();
    for (int i = 0; i < max; ++i) {
        QDomElement e = m_missingProxies.at(i).toElement();
        QString realPath = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
        QString id = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        QString originalService = Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.mlt_service"));
        m_missingProxyIds << id;
        // Tell Kdenlive to recreate proxy
        e.setAttribute(QStringLiteral("_replaceproxy"), QStringLiteral("1"));
        // Remove reference to missing proxy
        Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
        // Replace proxy url with real clip in MLT producers
        auto replaceProxy = [this](QDomElement &mltProd, const QString &id, const QString &realPath, const QString &originalService,
                                   const QStringList &missingPaths) {
            QString parentId = Xml::getXmlProperty(mltProd, QStringLiteral("kdenlive:id"));
            if (parentId == id) {
                // Hit, we must replace url
                QString prefix;
                if (Xml::getXmlProperty(mltProd, QStringLiteral("mlt_service")) == QLatin1String("timewarp")) {
                    prefix = Xml::getXmlProperty(mltProd, QStringLiteral("warp_speed"));
                    prefix.append(QLatin1Char(':'));
                    Xml::setXmlProperty(mltProd, QStringLiteral("warp_resource"), prefix + realPath);
                } else if (!originalService.isEmpty()) {
                    Xml::setXmlProperty(mltProd, QStringLiteral("mlt_service"), originalService);
                }
                prefix.append(realPath);
                Xml::setXmlProperty(mltProd, QStringLiteral("resource"), prefix);
                Xml::setXmlProperty(mltProd, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));

                if (missingPaths.contains(realPath)) {
                    // Proxy AND source missing
                    Xml::setXmlProperty(mltProd, QStringLiteral("_placeholder"), QStringLiteral("1"));
                    Xml::setXmlProperty(mltProd, QStringLiteral("kdenlive:orig_service"), Xml::getXmlProperty(mltProd, QStringLiteral("mlt_service")));
                }
            }
            return true;
        };
        QDomElement mltProd;
        int prodsCount = documentProducers.count();
        for (int j = 0; j < prodsCount; ++j) {
            mltProd = documentProducers.at(j).toElement();
            replaceProxy(mltProd, id, realPath, originalService, missingPaths);
        }
        prodsCount = documentChains.count();
        for (int j = 0; j < prodsCount; ++j) {
            mltProd = documentChains.at(j).toElement();
            replaceProxy(mltProd, id, realPath, originalService, missingPaths);
        }
    }

    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Proxy clip"));
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
        item->setText(
            1, i18np("%1 missing proxy clip, will be recreated on project opening", "%1 missing proxy clips, will be recreated on project opening", max));
        // item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, PROXYMISSING);
        item->setToolTip(0, i18n("Missing proxy"));
    }

    if (max > 0) {
        // original doc was modified
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
    }

    // Check clips with available proxies but missing original source clips
    max = m_missingSources.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Source clip"));
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
        item->setText(1, i18n("%1 missing source clips, you can only use the proxies", max));
        // item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, SOURCEMISSING);
        item->setToolTip(0, i18n("Missing source clip"));
        for (int i = 0; i < max; ++i) {
            QDomElement e = m_missingSources.at(i).toElement();
            QString realPath = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
            // Tell Kdenlive the source is missing
            if (QFileInfo(realPath).isRelative()) {
                realPath.prepend(root);
            }
            e.setAttribute(QStringLiteral("_missingsource"), QStringLiteral("1"));
            QTreeWidgetItem *subitem = new QTreeWidgetItem(item, QStringList() << i18n("Source clip"));
            // qCDebug(KDENLIVE_LOG)<<"// Adding missing source clip: "<<realPath;
            subitem->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
            subitem->setText(1, realPath);
            subitem->setData(0, hashRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash")));
            subitem->setData(0, sizeRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size")));
            subitem->setData(0, statusRole, SOURCEMISSING);
            // int t = e.attribute("type").toInt();
            subitem->setData(0, typeRole, Xml::getXmlProperty(e, QStringLiteral("mlt_service")));
            subitem->setData(0, idRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")));
        }
    }
    if (max > 0) {
        // original doc was modified
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
    }
    m_ui.treeWidget->resizeColumnToContents(0);
    connect(m_ui.recursiveSearch, &QAbstractButton::pressed, this, &DocumentChecker::slotCheckClips, Qt::DirectConnection);
    connect(m_ui.usePlaceholders, &QAbstractButton::pressed, this, &DocumentChecker::slotPlaceholders);
    connect(m_ui.removeSelected, &QAbstractButton::pressed, this, &DocumentChecker::slotDeleteSelected);
    connect(m_ui.treeWidget, &QTreeWidget::itemDoubleClicked, this, &DocumentChecker::slotEditItem);
    connect(m_ui.treeWidget, &QTreeWidget::itemSelectionChanged, this, &DocumentChecker::slotCheckButtons);
    connect(m_ui.manualSearch, &QAbstractButton::clicked, this, [this]() { slotEditItem(m_ui.treeWidget->currentItem(), 0); });
    // adjustSize();
    if (m_ui.treeWidget->topLevelItem(0)) {
        m_ui.treeWidget->setCurrentItem(m_ui.treeWidget->topLevelItem(0));
    }
    checkStatus();
    int acceptMissing = m_dialog->exec();
    if (acceptMissing == QDialog::Accepted) {
        acceptDialog();
    }
    return (acceptMissing != QDialog::Accepted);
}

DocumentChecker::~DocumentChecker()
{
    delete m_dialog;
}

const QString DocumentChecker::relocateResource(QString sourceResource)
{
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

QString DocumentChecker::getMissingProducers(QDomElement &e, const QDomNodeList &entries, const QStringList &verifiedPaths, QStringList &missingPaths,
                                             const QStringList &serviceToCheck, const QString &root, const QString &storageFolder)
{
    QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
    if (!service.startsWith(QLatin1String("avformat")) && !serviceToCheck.contains(service)) {
        return QString();
    }
    if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")).isEmpty()) {
        // This should not happen, try to recover the producer id
        int max2 = entries.count();
        QString producerName = e.attribute(QStringLiteral("id"));
        for (int j = 0; j < max2; j++) {
            QDomElement e2 = entries.item(j).toElement();
            if (e2.attribute(QStringLiteral("producer")) == producerName) {
                // Matche found
                QString entryName = Xml::getXmlProperty(e2, QStringLiteral("kdenlive:id"));
                if (!entryName.isEmpty()) {
                    Xml::setXmlProperty(e, QStringLiteral("kdenlive:id"), entryName);
                    break;
                }
            }
        }
    }
    bool isBinClip = m_binIds.contains(e.attribute(QLatin1String("id")));
    if (service == QLatin1String("qtext")) {
        QString text = Xml::getXmlProperty(e, QStringLiteral("text"));
        if (text == QLatin1String("INVALID")) {
            // Warning, this is an invalid clip (project saved with missing source)
            // Check if source clip is now available
            QString resource = Xml::getXmlProperty(e, QStringLiteral("warp_resource"));
            if (resource.isEmpty()) {
                resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
            }
            // Make sure to have absolute paths
            if (QFileInfo(resource).isRelative()) {
                resource.prepend(root);
            }
            if (QFile::exists(resource)) {
                // Reset to original service
                Xml::removeXmlProperty(e, QStringLiteral("text"));
                QString original_service = Xml::getXmlProperty(e, QStringLiteral("kdenlive:orig_service"));
                if (!original_service.isEmpty()) {
                    Xml::setXmlProperty(e, QStringLiteral("mlt_service"), original_service);
                } else {
                    // Try to guess service
                    if (Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
                        Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("qimage"));
                    } else if (resource.endsWith(QLatin1String(".kdenlivetitle"))) {
                        Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("kdenlivetitle"));
                    } else if (resource.endsWith(QLatin1String(".kdenlive")) || resource.endsWith(QLatin1String(".mlt"))) {
                        Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("xml"));
                    } else {
                        Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
                    }
                }
            }
            return QString();
        }

        checkMissingImagesAndFonts(QStringList(), QStringList(Xml::getXmlProperty(e, QStringLiteral("family"))), e.attribute(QStringLiteral("id")),
                                   e.attribute(QStringLiteral("name")));
        return QString();
    } else if (service == QLatin1String("kdenlivetitle")) {
        // TODO: Check is clip template is missing (xmltemplate) or hash changed
        QString xml = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
        QStringList images = TitleWidget::extractImageList(xml);
        QStringList fonts = TitleWidget::extractFontList(xml);
        checkMissingImagesAndFonts(images, fonts, Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")), e.attribute(QStringLiteral("name")));
        return QString();
    } else if (isBinClip && service == QLatin1String("tractor")) {
        // Check if this is a broken sequence clip / bug in Kdenlive 23.04.04
        QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
        if (resource.endsWith(QLatin1String("tractor>"))) {
            const QString brokenId = e.attribute(QStringLiteral("id"));
            const QString brokenUuid = Xml::getXmlProperty(e, QStringLiteral("kdenlive:uuid"));
            // Check that we have the original clip somewhere in the producers list
            if (brokenId != brokenUuid && m_tractorsList.contains(brokenUuid)) {
                // Replace bin clip entry
                for (int i = 0; i < m_binEntries.count(); ++i) {
                    QDomElement e = m_binEntries.item(i).toElement();
                    if (e.attribute(QStringLiteral("producer")) == brokenId) {
                        // Match
                        e.setAttribute(QStringLiteral("producer"), brokenUuid);
                        m_fixedSequences.append(brokenId);
                        return QString();
                    }
                }
            } else {
                // entry not found, this is a more complex recovery:
                // Change tag to tractor
                // Reinsert all tractor as tracks
                // Move the node just before main_bin to ensure its children tracks are defined before it
                // e.setTagName(QStringLiteral("tractor"));
                // Xml::removeXmlProperty(e, QStringLiteral("resource"));
                e.setAttribute(QStringLiteral("_fixbroken_tractor"), 1);
                m_fixedSequences.append(brokenId);
                return QString();
            }
        }
    }
    QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
    if (resource.isEmpty()) {
        return QString();
    }
    if (service == QLatin1String("timewarp")) {
        // slowmotion clip, trim speed info
        resource = Xml::getXmlProperty(e, QStringLiteral("warp_resource"));
    } else if (service == QLatin1String("framebuffer")) {
        // slowmotion clip, trim speed info
        resource = resource.section(QLatin1Char('?'), 0, 0);
    }

    // Make sure to have absolute paths
    if (QFileInfo(resource).isRelative()) {
        resource.prepend(root);
    }
    if (verifiedPaths.contains(resource)) {
        // Don't check same url twice (for example track producers)
        if (missingPaths.contains(resource)) {
            if ((resource.endsWith(QLatin1String(".mlt")) && resource.contains(QLatin1String("/sequences/"))) &&
                (service == QLatin1String("timewarp") ||
                 (service == QLatin1String("xml") && !Xml::getDirectChildrenByTagName(e, QStringLiteral("link")).isEmpty() &&
                  Xml::getXmlProperty(Xml::getDirectChildrenByTagName(e, QStringLiteral("link")).first().toElement(), QStringLiteral("mlt_service")) ==
                      QLatin1String("timeremap")))) {
                // This is a missing timeline sequence clip with speed effect, trigger recreate on opening
                Xml::setXmlProperty(e, QStringLiteral("_rebuild"), QStringLiteral("1"));
            } else {
                m_missingClips.append(e);
            }
        }
        return QString();
    }
    QString producerResource = resource;
    QString proxy = Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy"));
    // TODO: should this only apply to bin clips (isBinClip)
    if (proxy.length() > 1) {
        bool proxyFound = true;
        if (QFileInfo(proxy).isRelative()) {
            proxy.prepend(root);
        }
        if (!QFile::exists(proxy)) {
            // Missing clip found
            // Check if proxy exists in current storage folder
            bool fixed = false;
            if (!storageFolder.isEmpty()) {
                QDir dir(storageFolder + QStringLiteral("/proxy/"));
                if (dir.exists(QFileInfo(proxy).fileName())) {
                    QString updatedPath = dir.absoluteFilePath(QFileInfo(proxy).fileName());
                    fixProxyClip(e.attribute(QStringLiteral("id")), Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy")), updatedPath);
                    fixed = true;
                }
            }
            if (!fixed) {
                proxyFound = false;
            }
        }
        QString original = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
        if (QFileInfo(original).isRelative()) {
            original.prepend(root);
        }

        // Check for slideshows
        bool slideshow = original.contains(QStringLiteral("/.all.")) || original.contains(QStringLiteral("\\.all.")) || original.contains(QLatin1Char('?')) ||
                         original.contains(QLatin1Char('%'));
        if (slideshow && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
            original = QFileInfo(original).absolutePath();
        }
        if (!QFile::exists(original)) {
            bool resourceFixed = false;
            if (!m_rootReplacement.first.isEmpty()) {
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
            }
            if (!proxyFound) {
                // Neither proxy nor original file found
                if (!resourceFixed) {
                    m_missingClips.append(e);
                }
                m_missingProxies.append(e);
            } else {
                // clip has proxy but original clip is missing
                m_missingSources.append(e);
            }
            missingPaths.append(original);
        } else if (!proxyFound) {
            m_missingProxies.append(e);
        }
        return resource;
    }
    // Check for slideshows
    QString slidePattern;
    bool slideshow = resource.contains(QStringLiteral("/.all.")) || resource.contains(QStringLiteral("\\.all.")) || resource.contains(QLatin1Char('?')) ||
                     resource.contains(QLatin1Char('%'));
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
            if (QFileInfo(original).isRelative()) {
                original.prepend(root);
            }
            if (original != resource && QFile::exists(original)) {
                // Fix timewarp producer
                Xml::setXmlProperty(e, QStringLiteral("warp_resource"), original);
                Xml::setXmlProperty(e, QStringLiteral("resource"), Xml::getXmlProperty(e, QStringLiteral("warp_speed")) + QStringLiteral(":") + original);
                return original;
            }
        }
        // Missing clip found, make sure to omit timeline preview
        if (QFileInfo(resource).absolutePath().endsWith(QString("/%1/preview").arg(m_documentid))) {
            // This is a timeline preview missing chunk, ignore
        } else if ((resource.endsWith(QLatin1String(".mlt")) && resource.contains(QLatin1String("/sequences/"))) &&
                   (service == QLatin1String("timewarp") ||
                    (service == QLatin1String("xml") && !Xml::getDirectChildrenByTagName(e, QStringLiteral("link")).isEmpty() &&
                     Xml::getXmlProperty(Xml::getDirectChildrenByTagName(e, QStringLiteral("link")).first().toElement(), QStringLiteral("mlt_service")) ==
                         QLatin1String("timeremap")))) {
            // This is a missing timeline sequence clip with speed effect, trigger recreate on opening
            Xml::setXmlProperty(e, QStringLiteral("_rebuild"), QStringLiteral("1"));
            missingPaths.append(resource);
        } else {
            m_missingClips.append(e);
            missingPaths.append(resource);
        }
    } else if (isBinClip &&
               (service.startsWith(QLatin1String("avformat")) || slideshow || service == QLatin1String("qimage") || service == QLatin1String("pixbuf"))) {
        // Check if file changed
        const QByteArray hash = Xml::getXmlProperty(e, "kdenlive:file_hash").toLatin1();
        if (!hash.isEmpty()) {
            const QByteArray fileData =
                slideshow ? ProjectClip::getFolderHash(QDir(resource), slidePattern).toHex() : ProjectClip::calculateHash(resource).first.toHex();
            if (hash != fileData) {
                // For slideshow clips, silently upgrade hash
                if (slideshow) {
                    Xml::setXmlProperty(e, "kdenlive:file_hash", fileData);
                } else {
                    // Clip was changed, notify and trigger clip reload
                    Xml::removeXmlProperty(e, "kdenlive:file_hash");
                    m_changedClips.append(resource);
                }
            }
        }
    }
    // Make sure we don't query same path twice
    return producerResource;
}

void DocumentChecker::slotCheckClips()
{
    if (m_checkRunning) {
        m_abortSearch = true;
    } else {
        m_abortSearch = false;
        m_checkRunning = true;
        QString clipFolder = m_url.adjusted(QUrl::RemoveFilename).toLocalFile();
        const QString newpath = QFileDialog::getExistingDirectory(qApp->activeWindow(), i18nc("@title:window", "Clips Folder"), clipFolder);
        if (newpath.isEmpty()) {
            m_checkRunning = false;
            return;
        }
        slotSearchClips(newpath);
    }
}

void DocumentChecker::slotSearchClips(const QString &newpath)
{
    int ix = 0;
    bool fixed = false;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    QDir searchDir(newpath);
    QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList chains = m_doc.elementsByTagName(QStringLiteral("chain"));
    while (child != nullptr) {
        if (m_abortSearch) {
            break;
        }
        qApp->processEvents();
        if (child->data(0, statusRole).toInt() == SOURCEMISSING) {
            for (int j = 0; j < child->childCount(); ++j) {
                QTreeWidgetItem *subchild = child->child(j);
                QString clipPath =
                    searchFileRecursively(searchDir, subchild->data(0, sizeRole).toString(), subchild->data(0, hashRole).toString(), subchild->text(1));
                if (!clipPath.isEmpty()) {
                    fixed = true;
                    subchild->setText(1, clipPath);
                    subchild->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                    subchild->setData(0, statusRole, CLIPOK);
                    subchild->setToolTip(0, i18n("Recovered item"));
                    // Remove missing source attribute
                    const QString id = subchild->data(0, idRole).toString();
                    fixMissingSource(id, producers, chains);
                }
            }
        } else if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            bool perfectMatch = true;
            ClipType::ProducerType type = ClipType::ProducerType(child->data(0, clipTypeRole).toInt());
            QString clipPath;
            if (type != ClipType::SlideShow) {
                // Slideshows cannot be found with hash / size
                clipPath = searchFileRecursively(searchDir, child->data(0, sizeRole).toString(), child->data(0, hashRole).toString(), child->text(1));
            } else {
                clipPath = searchDirRecursively(searchDir, child->data(0, hashRole).toString(), child->text(1));
            }
            if (clipPath.isEmpty() && type != ClipType::SlideShow) {
                clipPath = searchPathRecursively(searchDir, QUrl::fromLocalFile(child->text(1)).fileName(), type);
                perfectMatch = false;
            }
            if (!clipPath.isEmpty()) {
                fixed = true;
                child->setText(1, clipPath);
                child->setIcon(0, perfectMatch ? QIcon::fromTheme(QStringLiteral("dialog-ok")) : QIcon::fromTheme(QStringLiteral("dialog-warning")));
                child->setToolTip(0, i18n("Recovered item"));
                child->setData(0, statusRole, CLIPOK);
            }
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            QString fileName = searchLuma(searchDir, child->data(0, idRole).toString());
            if (!fileName.isEmpty()) {
                fixed = true;
                child->setText(1, fileName);
                child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                child->setData(0, statusRole, LUMAOK);
                child->setToolTip(0, i18n("Recovered item"));
            }
        } else if (child->data(0, statusRole).toInt() == ASSETMISSING) {
            QString fileName = searchPathRecursively(searchDir, QFileInfo(child->data(0, idRole).toString()).fileName());
            if (!fileName.isEmpty()) {
                fixed = true;
                child->setText(1, fileName);
                child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                child->setData(0, statusRole, ASSETOK);
                child->setToolTip(0, i18n("Recovered item"));
            }
        } else if (child->data(0, typeRole).toInt() == TITLE_IMAGE_ELEMENT && child->data(0, statusRole).toInt() == CLIPPLACEHOLDER) {
            // Search missing title images
            QString missingFileName = QUrl::fromLocalFile(child->text(1)).fileName();
            QString newPath = searchPathRecursively(searchDir, missingFileName);
            if (!newPath.isEmpty()) {
                // File found
                fixed = true;
                child->setText(1, newPath);
                child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                child->setData(0, statusRole, CLIPOK);
                child->setToolTip(0, i18n("Recovered item"));
            }
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    m_ui.recursiveSearch->setChecked(false);
    m_ui.recursiveSearch->setEnabled(true);
    if (fixed) {
        // original doc was modified
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
    }
    if (m_abortSearch) {
        Q_EMIT showScanning(i18n("Search aborted"));
    } else {
        Q_EMIT showScanning(i18n("Search done"));
    }
    checkStatus();
    slotCheckButtons();
    m_checkRunning = false;
}

QString DocumentChecker::fixLuma(const QString &file)
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
    QString result = fixLuma(file);
    return result.isEmpty() ? searchPathRecursively(dir, QFileInfo(file).fileName()) : result;
}

QString DocumentChecker::searchPathRecursively(const QDir &dir, const QString &fileName, ClipType::ProducerType type)
{
    QString foundFileName;
    bool patternSlideshow = true;
    QDir searchDir(dir);
    QStringList filesAndDirs;
    qApp->processEvents();
    if (m_abortSearch) {
        return QString();
    }
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
    if (m_abortSearch) {
        return QString();
    }
    Q_EMIT showScanning(i18n("Scanning %1", dir.absolutePath()));
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
    if (m_abortSearch) {
        return QString();
    }
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
        if (m_abortSearch) {
            return QString();
        }
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

QString DocumentChecker::ensureAbsoultePath(const QString &root, QString filepath)
{
    if (!filepath.isEmpty() && QFileInfo(filepath).isRelative()) {
        filepath.prepend(root);
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

void DocumentChecker::replaceTransitionsLumas(QDomDocument &doc, const QMap<QString, QString> &names)
{
    if (names.isEmpty()) {
        return;
    }

    QMap<QString, QString> lumaSearchPairs = getLumaPairs();

    QDomNodeList trans = doc.elementsByTagName(QStringLiteral("transition"));
    int max = trans.count();
    for (int i = 0; i < max; ++i) {
        QDomElement transition = trans.at(i).toElement();
        QString service = Xml::getXmlProperty(transition, QStringLiteral("mlt_service"));
        QString luma;
        if (lumaSearchPairs.contains(service)) {
            luma = Xml::getXmlProperty(transition, lumaSearchPairs.value(service));
        }
        if (!luma.isEmpty() && names.contains(luma)) {
            Xml::setXmlProperty(transition, lumaSearchPairs.value(service), names.value(luma));
        }
    }
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

void DocumentChecker::slotEditItem(QTreeWidgetItem *item, int)
{
    if (!item) {
        return;
    }
    int t = item->data(0, typeRole).toInt();
    if (t == TITLE_FONT_ELEMENT || t == SEQUENCE_ELEMENT) {
        return;
    }
    //|| t == TITLE_IMAGE_ELEMENT) {
    ClipType::ProducerType type = ClipType::ProducerType(item->data(0, clipTypeRole).toInt());
    QUrl url;
    if (type == ClipType::SlideShow) {
        QString path = QFileInfo(item->text(1)).dir().absolutePath();
        QPointer<KUrlRequesterDialog> dlg(new KUrlRequesterDialog(QUrl::fromLocalFile(path), i18n("Enter new location for folder"), m_dialog));
        dlg->urlRequester()->setMode(KFile::Directory | KFile::ExistingOnly);
        if (dlg->exec() != QDialog::Accepted) {
            delete dlg;
            return;
        }
        url = QUrl::fromLocalFile(QDir(dlg->selectedUrl().path()).absoluteFilePath(QFileInfo(item->text(1)).fileName()));
        // Reset hash to ensure we find it next time
        const QString id = item->data(0, idRole).toString();
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        QDomElement e;
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
            if (parentId.isEmpty()) {
                // This is probably an old project file
                QString sourceId = e.attribute(QStringLiteral("id"));
                parentId = sourceId.section(QLatin1Char('_'), 0, 0);
            }
            if (parentId == id) {
                // Fix clip
                Xml::removeXmlProperty(e, QStringLiteral("kdenlive:file_hash"));
            }
        }
        delete dlg;
    } else {
        url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(item->text(1)), m_dialog, i18n("Enter new location for file"));
    }
    if (!url.isValid()) {
        return;
    }
    item->setText(1, url.toLocalFile());
    bool fixed = false;
    if (type == ClipType::SlideShow && QFile::exists(url.adjusted(QUrl::RemoveFilename).toLocalFile())) {
        fixed = true;
    }
    if (fixed || QFile::exists(url.toLocalFile())) {
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
        item->setToolTip(0, i18n("Relocated item"));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) {
            item->setData(0, statusRole, CLIPOK);
        } else if (id == LUMAMISSING) {
            item->setData(0, statusRole, LUMAOK);
        } else if (id == ASSETMISSING) {
            item->setData(0, statusRole, ASSETOK);
        } else if (id == SOURCEMISSING) {
            QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
            QDomNodeList chains = m_doc.elementsByTagName(QStringLiteral("chain"));
            fixMissingSource(item->data(0, idRole).toString(), producers, chains);
        }
        checkStatus();
    } else {
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) {
            item->setData(0, statusRole, CLIPMISSING);
        } else if (id == LUMAMISSING) {
            item->setData(0, statusRole, LUMAMISSING);
        } else if (id == ASSETMISSING) {
            item->setData(0, statusRole, ASSETMISSING);
        }
        checkStatus();
    }
}

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

void DocumentChecker::acceptDialog()
{
    QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList chains = m_doc.elementsByTagName(QStringLiteral("chain"));
    int ix = 0;

    // prepare transitions
    QDomNodeList trans = m_doc.elementsByTagName(QStringLiteral("transition"));
    QDomNodeList filters = m_doc.elementsByTagName(QStringLiteral("filter"));

    // Check if we have corrupted sequences first (Kdenlive 23.04.0 did produce some corruptions)
    if (!m_fixedSequences.isEmpty()) {
        QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
        QDomElement e;
        while (child != nullptr) {
            int t = child->data(0, typeRole).toInt();
            if (t == SEQUENCE_ELEMENT) {
                const QString id = child->data(0, idRole).toString();
                for (int i = 0; i < producers.count(); ++i) {
                    e = producers.item(i).toElement();
                    if (e.attribute(QStringLiteral("id")) == id) {
                        if (e.hasAttribute(QStringLiteral("_fixbroken_tractor")) && e.elementsByTagName(QStringLiteral("track")).isEmpty()) {
                            // Change tag, add tracks and move to the end of the document (just before the main_bin)
                            QDomNodeList tractors = m_doc.elementsByTagName(QStringLiteral("tractor"));
                            QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
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
                            for (int j = 0; j < tractors.count(); ++j) {
                                QDomElement current = tractors.item(j).toElement();
                                // Check all non used tractors and attach them as tracks
                                if (!Xml::hasXmlProperty(current, QStringLiteral("kdenlive:projectTractor")) &&
                                    !insertedTractors.contains(current.attribute("id"))) {
                                    QDomElement tk = m_doc.createElement(QStringLiteral("track"));
                                    tk.setAttribute(QStringLiteral("producer"), current.attribute(QStringLiteral("id")));
                                    lastProperty = e.insertAfter(tk, lastProperty);
                                }
                            }
                            e.removeAttribute(QStringLiteral("_fixbroken_tractor"));
                            QDomNode brokenSequence = m_doc.documentElement().removeChild(producers.item(i));
                            QDomElement fixedSequence = brokenSequence.toElement();
                            fixedSequence.setTagName(QStringLiteral("tractor"));
                            Xml::removeXmlProperty(fixedSequence, QStringLiteral("resource"));
                            Xml::removeXmlProperty(fixedSequence, QStringLiteral("mlt_service"));
                            for (int p = 0; p < playlists.count(); ++p) {
                                if (playlists.at(p).toElement().attribute(QStringLiteral("id")) == BinPlaylist::binPlaylistId) {
                                    QDomNode mainBinPlaylist = playlists.at(p);
                                    m_doc.documentElement().insertBefore(brokenSequence, mainBinPlaylist);
                                }
                            }
                            break;
                        }
                    }
                }
            }
            ix++;
            child = m_ui.treeWidget->topLevelItem(ix);
        }
    }
    ix = 0;
    // Mark document as modified
    m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);

    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child != nullptr) {
        if (child->data(0, statusRole).toInt() == SOURCEMISSING) {
            for (int j = 0; j < child->childCount(); ++j) {
                fixSourceClipItem(child->child(j), producers, chains);
            }
        } else {
            fixClipItem(child, producers, chains, trans, filters);
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
        qApp->processEvents();
    }
    // QDialog::accept();
}

void DocumentChecker::fixProxyClip(const QString &id, const QString &oldUrl, const QString &newUrl)
{
    QDomElement e, property;
    QDomNodeList properties;
    QDomNodeList documentProducers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomNodeList documentChains = m_doc.elementsByTagName(QStringLiteral("chain"));
    for (int i = 0; i < documentProducers.count(); ++i) {
        e = documentProducers.item(i).toElement();
        QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (parentId.isEmpty()) {
            // This is probably an old project file
            QString sourceId = e.attribute(QStringLiteral("id"));
            parentId = sourceId.section(QLatin1Char('_'), 0, 0);
        }
        if (parentId == id) {
            doFixProxyClip(e, oldUrl, newUrl);
        }
    }
    for (int i = 0; i < documentChains.count(); ++i) {
        e = documentChains.item(i).toElement();
        QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (parentId.isEmpty()) {
            // This is probably an old project file
            QString sourceId = e.attribute(QStringLiteral("id"));
            parentId = sourceId.section(QLatin1Char('_'), 0, 0);
        }
        if (parentId == id) {
            doFixProxyClip(e, oldUrl, newUrl);
        }
    }
}

void DocumentChecker::doFixProxyClip(QDomElement &e, const QString &oldUrl, const QString &newUrl)
{
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

void DocumentChecker::fixSourceClipItem(QTreeWidgetItem *child, const QDomNodeList &producers, const QDomNodeList &chains)
{
    QDomElement e;
    auto fixClip = [this](QDomElement &e, const QString &id, const QString &fixedResource) {
        QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (parentId.isEmpty()) {
            // This is probably an old project file
            QString sourceId = e.attribute(QStringLiteral("id"));
            parentId = sourceId.section(QLatin1Char('_'), 0, 0);
        }
        if (parentId == id) {
            // Fix clip
            if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl")).isEmpty()) {
                // Only set originalurl on master producer
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:originalurl"), fixedResource);
            }
            if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.resource")).isEmpty()) {
                // Only set original.resource on master producer
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:original.resource"), fixedResource);
            }
            if (m_missingProxyIds.contains(parentId)) {
                // Proxy is also missing, replace resource
                if (Xml::getXmlProperty(e, QStringLiteral("mlt_service")) == QLatin1String("timewarp")) {
                    Xml::setXmlProperty(e, QStringLiteral("warp_resource"), fixedResource);
                    Xml::setXmlProperty(e, QStringLiteral("resource"), Xml::getXmlProperty(e, QStringLiteral("warp_speed")) + ":" + fixedResource);
                } else {
                    Xml::setXmlProperty(e, QStringLiteral("resource"), fixedResource);
                }
            }
        }
        return true;
    };
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString id = child->data(0, idRole).toString();
        const QString fixedRes = child->text(1);
        for (int i = 0; i < producers.count(); ++i) {
            QDomElement e = producers.item(i).toElement();
            fixClip(e, id, fixedRes);
        }
        for (int i = 0; i < chains.count(); ++i) {
            QDomElement e = chains.item(i).toElement();
            fixClip(e, id, fixedRes);
        }
    }
}

void DocumentChecker::fixClipItem(QTreeWidgetItem *child, const QDomNodeList &producers, const QDomNodeList &chains, const QDomNodeList &trans,
                                  const QDomNodeList &filters)
{
    QDomElement e, property;
    QDomNodeList properties;
    int t = child->data(0, typeRole).toInt();
    QString id = child->data(0, idRole).toString();
    qDebug() << "==== FIXING PRODUCER WITH ID: " << id;
    if (t == SEQUENCE_ELEMENT) {
        // Already processed
        return;
    }
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString fixedResource = child->text(1);
        if (t == TITLE_IMAGE_ELEMENT) {
            // Title clips are not embeded in chains
            // edit images embedded in titles
            for (int i = 0; i < producers.count(); ++i) {
                e = producers.item(i).toElement();
                QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
                if (parentId.isEmpty()) {
                    // This is probably an old project file
                    QString sourceId = e.attribute(QStringLiteral("id"));
                    parentId = sourceId.section(QLatin1Char('_'), 0, 0);
                }
                if (parentId == id) {
                    // Fix clip
                    properties = e.childNodes();
                    for (int j = 0; j < properties.count(); ++j) {
                        property = properties.item(j).toElement();
                        if (property.attribute(QStringLiteral("name")) == QLatin1String("xmldata")) {
                            QString xml = property.firstChild().nodeValue();
                            xml.replace(child->data(0, typeOriginalResource).toString(), fixedResource);
                            property.firstChild().setNodeValue(xml);
                            break;
                        }
                    }
                }
            }
        } else {
            auto fixUrl = [this](QDomElement &e, const QString &id, const QString &fixedResource) {
                if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == id) {
                    // Fix clip
                    QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
                    QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
                    QString updatedResource = fixedResource;
                    qDebug() << "===== UPDATING RESOURCE FOR: " << id << ": " << resource << " > " << fixedResource;
                    if (service == QLatin1String("timewarp")) {
                        Xml::setXmlProperty(e, QStringLiteral("warp_resource"), updatedResource);
                        updatedResource.prepend(Xml::getXmlProperty(e, QStringLiteral("warp_speed")) + QLatin1Char(':'));
                    } else if (service.startsWith(QLatin1String("avformat")) && e.tagName() == QLatin1String("producer")) {
                        e.setTagName(QStringLiteral("chain"));
                    }
                    if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl")).isEmpty()) {
                        // Only set originalurl on master producer
                        Xml::setXmlProperty(e, QStringLiteral("kdenlive:originalurl"), fixedResource);
                    }
                    if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:original.resource")).isEmpty()) {
                        // Only set original.resource on master producer
                        Xml::setXmlProperty(e, QStringLiteral("kdenlive:original.resource"), fixedResource);
                    }
                    Xml::setXmlProperty(e, QStringLiteral("resource"), updatedResource);
                    QString proxy = Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy"));
                    if (proxy.length() > 1) {
                        // Disable proxy
                        Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
                    }
                }
                return true;
            };
            // edit clip url
            QString fixedResource = child->text(1);
            for (int i = 0; i < chains.count(); ++i) {
                QDomElement e = chains.item(i).toElement();
                fixUrl(e, id, fixedResource);
            }
            // Changing the tag name will remove the producer from the list, so we need to parse in reverse order
            for (int i = producers.count() - 1; i >= 0; --i) {
                QDomElement e = producers.item(i).toElement();
                fixUrl(e, id, fixedResource);
            }
        }
    } else if (child->data(0, statusRole).toInt() == CLIPPLACEHOLDER && t != TITLE_FONT_ELEMENT && t != TITLE_IMAGE_ELEMENT && t != SEQUENCE_ELEMENT) {
        // QString id = child->data(0, idRole).toString();
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == id) {
                // Fix clip
                Xml::setXmlProperty(e, QStringLiteral("_placeholder"), QStringLiteral("1"));
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:orig_service"), Xml::getXmlProperty(e, QStringLiteral("mlt_service")));
            }
        }
        for (int i = chains.count() - 1; i >= 0; --i) {
            // Changing the tag name will remove the chain from the list, so we need to parse in reverse order
            e = chains.item(i).toElement();
            if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == id) {
                // Fix clip
                Xml::setXmlProperty(e, QStringLiteral("_placeholder"), QStringLiteral("1"));
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:orig_service"), Xml::getXmlProperty(e, QStringLiteral("mlt_service")));

                // In MLT 7.14/15, link_swresample crashes on invalid avformat clips,
                // so switch to producer instead of chain to use filter_swresample
                e.setTagName(QStringLiteral("producer"));
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAOK) {
        QMap<QString, QString> lumaSearchPairs = getLumaPairs();
        QString luma;
        for (int i = 0; i < trans.count(); ++i) {
            QString service = Xml::getXmlProperty(trans.at(i).toElement(), QStringLiteral("mlt_service"));
            if (lumaSearchPairs.contains(service)) {
                luma = Xml::getXmlProperty(trans.at(i).toElement(), lumaSearchPairs.value(service));
                if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                    Xml::setXmlProperty(trans.at(i).toElement(), lumaSearchPairs.value(service), child->text(1));
                    // qCDebug(KDENLIVE_LOG) << "replace with; " << child->text(1);
                }
            }
        }
    } else if (child->data(0, statusRole).toInt() == ASSETOK) {
        QMap<QString, QString> assetSearchPairs = getAssetPairs();
        QString asset;
        for (int i = 0; i < filters.count(); ++i) {
            QString service = Xml::getXmlProperty(filters.at(i).toElement(), QStringLiteral("mlt_service"));

            if (assetSearchPairs.contains(service)) {
                asset = Xml::getXmlProperty(filters.at(i).toElement(), assetSearchPairs.value(service));
                if (!asset.isEmpty() && asset == child->data(0, idRole).toString()) {
                    Xml::setXmlProperty(filters.at(i).toElement(), assetSearchPairs.value(service), child->text(1));
                    // qCDebug(KDENLIVE_LOG) << "replace with; " << child->text(1);
                }
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
        QMap<QString, QString> lumaSearchPairs = getLumaPairs();
        QString luma;
        for (int i = 0; i < trans.count(); ++i) {
            QString service = Xml::getXmlProperty(trans.at(i).toElement(), QStringLiteral("mlt_service"));
            if (lumaSearchPairs.contains(service)) {
                luma = Xml::getXmlProperty(trans.at(i).toElement(), lumaSearchPairs.value(service));
                if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                    Xml::setXmlProperty(trans.at(i).toElement(), lumaSearchPairs.value(service), QString());
                }
            }
        }
    } else if (t == TITLE_FONT_ELEMENT) {
        // Parse all title producers
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
            // Fix clip
            if (service == QLatin1String("kdenlivetitle")) {
                QString xml = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
                QStringList fonts = TitleWidget::extractFontList(xml);
                bool updated = false;
                for (const auto &f : qAsConst(fonts)) {
                    if (m_missingFonts.contains(f)) {
                        updated = true;
                        QString replacementFont = QFontInfo(QFont(f)).family();
                        xml.replace(QString("font=\"%1\"").arg(f), QString("font=\"%1\"").arg(replacementFont));
                    }
                }
                if (updated) {
                    Xml::setXmlProperty(e, QStringLiteral("xmldata"), xml);
                }
            }
        }
    }
}

void DocumentChecker::slotPlaceholders()
{
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child != nullptr) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            child->setData(0, statusRole, CLIPPLACEHOLDER);
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            child->setData(0, statusRole, LUMAPLACEHOLDER);
        }
        child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));

        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    checkStatus();
}

void DocumentChecker::checkStatus()
{
    bool status = true;
    bool missingAssets = false;
    bool missingSource = false;
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child != nullptr) {
        int childStatus = child->data(0, statusRole).toInt();
        if (childStatus == CLIPMISSING) {
            status = false;
        } else if (childStatus == SOURCEMISSING) {
            missingSource = true;
        } else if (childStatus == LUMAMISSING || childStatus == ASSETMISSING) {
            missingAssets = true;
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    m_ui.recursiveSearch->setEnabled(!status || missingSource || missingAssets);
    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}

void DocumentChecker::slotDeleteSelected()
{
    if (KMessageBox::warningContinueCancel(m_dialog,
                                           i18np("This will remove the selected clip from this project",
                                                 "This will remove the selected clips from this project", m_ui.treeWidget->selectedItems().count()),
                                           i18n("Remove clips")) == KMessageBox::Cancel) {
        return;
    }
    QStringList deletedIds;
    QStringList deletedLumas;
    QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));

    for (QTreeWidgetItem *child : m_ui.treeWidget->selectedItems()) {
        int id = child->data(0, statusRole).toInt();
        if (id == CLIPMISSING) {
            deletedIds.append(child->data(0, idRole).toString());
            delete child;
        } else if (id == LUMAMISSING) {
            deletedLumas.append(child->data(0, idRole).toString());
            delete child;
        }
    }

    if (!deletedLumas.isEmpty()) {
        QDomElement e;
        QDomNodeList transitions = m_doc.elementsByTagName(QStringLiteral("transition"));
        QMap<QString, QString> lumaSearchPairs = getLumaPairs();
        for (const QString &lumaPath : deletedLumas) {
            for (int i = 0; i < transitions.count(); ++i) {
                e = transitions.item(i).toElement();
                QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
                QString resource;
                if (lumaSearchPairs.contains(service)) {
                    resource = Xml::getXmlProperty(e, lumaSearchPairs.value(service));
                }
                if (!resource.isEmpty() && resource == lumaPath) {
                    Xml::removeXmlProperty(e, lumaSearchPairs.value(service));
                }
            }
        }
    }

    if (!deletedIds.isEmpty()) {
        QDomElement e;
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            if (deletedIds.contains(Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")))) {
                // Mark clip for deletion
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:remove"), QStringLiteral("1"));
            }
        }

        for (int i = 0; i < playlists.count(); ++i) {
            QDomNodeList entries = playlists.at(i).toElement().elementsByTagName(QStringLiteral("entry"));
            for (int j = 0; j < entries.count(); ++j) {
                e = entries.item(j).toElement();
                if (deletedIds.contains(Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")))) {
                    // Mark clip for deletion
                    Xml::setXmlProperty(e, QStringLiteral("kdenlive:remove"), QStringLiteral("1"));
                }
            }
        }
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
        checkStatus();
    }
}

void DocumentChecker::checkMissingImagesAndFonts(const QStringList &images, const QStringList &fonts, const QString &id, const QString &baseClip)
{
    QDomDocument doc;
    for (const QString &img : images) {
        if (m_safeImages.contains(img)) {
            continue;
        }
        if (!QFile::exists(img)) {
            QDomElement e = doc.createElement(QStringLiteral("missingtitle"));
            e.setAttribute(QStringLiteral("type"), TITLE_IMAGE_ELEMENT);
            e.setAttribute(QStringLiteral("resource"), img);
            e.setAttribute(QStringLiteral("id"), id);
            e.setAttribute(QStringLiteral("name"), baseClip);
            QMap<QString, QString> properties;
            properties.insert("kdenlive:id", id);
            Xml::addXmlProperties(e, properties);
            m_missingClips.append(e);
        } else {
            m_safeImages.append(img);
        }
    }
    for (const QString &fontelement : fonts) {
        if (m_safeFonts.contains(fontelement) || m_missingFonts.contains(fontelement)) {
            continue;
        }
        QFont f(fontelement);
        ////qCDebug(KDENLIVE_LOG) << "/ / / CHK FONTS: " << fontelement << " = " << QFontInfo(f).family();
        if (fontelement != QFontInfo(f).family()) {
            m_missingFonts << fontelement;
        } else {
            m_safeFonts.append(fontelement);
        }
    }
}

void DocumentChecker::slotCheckButtons()
{
    if (m_ui.treeWidget->currentItem()) {
        QTreeWidgetItem *item = m_ui.treeWidget->currentItem();
        int t = item->data(0, typeRole).toInt();
        int s = item->data(0, statusRole).toInt();
        if (t == TITLE_FONT_ELEMENT || t == TITLE_IMAGE_ELEMENT || s == PROXYMISSING) {
            m_ui.removeSelected->setEnabled(false);
        } else {
            m_ui.removeSelected->setEnabled(true);
        }
        bool allowEdit = s == CLIPMISSING || s == LUMAMISSING;
        m_ui.manualSearch->setEnabled(allowEdit);
    }
}
