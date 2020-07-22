/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "documentchecker.h"
#include "bin/binplaylist.hpp"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "kthumb.h"
#include "titler/titlewidget.h"

#include <KMessageBox>
#include <KRecentDirs>
#include <KUrlRequesterDialog>
#include <klocalizedstring.h>

#include "kdenlive_debug.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QTreeWidgetItem>
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

enum MISSINGTYPE { TITLE_IMAGE_ELEMENT = 20, TITLE_FONT_ELEMENT = 21 };

DocumentChecker::DocumentChecker(QUrl url, const QDomDocument &doc)
    : m_url(std::move(url))
    , m_doc(doc)
    , m_dialog(nullptr)
{
}

QMap<QString, QString> DocumentChecker::getLumaPairs() const
{
    QMap<QString, QString> lumaSearchPairs;
    lumaSearchPairs.insert(QStringLiteral("luma"), QStringLiteral("resource"));
    lumaSearchPairs.insert(QStringLiteral("movit.luma_mix"), QStringLiteral("resource"));
    lumaSearchPairs.insert(QStringLiteral("composite"), QStringLiteral("luma"));
    lumaSearchPairs.insert(QStringLiteral("region"), QStringLiteral("composite.luma"));
    return lumaSearchPairs;
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
    // Check if strorage folder for temp files exists
    QString storageFolder;
    QDir projectDir(m_url.adjusted(QUrl::RemoveFilename).toLocalFile());
    QString documentid;
    QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
    for (int i = 0; i < playlists.count(); ++i) {
        if (playlists.at(i).toElement().attribute(QStringLiteral("id")) == BinPlaylist::binPlaylistId) {
            documentid = Xml::getXmlProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.documentid"));
            if (documentid.isEmpty()) {
                // invalid document id, recreate one
                documentid = QString::number(QDateTime::currentMSecsSinceEpoch());
                // TODO: Warn on invalid doc id
                Xml::setXmlProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.documentid"), documentid);
            }
            storageFolder = Xml::getXmlProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.storagefolder"));
            if (!storageFolder.isEmpty() && QFileInfo(storageFolder).isRelative()) {
                storageFolder.prepend(root);
            }
            if (!storageFolder.isEmpty() && !QFile::exists(storageFolder) && projectDir.exists(documentid)) {
                storageFolder = projectDir.absolutePath();
                Xml::setXmlProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.storagefolder"),
                                    projectDir.absoluteFilePath(documentid));
                m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
            }
            break;
        }
    }

    QDomNodeList documentProducers = m_doc.elementsByTagName(QStringLiteral("producer"));
    QDomElement profile = baseElement.firstChildElement(QStringLiteral("profile"));
    bool hdProfile = true;
    if (!profile.isNull()) {
        if (profile.attribute(QStringLiteral("width")).toInt() < 1000) {
            hdProfile = false;
        }
    }
    // List clips whose proxy is missing
    QList<QDomElement> missingProxies;
    // List clips who have a working proxy but no source clip
    QList<QDomElement> missingSources;
    m_safeImages.clear();
    m_safeFonts.clear();
    m_missingFonts.clear();
    max = documentProducers.count();
    QStringList verifiedPaths;
    QStringList missingPaths;
    QStringList serviceToCheck;
    serviceToCheck << QStringLiteral("kdenlivetitle") << QStringLiteral("qimage") << QStringLiteral("pixbuf") << QStringLiteral("timewarp")
                   << QStringLiteral("framebuffer") << QStringLiteral("xml") << QStringLiteral("qtext");
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentProducers.item(i).toElement();
        QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        if (!service.startsWith(QLatin1String("avformat")) && !serviceToCheck.contains(service)) {
            continue;
        }
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
                        }
                        else if (resource.endsWith(QLatin1String(".kdenlivetitle"))) {
                            Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("kdenlivetitle"));
                        } else if (resource.endsWith(QLatin1String(".kdenlive")) || resource.endsWith(QLatin1String(".mlt"))) {
                            Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("xml"));
                        } else {
                            Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
                        }
                    }
                }
                continue;
            }

            checkMissingImagesAndFonts(QStringList(), QStringList(Xml::getXmlProperty(e, QStringLiteral("family"))), e.attribute(QStringLiteral("id")),
                                       e.attribute(QStringLiteral("name")));
            continue;
        }
        if (service == QLatin1String("kdenlivetitle")) {
            // TODO: Check is clip template is missing (xmltemplate) or hash changed
            QString xml = Xml::getXmlProperty(e, QStringLiteral("xmldata"));
            QStringList images = TitleWidget::extractImageList(xml);
            QStringList fonts = TitleWidget::extractFontList(xml);
            checkMissingImagesAndFonts(images, fonts, e.attribute(QStringLiteral("id")), e.attribute(QStringLiteral("name")));
            continue;
        }
        QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
        if (resource.isEmpty()) {
            continue;
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
                m_missingClips.append(e);
            }
            continue;
        }

        QString proxy = Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy"));
        if (proxy.length() > 1) {
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
                        fixProxyClip(e.attribute(QStringLiteral("id")), Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy")), updatedPath,
                                     documentProducers);
                        fixed = true;
                    }
                }
                if (!fixed) {
                    missingProxies.append(e);
                }
            }
            QString original = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
            if (QFileInfo(original).isRelative()) {
                original.prepend(root);
            }
            // Check for slideshows
            bool slideshow = original.contains(QStringLiteral("/.all.")) || original.contains(QLatin1Char('?')) || original.contains(QLatin1Char('%'));
            if (slideshow && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
                original = QFileInfo(original).absolutePath();
            }
            if (!QFile::exists(original)) {
                // clip has proxy but original clip is missing
                missingSources.append(e);
                missingPaths.append(original);
            }
            verifiedPaths.append(resource);
            continue;
        }
        // Check for slideshows
        bool slideshow = resource.contains(QStringLiteral("/.all.")) || resource.contains(QLatin1Char('?')) || resource.contains(QLatin1Char('%'));
        if (slideshow) {
            if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
                resource = QFileInfo(resource).absolutePath();
            } else if (service.startsWith(QLatin1String("avformat")) && Xml::hasXmlProperty(e, QStringLiteral("ttl"))) {
                // Fix MLT 6.20 avformat slideshows
                if (service.startsWith(QLatin1String("avformat"))) {
                    Xml::setXmlProperty(e, QStringLiteral("mlt_service"), QStringLiteral("qimage"));
                }
                resource = QFileInfo(resource).absolutePath();
            }
        }
        if (!QFile::exists(resource)) {
            // Missing clip found, make sure to omit timeline preview
            if (QFileInfo(resource).absolutePath().endsWith(QString("/%1/preview").arg(documentid))) {
                // This is a timeline preview missing chunk, ignore
            } else {
                m_missingClips.append(e);
                missingPaths.append(resource);
            }
        }
        // Make sure we don't query same path twice
        verifiedPaths.append(resource);
    }

    // Get list of used Luma files
    QStringList missingLumas;
    QStringList filesToCheck;
    QString filePath;
    QMap<QString, QString> lumaSearchPairs = getLumaPairs();

    QDomNodeList trans = m_doc.elementsByTagName(QStringLiteral("transition"));
    max = trans.count();
    for (int i = 0; i < max; ++i) {
        QDomElement transition = trans.at(i).toElement();
        QString service = getProperty(transition, QStringLiteral("mlt_service"));
        QString luma;
        if (lumaSearchPairs.contains(service)) {
            luma = getProperty(transition, lumaSearchPairs.value(service));
        }
        if (!luma.isEmpty() && !filesToCheck.contains(luma)) {
            filesToCheck.append(luma);
        }
    }

    QMap<QString, QString> autoFixLuma;
    QString lumaPath;
    QString lumaMltPath;
    // Check existence of luma files
    for (const QString &lumafile : filesToCheck) {
        filePath = lumafile;
        if (QFileInfo(filePath).isRelative()) {
            filePath.prepend(root);
        }
        if (!QFile::exists(filePath)) {
            QString lumaName = filePath.section(QLatin1Char('/'), -1);
            // check if this was an old format luma, not in correct folder
            QString fixedLuma = filePath.section(QLatin1Char('/'), 0, -2);
            lumaName.prepend(hdProfile ? QStringLiteral("/HD/") : QStringLiteral("/PAL/"));
            fixedLuma.append(lumaName);
            if (QFile::exists(fixedLuma)) {
                // Auto replace pgm with png for lumas
                autoFixLuma.insert(filePath, fixedLuma);
                continue;
            }
            // Check Kdenlive folder
            if (lumaPath.isEmpty()) {
                QDir dir(QCoreApplication::applicationDirPath());
                dir.cdUp();
                dir.cd(QStringLiteral("share/kdenlive/lumas/"));
                lumaPath = dir.absolutePath() + QStringLiteral("/");
            }
            lumaName = filePath.section(QLatin1Char('/'), -2);
            lumaName.prepend(lumaPath);
            if (QFile::exists(lumaName)) {
                autoFixLuma.insert(filePath, lumaName);
                continue;
            }
            // Check MLT folder
            if (lumaMltPath.isEmpty()) {
                QDir dir(KdenliveSettings::mltpath());
                dir.cd(QStringLiteral("../lumas/"));
                lumaMltPath = dir.absolutePath() + QStringLiteral("/");
            }
            lumaName = filePath.section(QLatin1Char('/'), -2);
            lumaName.prepend(lumaMltPath);
            if (QFile::exists(lumaName)) {
                autoFixLuma.insert(filePath, lumaName);
                continue;
            }

            if (filePath.endsWith(QLatin1String(".pgm"))) {
                fixedLuma = filePath.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".png");
            } else if (filePath.endsWith(QLatin1String(".png"))) {
                fixedLuma = filePath.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".pgm");
            }
            if (!fixedLuma.isEmpty() && QFile::exists(fixedLuma)) {
                // Auto replace pgm with png for lumas
                autoFixLuma.insert(filePath, fixedLuma);
            } else {
                missingLumas.append(lumafile);
            }
        }
    }
    if (!autoFixLuma.isEmpty()) {
        for (int i = 0; i < max; ++i) {
            QDomElement transition = trans.at(i).toElement();
            QString service = getProperty(transition, QStringLiteral("mlt_service"));
            QString luma;
            if (lumaSearchPairs.contains(service)) {
                luma = getProperty(transition, lumaSearchPairs.value(service));
            }
            if (!luma.isEmpty() && autoFixLuma.contains(luma)) {
                updateProperty(transition, lumaSearchPairs.value(service), autoFixLuma.value(luma));
            }
        }
    }
    // Check for missing effects
    QDomNodeList effs = m_doc.elementsByTagName(QStringLiteral("filter"));
    max = effs.count();
    QStringList filters;
    for (int i = 0; i < max; ++i) {
        QDomElement transition = effs.at(i).toElement();
        QString service = getProperty(transition, QStringLiteral("kdenlive_id"));
        if (service.isEmpty()) {
            service = getProperty(transition, QStringLiteral("mlt_service"));
        }
        filters << service;
    }
    QStringList processed;
    for (const QString &id : qAsConst(filters)) {
        if (!processed.contains(id) && !EffectsRepository::get()->exists(id)) {
            m_missingFilters << id;
        }
        processed << id;
    }

    if (!m_missingFilters.isEmpty()) {
        // Delete missing effects
        for (int i = 0; i < effs.count(); ++i) {
            QDomElement e = effs.item(i).toElement();
            if (m_missingFilters.contains(getProperty(e, QStringLiteral("kdenlive_id")))) {
                // Remove clip
                e.parentNode().removeChild(e);
                --i;
            }
        }
    }
    if (m_missingClips.isEmpty() && missingLumas.isEmpty() && missingProxies.isEmpty() && missingSources.isEmpty() && m_missingFonts.isEmpty() &&
        m_missingFilters.isEmpty()) {
        return false;
    }

    m_dialog = new QDialog();
    m_dialog->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_ui.setupUi(m_dialog);

    for (const QString &l : missingLumas) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Luma file") << l);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        item->setData(0, idRole, l);
        item->setData(0, statusRole, LUMAMISSING);
    }
    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_missingClips.isEmpty() && missingProxies.isEmpty() && missingSources.isEmpty());
    max = m_missingClips.count();
    m_missingProxyIds.clear();
    QStringList processedIds;
    for (int i = 0; i < max; ++i) {
        QDomElement e = m_missingClips.at(i).toElement();
        QString clipType;
        ClipType::ProducerType type;
        int status = CLIPMISSING;
        const QString service = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
        QString resource =
            service == QLatin1String("timewarp") ? Xml::getXmlProperty(e, QStringLiteral("warp_resource")) : Xml::getXmlProperty(e, QStringLiteral("resource"));
        bool slideshow = resource.contains(QStringLiteral("/.all.")) || resource.contains(QLatin1Char('?')) || resource.contains(QLatin1Char('%'));
        if (service.startsWith(QLatin1String("avformat")) || service == QLatin1String("framebuffer") ||
            service == QLatin1String("timewarp")) {
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
                continue;
            }
            processedIds << clipId;
        } else {
            // Older project file format
            clipId = e.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0);
            if (processedIds.contains(clipId)) {
                continue;
            }
            processedIds << clipId;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPMISSING);
        item->setData(0, clipTypeRole, (int)type);
        item->setData(0, idRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")));
        item->setToolTip(0, i18n("Missing item"));

        if (status == TITLE_IMAGE_ELEMENT) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
            item->setToolTip(1, e.attribute(QStringLiteral("name")));
            QString imageResource = e.attribute(QStringLiteral("resource"));
            item->setData(0, typeRole, status);
            item->setData(0, typeOriginalResource, e.attribute(QStringLiteral("resource")));
            if (!m_rootReplacement.first.isEmpty()) {
                if (imageResource.startsWith(m_rootReplacement.first)) {
                    imageResource.replace(m_rootReplacement.first, m_rootReplacement.second);
                    if (QFile::exists(imageResource)) {
                        item->setData(0, statusRole, CLIPOK);
                        item->setToolTip(0, i18n("Relocated item"));
                    }
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
                if (resource.startsWith(m_rootReplacement.first)) {
                    resource.replace(m_rootReplacement.first, m_rootReplacement.second);
                    if (QFile::exists(resource)) {
                        item->setData(0, statusRole, CLIPOK);
                        item->setToolTip(0, i18n("Relocated item"));
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
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
        QString newft = QFontInfo(QFont(font)).family();
        item->setText(1, i18n("%1 will be replaced by %2", font, newft));
        item->setData(0, typeRole, TITLE_FONT_ELEMENT);
    }

    QString infoLabel;
    if (!m_missingClips.isEmpty()) {
        infoLabel = i18n("The project file contains missing clips or files.");
    }
    if (!m_missingFilters.isEmpty()) {
        if (!infoLabel.isEmpty()) {
            infoLabel.append(QStringLiteral("\n"));
        }
        infoLabel.append(i18np("Missing effect: %2 will be removed from project.", "Missing effects: %2 will be removed from project.",
                               m_missingFilters.count(), m_missingFilters.join(",")));
    }
    if (!missingProxies.isEmpty()) {
        if (!infoLabel.isEmpty()) {
            infoLabel.append(QStringLiteral("\n"));
        }
        infoLabel.append(i18n("Missing proxies can be recreated on opening."));
        m_ui.rebuildProxies->setChecked(true);
        connect(m_ui.rebuildProxies, &QCheckBox::stateChanged, [missingProxies] (int state) {
            for (QDomElement e : missingProxies) {
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
    if (!missingSources.isEmpty()) {
        if (!infoLabel.isEmpty()) {
            infoLabel.append(QStringLiteral("\n"));
        }
        infoLabel.append(i18np("The project file contains a missing clip, you can still work with its proxy.",
                               "The project file contains %1 missing clips, you can still work with their proxies.", missingSources.count()));
    }
    if (!infoLabel.isEmpty()) {
        m_ui.infoLabel->setText(infoLabel);
    } else {
        m_ui.infoLabel->setVisible(false);
    }

    m_ui.removeSelected->setEnabled(!m_missingClips.isEmpty());
    m_ui.recursiveSearch->setEnabled(!m_missingClips.isEmpty() || !missingLumas.isEmpty() || !missingSources.isEmpty());
    m_ui.usePlaceholders->setEnabled(!m_missingClips.isEmpty());

    // Check missing proxies
    max = missingProxies.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Proxy clip"));
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
        item->setText(
            1, i18np("%1 missing proxy clip, will be recreated on project opening", "%1 missing proxy clips, will be recreated on project opening", max));
        // item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, PROXYMISSING);
        item->setToolTip(0, i18n("Missing proxy"));
    }

    for (int i = 0; i < max; ++i) {
        QDomElement e = missingProxies.at(i).toElement();
        QString realPath = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
        QString id = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        m_missingProxyIds << id;
        // Tell Kdenlive to recreate proxy
        e.setAttribute(QStringLiteral("_replaceproxy"), QStringLiteral("1"));
        // Remove reference to missing proxy
        Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
        // Replace proxy url with real clip in MLT producers
        QDomElement mltProd;
        int prodsCount = documentProducers.count();
        for (int j = 0; j < prodsCount; ++j) {
            mltProd = documentProducers.at(j).toElement();
            QString parentId = Xml::getXmlProperty(mltProd, QStringLiteral("kdenlive:id"));
            bool slowmotion = false;
            if (parentId.startsWith(QLatin1String("slowmotion"))) {
                slowmotion = true;
                parentId = parentId.section(QLatin1Char(':'), 1, 1);
            }
            if (parentId.contains(QLatin1Char('_'))) {
                parentId = parentId.section(QLatin1Char('_'), 0, 0);
            }
            if (parentId == id) {
                // Hit, we must replace url
                QString suffix;
                if (slowmotion) {
                    QString resource = Xml::getXmlProperty(mltProd, QStringLiteral("resource"));
                    suffix = QLatin1Char('?') + resource.section(QLatin1Char('?'), -1);
                }
                Xml::setXmlProperty(mltProd, QStringLiteral("resource"), realPath + suffix);
                Xml::setXmlProperty(mltProd, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
                if (missingPaths.contains(realPath)) {
                    // Proxy AND source missing
                    setProperty(mltProd, QStringLiteral("_placeholder"), QStringLiteral("1"));
                    setProperty(mltProd, QStringLiteral("kdenlive:orig_service"), Xml::getXmlProperty(mltProd, "mlt_service"));
                }
            }
        }
    }

    if (max > 0) {
        // original doc was modified
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);
    }

    // Check clips with available proxies but missing original source clips
    max = missingSources.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Source clip"));
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-warning")));
        item->setText(1, i18n("%1 missing source clips, you can only use the proxies", max));
        // item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, SOURCEMISSING);
        item->setToolTip(0, i18n("Missing source clip"));
        for (int i = 0; i < max; ++i) {
            QDomElement e = missingSources.at(i).toElement();
            QString realPath = Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl"));
            // Tell Kdenlive the source is missing
            e.setAttribute(QStringLiteral("_missingsource"), QStringLiteral("1"));
            QTreeWidgetItem *subitem = new QTreeWidgetItem(item, QStringList() << i18n("Source clip"));
            // qCDebug(KDENLIVE_LOG)<<"// Adding missing source clip: "<<realPath;
            subitem->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
            subitem->setText(1, realPath);
            subitem->setData(0, hashRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_hash")));
            subitem->setData(0, sizeRole, Xml::getXmlProperty(e, QStringLiteral("kdenlive:file_size")));
            subitem->setData(0, statusRole, CLIPMISSING);
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
    connect(m_ui.recursiveSearch, &QAbstractButton::pressed, this, &DocumentChecker::slotSearchClips);
    connect(m_ui.usePlaceholders, &QAbstractButton::pressed, this, &DocumentChecker::slotPlaceholders);
    connect(m_ui.removeSelected, &QAbstractButton::pressed, this, &DocumentChecker::slotDeleteSelected);
    connect(m_ui.treeWidget, &QTreeWidget::itemDoubleClicked, this, &DocumentChecker::slotEditItem);
    connect(m_ui.treeWidget, &QTreeWidget::itemSelectionChanged, this, &DocumentChecker::slotCheckButtons);
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

QString DocumentChecker::getProperty(const QDomElement &effect, const QString &name)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            return e.firstChild().nodeValue();
        }
    }
    return QString();
}

void DocumentChecker::updateProperty(const QDomElement &effect, const QString &name, const QString &value)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            e.firstChild().setNodeValue(value);
            break;
        }
    }
}

void DocumentChecker::setProperty(QDomElement &effect, const QString &name, const QString &value)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    bool found = false;
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            e.firstChild().setNodeValue(value);
            found = true;
            break;
        }
    }

    if (!found) {
        // create property
        QDomDocument doc = effect.ownerDocument();
        QDomElement e = doc.createElement(QStringLiteral("property"));
        e.setAttribute(QStringLiteral("name"), name);
        QDomText val = doc.createTextNode(value);
        e.appendChild(val);
        effect.appendChild(e);
    }
}

void DocumentChecker::slotSearchClips()
{
    // QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    QString clipFolder = m_url.adjusted(QUrl::RemoveFilename).toLocalFile();
    QString newpath = QFileDialog::getExistingDirectory(qApp->activeWindow(), i18n("Clips folder"), clipFolder);
    if (newpath.isEmpty()) {
        return;
    }
    int ix = 0;
    bool fixed = false;
    m_ui.recursiveSearch->setChecked(true);
    // TODO: make non modal
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    QDir searchDir(newpath);
    while (child != nullptr) {
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
                }
            }
        } else if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            bool perfectMatch = true;
            ClipType::ProducerType type = (ClipType::ProducerType)child->data(0, clipTypeRole).toInt();
            QString clipPath;
            if (type != ClipType::SlideShow) {
                // Slideshows cannot be found with hash / size
                clipPath = searchFileRecursively(searchDir, child->data(0, sizeRole).toString(), child->data(0, hashRole).toString(), child->text(1));
            }
            if (clipPath.isEmpty()) {
                clipPath = searchPathRecursively(searchDir, QUrl::fromLocalFile(child->text(1)).fileName(), type);
                perfectMatch = false;
            }
            if (!clipPath.isEmpty()) {
                fixed = true;
                child->setText(1, clipPath);
                child->setIcon(0, perfectMatch ? QIcon::fromTheme(QStringLiteral("dialog-ok")) : QIcon::fromTheme(QStringLiteral("dialog-warning")));
                child->setData(0, statusRole, CLIPOK);
            }
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            QString fileName = searchLuma(searchDir, child->data(0, idRole).toString());
            if (!fileName.isEmpty()) {
                fixed = true;
                child->setText(1, fileName);
                child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
                child->setData(0, statusRole, LUMAOK);
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
    checkStatus();
}

QString DocumentChecker::searchLuma(const QDir &dir, const QString &file) const
{
    QDir searchPath(KdenliveSettings::mltpath());
    QString fname = QUrl::fromLocalFile(file).fileName();
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
    searchPath.cd(QStringLiteral("data/lumas"));
#else
    searchPath.cd(QStringLiteral("../share/apps/kdenlive/lumas"));
#endif
    result.setFile(searchPath, fname);
    if (result.exists()) {
        return result.filePath();
    }
    // Try in Kdenlive's standard KDE path
    QString res = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("lumas/") + fname);
    if (!res.isEmpty()) {
        return res;
    }
    // Try in user's chosen folder
    return searchPathRecursively(dir, fname);
}

QString DocumentChecker::searchPathRecursively(const QDir &dir, const QString &fileName, ClipType::ProducerType type) const
{
    QString foundFileName;
    QStringList filters;
    if (type == ClipType::SlideShow) {
        if (fileName.contains(QLatin1Char('%'))) {
            filters << fileName.section(QLatin1Char('%'), 0, -2) + QLatin1Char('*');
        } else {
            return QString();
        }
    } else {
        filters << fileName;
    }
    QDir searchDir(dir);
    searchDir.setNameFilters(filters);
    QStringList filesAndDirs = searchDir.entryList(QDir::Files | QDir::Readable);
    if (!filesAndDirs.isEmpty()) {
        // File Found
        if (type == ClipType::SlideShow) {
            return searchDir.absoluteFilePath(fileName);
        }
        return searchDir.absoluteFilePath(filesAndDirs.at(0));
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

QString DocumentChecker::searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash, const QString &fileName) const
{
    if (matchSize.isEmpty() && matchHash.isEmpty()) {
        return searchPathRecursively(dir, QUrl::fromLocalFile(fileName).fileName());
    }
    QString foundFileName;
    QByteArray fileData;
    QByteArray fileHash;
    QStringList filesAndDirs = dir.entryList(QDir::Files | QDir::Readable);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); ++i) {
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

void DocumentChecker::slotEditItem(QTreeWidgetItem *item, int)
{
    int t = item->data(0, typeRole).toInt();
    if (t == TITLE_FONT_ELEMENT) {
        return;
    }
    //|| t == TITLE_IMAGE_ELEMENT) {

    QUrl url = KUrlRequesterDialog::getUrl(QUrl::fromLocalFile(item->text(1)), m_dialog, i18n("Enter new location for file"));
    if (!url.isValid()) {
        return;
    }
    item->setText(1, url.toLocalFile());
    ClipType::ProducerType type = (ClipType::ProducerType)item->data(0, clipTypeRole).toInt();
    bool fixed = false;
    if (type == ClipType::SlideShow && QFile::exists(url.adjusted(QUrl::RemoveFilename).toLocalFile())) {
        fixed = true;
    }
    if (fixed || QFile::exists(url.toLocalFile())) {
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) {
            item->setData(0, statusRole, CLIPOK);
        } else {
            item->setData(0, statusRole, LUMAOK);
        }
        checkStatus();
    } else {
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) {
            item->setData(0, statusRole, CLIPMISSING);
        } else {
            item->setData(0, statusRole, LUMAMISSING);
        }
        checkStatus();
    }
}

void DocumentChecker::acceptDialog()
{
    QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
    int ix = 0;

    // prepare transitions
    QDomNodeList trans = m_doc.elementsByTagName(QStringLiteral("transition"));

    // prepare filters
    QDomNodeList filters = m_doc.elementsByTagName(QStringLiteral("filter"));

    // Mark document as modified
    m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);

    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child != nullptr) {
        if (child->data(0, statusRole).toInt() == SOURCEMISSING) {
            for (int j = 0; j < child->childCount(); ++j) {
                fixSourceClipItem(child->child(j), producers);
            }
        } else {
            fixClipItem(child, producers, trans);
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    // QDialog::accept();
}

void DocumentChecker::fixProxyClip(const QString &id, const QString &oldUrl, const QString &newUrl, const QDomNodeList &producers)
{
    QDomElement e, property;
    QDomNodeList properties;
    for (int i = 0; i < producers.count(); ++i) {
        e = producers.item(i).toElement();
        QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
        if (parentId.isEmpty()) {
            // This is probably an old project file
            QString sourceId = e.attribute(QStringLiteral("id"));
            parentId = sourceId.section(QLatin1Char('_'), 0, 0);
            if (parentId.startsWith(QLatin1String("slowmotion"))) {
                parentId = parentId.section(QLatin1Char(':'), 1, 1);
            }
        }
        if (parentId == id) {
            // Fix clip
            QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
            // TODO: Slowmmotion clips
            if (resource.contains(QRegExp(QStringLiteral("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))) {
                // fixedResource.append(QLatin1Char('?') + resource.section(QLatin1Char('?'), -1));
            }
            if (resource == oldUrl) {
                Xml::setXmlProperty(e, QStringLiteral("resource"), newUrl);
            }
            if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:proxy")).isEmpty()) {
                // Only set originalurl on master producer
                Xml::setXmlProperty(e, QStringLiteral("kdenlive:proxy"), newUrl);
            }
        }
    }
}

void DocumentChecker::fixSourceClipItem(QTreeWidgetItem *child, const QDomNodeList &producers)
{
    QDomElement e, property;
    QDomNodeList properties;
    // int t = child->data(0, typeRole).toInt();
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString id = child->data(0, idRole).toString();
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
            if (parentId.isEmpty()) {
                // This is probably an old project file
                QString sourceId = e.attribute(QStringLiteral("id"));
                parentId = sourceId.section(QLatin1Char('_'), 0, 0);
                if (parentId.startsWith(QLatin1String("slowmotion"))) {
                    parentId = parentId.section(QLatin1Char(':'), 1, 1);
                }
            }
            if (parentId == id) {
                // Fix clip
                QString resource = Xml::getXmlProperty(e, QStringLiteral("resource"));
                QString fixedResource = child->text(1);
                if (resource.contains(QRegExp(QStringLiteral("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))) {
                    fixedResource.append(QLatin1Char('?') + resource.section(QLatin1Char('?'), -1));
                }
                if (!Xml::getXmlProperty(e, QStringLiteral("kdenlive:originalurl")).isEmpty()) {
                    // Only set originalurl on master producer
                    Xml::setXmlProperty(e, QStringLiteral("kdenlive:originalurl"), fixedResource);
                }
                if (m_missingProxyIds.contains(parentId)) {
                    // Proxy is also missing, replace resource
                    Xml::setXmlProperty(e, QStringLiteral("resource"), fixedResource);
                }
            }
        }
    }
}

void DocumentChecker::fixClipItem(QTreeWidgetItem *child, const QDomNodeList &producers, const QDomNodeList &trans)
{
    QDomElement e, property;
    QDomNodeList properties;
    int t = child->data(0, typeRole).toInt();
    QString id = child->data(0, idRole).toString();
    qDebug()<<"==== FIXING PRODUCER WITH ID: "<<id;
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString fixedResource = child->text(1);
        if (t == TITLE_IMAGE_ELEMENT) {
            // edit images embedded in titles
            for (int i = 0; i < producers.count(); ++i) {
                e = producers.item(i).toElement();
                QString parentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
                if (parentId.isEmpty()) {
                    // This is probably an old project file
                    QString sourceId = e.attribute(QStringLiteral("id"));
                    parentId = sourceId.section(QLatin1Char('_'), 0, 0);
                    if (parentId.startsWith(QLatin1String("slowmotion"))) {
                        parentId = parentId.section(QLatin1Char(':'), 1, 1);
                    }
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
            // edit clip url
            /*for (int i = 0; i < infoproducers.count(); ++i) {
                e = infoproducers.item(i).toElement();
                if (e.attribute("id") == id) {
                    // Fix clip
                    e.setAttribute("resource", child->text(1));
                    e.setAttribute("name", QUrl(child->text(1)).fileName());
                    e.removeAttribute("_missingsource");
                    break;
                }
            }*/
            for (int i = 0; i < producers.count(); ++i) {
                e = producers.item(i).toElement();
                if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == id) {
                    // Fix clip
                    QString resource = getProperty(e, QStringLiteral("resource"));
                    QString service = getProperty(e, QStringLiteral("mlt_service"));
                    QString updatedResource = fixedResource;
                    qDebug()<<"===== UPDATING RESOURCE FOR: "<<id<<": "<<resource<<" > "<<fixedResource;
                    if (resource.contains(QRegExp(QStringLiteral("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))) {
                        updatedResource.append(QLatin1Char('?') + resource.section(QLatin1Char('?'), -1));
                    }
                    if (service == QLatin1String("timewarp")) {
                        updateProperty(e, QStringLiteral("warp_resource"), updatedResource);
                        updatedResource.prepend(getProperty(e, QStringLiteral("warp_speed")) + QLatin1Char(':'));
                    }
                    updateProperty(e, QStringLiteral("resource"), updatedResource);
                }
            }
        }
    } else if (child->data(0, statusRole).toInt() == CLIPPLACEHOLDER && t != TITLE_FONT_ELEMENT && t != TITLE_IMAGE_ELEMENT) {
        // QString id = child->data(0, idRole).toString();
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            if (Xml::getXmlProperty(e, QStringLiteral("kdenlive:id")) == id) {
                // Fix clip
                setProperty(e, QStringLiteral("_placeholder"), QStringLiteral("1"));
                setProperty(e, QStringLiteral("kdenlive:orig_service"), getProperty(e, QStringLiteral("mlt_service")));
                break;
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAOK) {
        QMap<QString, QString> lumaSearchPairs = getLumaPairs();
        for (int i = 0; i < trans.count(); ++i) {
            QString service = getProperty(trans.at(i).toElement(), QStringLiteral("mlt_service"));
            QString luma;
            if (lumaSearchPairs.contains(service)) {
                luma = getProperty(trans.at(i).toElement(), lumaSearchPairs.value(service));
            }
            if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                updateProperty(trans.at(i).toElement(), lumaSearchPairs.value(service), child->text(1));
                // qCDebug(KDENLIVE_LOG) << "replace with; " << child->text(1);
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
        QMap<QString, QString> lumaSearchPairs = getLumaPairs();
        for (int i = 0; i < trans.count(); ++i) {
            QString service = getProperty(trans.at(i).toElement(), QStringLiteral("mlt_service"));
            QString luma;
            if (lumaSearchPairs.contains(service)) {
                luma = getProperty(trans.at(i).toElement(), lumaSearchPairs.value(service));
            }
            if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                updateProperty(trans.at(i).toElement(), lumaSearchPairs.value(service), QString());
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
            child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            child->setData(0, statusRole, LUMAPLACEHOLDER);
            child->setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    checkStatus();
}

void DocumentChecker::checkStatus()
{
    bool status = true;
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child != nullptr) {
        int childStatus = child->data(0, statusRole).toInt();
        if (childStatus == CLIPMISSING) {
            status = false;
            break;
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
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
                    resource = getProperty(e, lumaSearchPairs.value(service));
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

        QDomNode mlt = m_doc.elementsByTagName(QStringLiteral("mlt")).at(0);

        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            if (deletedIds.contains(e.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0)) ||
                deletedIds.contains(e.attribute(QStringLiteral("id")).section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0))) {
                // Remove clip
                mlt.removeChild(e);
                --i;
            }
        }

        for (int i = 0; i < playlists.count(); ++i) {
            QDomNodeList entries = playlists.at(i).toElement().elementsByTagName(QStringLiteral("entry"));
            for (int j = 0; j < entries.count(); ++j) {
                e = entries.item(j).toElement();
                if (deletedIds.contains(e.attribute(QStringLiteral("producer")).section(QLatin1Char('_'), 0, 0)) ||
                    deletedIds.contains(e.attribute(QStringLiteral("producer")).section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0))) {
                    // Replace clip with blank
                    while (e.childNodes().count() > 0) {
                        e.removeChild(e.firstChild());
                    }
                    e.setTagName(QStringLiteral("blank"));
                    e.removeAttribute(QStringLiteral("producer"));
                    int length = e.attribute(QStringLiteral("out")).toInt() - e.attribute(QStringLiteral("in")).toInt();
                    e.setAttribute(QStringLiteral("length"), length);
                    j--;
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
            m_missingClips.append(e);
        } else {
            m_safeImages.append(img);
        }
    }
    for (const QString &fontelement : fonts) {
        if (m_safeFonts.contains(fontelement)) {
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
    }
}
