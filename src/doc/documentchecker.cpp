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
#include "kthumb.h"

#include "titler/titlewidget.h"
#include "kdenlivesettings.h"
#include "utils/KoIconUtils.h"

#include <KUrlRequesterDialog>
#include <KMessageBox>
#include <klocalizedstring.h>
#include <KRecentDirs>

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QTreeWidgetItem>
#include <QFile>
#include <QFileDialog>
#include <QCryptographicHash>
#include <QStandardPaths>

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

enum TITLECLIPTYPE { TITLE_IMAGE_ELEMENT = 20, TITLE_FONT_ELEMENT = 21 };

DocumentChecker::DocumentChecker(const QUrl &url, const QDomDocument &doc):
    m_url(url), m_doc(doc), m_dialog(nullptr)
{
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
        }
        else {
            root = QDir::cleanPath(root) + QDir::separator();
        }
    }
    // Check if strorage folder for temp files exists
    QString storageFolder;
    QDir projectDir(m_url.adjusted(QUrl::RemoveFilename).toLocalFile());
    QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));
    for (int i = 0; i < playlists.count(); ++i) {
        if (playlists.at(i).toElement().attribute(QStringLiteral("id")) == QStringLiteral("main bin")) {
            QString documentid = EffectsList::property(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.documentid"));
            if (documentid.isEmpty()) {
                // invalid document id, recreate one
                documentid = QString::number(QDateTime::currentMSecsSinceEpoch());
                //TODO: Warn on invalid doc id
                EffectsList::setProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.documentid"), documentid);
            }
            storageFolder = EffectsList::property(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.storagefolder"));
            if (!storageFolder.isEmpty() && QFileInfo(storageFolder).isRelative()) {
                storageFolder.prepend(root);
            }
            if (!storageFolder.isEmpty() && !QFile::exists(storageFolder) && projectDir.exists( documentid)) {
                storageFolder = projectDir.absolutePath();
                EffectsList::setProperty(playlists.at(i).toElement(), QStringLiteral("kdenlive:docproperties.storagefolder"), projectDir.absoluteFilePath(documentid));
                m_doc.documentElement().setAttribute(QStringLiteral("modified"), QStringLiteral("1"));
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
    QStringList serviceToCheck;
    serviceToCheck << QStringLiteral("kdenlivetitle") << QStringLiteral("qimage") << QStringLiteral("pixbuf") << QStringLiteral("timewarp") << QStringLiteral("framebuffer") << QStringLiteral("xml");
    for (int i = 0; i < max; ++i) {
        QDomElement e = documentProducers.item(i).toElement();
        QString service = EffectsList::property(e, QStringLiteral("mlt_service"));
        if (!service.startsWith(QLatin1String("avformat")) && !serviceToCheck.contains(service)) {
            continue;
        }
        if (service == QLatin1String("qtext")) {
            checkMissingImagesAndFonts(QStringList(), QStringList(EffectsList::property(e, QStringLiteral("family"))),
                                       e.attribute(QStringLiteral("id")), e.attribute(QStringLiteral("name")));
            continue;
        }
        if (service == QLatin1String("kdenlivetitle")) {
            //TODO: Check is clip template is missing (xmltemplate) or hash changed
            QString xml = EffectsList::property(e, QStringLiteral("xmldata"));
            QStringList images = TitleWidget::extractImageList(xml);
            QStringList fonts = TitleWidget::extractFontList(xml);
            checkMissingImagesAndFonts(images, fonts, e.attribute(QStringLiteral("id")), e.attribute(QStringLiteral("name")));
            continue;
        }
        QString resource = EffectsList::property(e, QStringLiteral("resource"));
        if (resource.isEmpty()) {
            continue;
        }
        if (service == QLatin1String("timewarp")) {
            //slowmotion clip, trim speed info
            resource = EffectsList::property(e, QStringLiteral("warp_resource"));
        } else if (service == QLatin1String("framebuffer")) {
            //slowmotion clip, trim speed info
            resource = resource.section(QLatin1Char('?'), 0, 0);
        }

        // Make sure to have absolute paths
        if (QFileInfo(resource).isRelative()) {
            resource.prepend(root);
        }
        if (verifiedPaths.contains(resource)) {
            // Don't check same url twice (for example track producers)
            continue;
        }

        QString proxy = EffectsList::property(e, QStringLiteral("kdenlive:proxy"));
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
                        fixProxyClip(e.attribute(QStringLiteral("id")), EffectsList::property(e, QStringLiteral("kdenlive:proxy")), updatedPath, documentProducers);
                        fixed = true;
                    }
                }
                if (!fixed) {
                    missingProxies.append(e);
                }
            }
            QString original = EffectsList::property(e, QStringLiteral("kdenlive:originalurl"));
            if (QFileInfo(original).isRelative()) {
                original.prepend(root);
            }
            // Check for slideshows
            bool slideshow = original.contains(QStringLiteral("/.all.")) || original.contains(QLatin1Char('?')) || original.contains(QLatin1Char('%'));
            if (slideshow && !EffectsList::property(e, QStringLiteral("ttl")).isEmpty()) {
                original = QFileInfo(original).absolutePath();
            }
            if (!QFile::exists(original)) {
                // clip has proxy but original clip is missing
                missingSources.append(e);
            }
            verifiedPaths.append(resource);
            continue;
        }
        // Check for slideshows
        bool slideshow = resource.contains(QStringLiteral("/.all.")) || resource.contains(QLatin1Char('?')) || resource.contains(QLatin1Char('%'));
        if ((service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) && slideshow) {
            resource = QFileInfo(resource).absolutePath();
        }
        if (!QFile::exists(resource)) {
            // Missing clip found
            m_missingClips.append(e);
        }
        // Make sure we don't query same path twice
        verifiedPaths.append(resource);
    }

    // Get list of used Luma files
    QStringList missingLumas;
    QStringList filesToCheck;
    QString filePath;
    QDomNodeList trans = m_doc.elementsByTagName(QStringLiteral("transition"));
    max = trans.count();
    for (int i = 0; i < max; ++i) {
        QDomElement transition = trans.at(i).toElement();
        QString service = getProperty(transition, QStringLiteral("mlt_service"));
        QString luma;
        if (service == QLatin1String("luma")) {
            luma = getProperty(transition, QStringLiteral("resource"));
        } else if (service == QLatin1String("composite")) {
            luma = getProperty(transition, QStringLiteral("luma"));
        }
        if (!luma.isEmpty() && !filesToCheck.contains(luma)) {
            filesToCheck.append(luma);
        }
    }

    QMap<QString, QString> autoFixLuma;
    // Check existence of luma files
    foreach (const QString &lumafile, filesToCheck) {
        filePath = lumafile;
        if (QFileInfo(filePath).isRelative()) {
            filePath.prepend(root);
        }
        if (!QFile::exists(filePath)) {
            QString fixedLuma;
            // check if this was an old format luma, not in correct folder
            fixedLuma = filePath.section(QLatin1Char('/'), 0, -2);
            fixedLuma.append(hdProfile ? QStringLiteral("/HD/") : QStringLiteral("/PAL/"));
            fixedLuma.append(filePath.section(QLatin1Char('/'), -1));
            if (QFile::exists(fixedLuma)) {
                // Auto replace pgm with png for lumas
                autoFixLuma.insert(filePath, fixedLuma);
                continue;
            }
            if (filePath.endsWith(QLatin1String(".pgm"))) {
                fixedLuma = filePath.section(QLatin1Char('.') , 0, -2) + QStringLiteral(".png");
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
            if (service == QLatin1String("luma")) {
                luma = getProperty(transition, QStringLiteral("resource"));
            } else if (service == QLatin1String("composite")) {
                luma = getProperty(transition, QStringLiteral("luma"));
            }
            if (!luma.isEmpty() && autoFixLuma.contains(luma)) {
                setProperty(transition, service == QLatin1String("luma") ? QStringLiteral("resource") : QStringLiteral("luma"), autoFixLuma.value(luma));
            }
        }
    }

    if (m_missingClips.isEmpty() && missingLumas.isEmpty() && missingProxies.isEmpty() && missingSources.isEmpty() && m_missingFonts.isEmpty()) {
        return false;
    }

    m_dialog = new QDialog();
    m_dialog->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_ui.setupUi(m_dialog);

    foreach (const QString &l, missingLumas) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Luma file") << l);
        item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-close")));
        item->setData(0, idRole, l);
        item->setData(0, statusRole, LUMAMISSING);
    }
    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_missingClips.isEmpty() && missingProxies.isEmpty() && missingSources.isEmpty());
    max = m_missingClips.count();
    m_missingProxyIds.clear();
    for (int i = 0; i < max; ++i) {
        QDomElement e = m_missingClips.at(i).toElement();
        QString clipType;
        ClipType type;
        int status = CLIPMISSING;
        const QString service = EffectsList::property(e, QStringLiteral("mlt_service"));
        QString resource = service == QLatin1String("timewarp") ? EffectsList::property(e, QStringLiteral("warp_resource")) : EffectsList::property(e, QStringLiteral("resource"));
        bool slideshow = resource.contains(QStringLiteral("/.all.")) || resource.contains(QLatin1Char('?')) || resource.contains(QLatin1Char('%'));
        if (service == QLatin1String("avformat") || service == QLatin1String("avformat-novalidate") || service == QLatin1String("framebuffer") || service == QLatin1String("timewarp")) {
            clipType = i18n("Video clip");
            type = AV;
        } else if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
            if (slideshow) {
                clipType = i18n("Slideshow clip");
                type = SlideShow;
            } else {
                clipType = i18n("Image clip");
                type = Image;
            }
        } else if (service == QLatin1String("mlt") || service == QLatin1String("xml")) {
            clipType = i18n("Playlist clip");
            type = Playlist;
        } else if (e.tagName() == QLatin1String("missingtitle")) {
            clipType = i18n("Title Image");
            status = TITLE_IMAGE_ELEMENT;
            type = Text;
        } else {
            clipType = i18n("Unknown");
            type = Unknown;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPMISSING);
        item->setData(0, clipTypeRole, type);
        item->setData(0, idRole, e.attribute(QStringLiteral("id")));
        item->setToolTip(0, i18n("Missing item"));

        if (status == TITLE_IMAGE_ELEMENT) {
            item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-warning")));
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
            item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-close")));
            if (QFileInfo(resource).isRelative()) {
                resource.prepend(root);
            }
            item->setData(0, hashRole, EffectsList::property(e, QStringLiteral("kdenlive:file_hash")));
            item->setData(0, sizeRole, EffectsList::property(e, QStringLiteral("kdenlive:file_size")));
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

    //TODO the following loop is disabled because m_missingFonts is cleared (line 124), but not filled, that looks suspicious
    /*foreach (const QString &font, m_missingFonts) {
        QString clipType = i18n("Title Font");
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setData(0, statusRole, CLIPPLACEHOLDER);
        item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-warning")));
        item->setToolTip(1, e.attribute(QStringLiteral("name")));
        QString newft = QFontInfo(QFont(font)).family();
        item->setText(1, i18n("%1 will be replaced by %2", font, newft));
        item->setData(0, typeRole, CLIPMISSING);
        }*/

    if (!m_missingClips.isEmpty()) {
        m_ui.infoLabel->setText(i18n("The project file contains missing clips or files"));
    }
    if (!missingProxies.isEmpty()) {
        if (!m_ui.infoLabel->text().isEmpty()) {
            m_ui.infoLabel->setText(m_ui.infoLabel->text() + QStringLiteral(". "));
        }
        m_ui.infoLabel->setText(m_ui.infoLabel->text() + i18n("Missing proxies will be recreated after opening."));
    }
    if (!missingSources.isEmpty()) {
        if (!m_ui.infoLabel->text().isEmpty()) {
            m_ui.infoLabel->setText(m_ui.infoLabel->text() + QStringLiteral(". "));
        }
        m_ui.infoLabel->setText(m_ui.infoLabel->text() + i18np("The project file contains a missing clip, you can still work with its proxy.", "The project file contains %1 missing clips, you can still work with their proxies.", missingSources.count()));
    }

    m_ui.removeSelected->setEnabled(!m_missingClips.isEmpty());
    m_ui.recursiveSearch->setEnabled(!m_missingClips.isEmpty() || !missingLumas.isEmpty() || !missingSources.isEmpty());
    m_ui.usePlaceholders->setEnabled(!m_missingClips.isEmpty());

    // Check missing proxies
    max = missingProxies.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Proxy clip"));
        item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-warning")));
        item->setText(1, i18np("%1 missing proxy clip, will be recreated on project opening", "%1 missing proxy clips, will be recreated on project opening", max));
        //item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, PROXYMISSING);
        item->setToolTip(0, i18n("Missing proxy"));
    }

    for (int i = 0; i < max; ++i) {
        QDomElement e = missingProxies.at(i).toElement();
        QString realPath = EffectsList::property(e, QStringLiteral("kdenlive:originalurl"));
        QString id = e.attribute(QStringLiteral("id"));
        m_missingProxyIds << id;
        // Tell Kdenlive to recreate proxy
        e.setAttribute(QStringLiteral("_replaceproxy"), QStringLiteral("1"));
        // Replace proxy url with real clip in MLT producers
        QDomElement mltProd;
        int prodsCount = documentProducers.count();
        for (int j = 0; j < prodsCount; ++j) {
            mltProd = documentProducers.at(j).toElement();
            QString prodId = mltProd.attribute(QStringLiteral("id"));
            QString parentId = prodId;
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
                QString resource = EffectsList::property(mltProd, QStringLiteral("resource"));
                if (slowmotion) {
                    suffix = QLatin1Char('?') + resource.section(QLatin1Char('?'), -1);
                }
                EffectsList::setProperty(mltProd, QStringLiteral("resource"), realPath + suffix);
                if (prodId == id) {
                    // Only set proxy property on master producer
                    EffectsList::setProperty(mltProd, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
                }
            }
        }
    }

    if (max > 0) {
        // original doc was modified
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), QStringLiteral("1"));
    }

    // Check clips with available proxies but missing original source clips
    max = missingSources.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Source clip"));
        item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-warning")));
        item->setText(1, i18n("%1 missing source clips, you can only use the proxies", max));
        //item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, SOURCEMISSING);
        item->setToolTip(0, i18n("Missing source clip"));
        for (int i = 0; i < max; ++i) {
            QDomElement e = missingSources.at(i).toElement();
            QString realPath = EffectsList::property(e, QStringLiteral("kdenlive:originalurl"));
            QString id = e.attribute(QStringLiteral("id"));
            // Tell Kdenlive the source is missing
            e.setAttribute(QStringLiteral("_missingsource"), QStringLiteral("1"));
            QTreeWidgetItem *subitem = new QTreeWidgetItem(item, QStringList() << i18n("Source clip"));
            //qCDebug(KDENLIVE_LOG)<<"// Adding missing source clip: "<<realPath;
            subitem->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-close")));
            subitem->setText(1, realPath);
            subitem->setData(0, hashRole, EffectsList::property(e, QStringLiteral("kdenlive:file_hash")));
            subitem->setData(0, sizeRole, EffectsList::property(e, QStringLiteral("kdenlive:file_size")));
            subitem->setData(0, statusRole, CLIPMISSING);
            //int t = e.attribute("type").toInt();
            subitem->setData(0, typeRole, EffectsList::property(e, QStringLiteral("mlt_service")));
            subitem->setData(0, idRole, id);
        }
    }
    if (max > 0) {
        // original doc was modified
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), QStringLiteral("1"));
    }
    m_ui.treeWidget->resizeColumnToContents(0);
    connect(m_ui.recursiveSearch, &QAbstractButton::pressed, this, &DocumentChecker::slotSearchClips);
    connect(m_ui.usePlaceholders, &QAbstractButton::pressed, this, &DocumentChecker::slotPlaceholders);
    connect(m_ui.removeSelected, &QAbstractButton::pressed, this, &DocumentChecker::slotDeleteSelected);
    connect(m_ui.treeWidget, &QTreeWidget::itemDoubleClicked, this, &DocumentChecker::slotEditItem);
    connect(m_ui.treeWidget, &QTreeWidget::itemSelectionChanged, this, &DocumentChecker::slotCheckButtons);
    //adjustSize();
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

void DocumentChecker::setProperty(const QDomElement &effect, const QString &name, const QString &value)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            e.firstChild().setNodeValue(value);
        }
    }
}

void DocumentChecker::slotSearchClips()
{
    //QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    QString clipFolder = m_url.adjusted(QUrl::RemoveFilename).toLocalFile();
    QString newpath = QFileDialog::getExistingDirectory(qApp->activeWindow(), i18n("Clips folder"), clipFolder);
    if (newpath.isEmpty()) {
        return;
    }
    int ix = 0;
    bool fixed = false;
    m_ui.recursiveSearch->setChecked(true);
    //TODO: make non modal
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    QDir searchDir(newpath);
    while (child) {
        if (child->data(0, statusRole).toInt() == SOURCEMISSING) {
            for (int j = 0; j < child->childCount(); ++j) {
                QTreeWidgetItem *subchild = child->child(j);
                QString clipPath = searchFileRecursively(searchDir, subchild->data(0, sizeRole).toString(), subchild->data(0, hashRole).toString(), subchild->text(1));
                if (!clipPath.isEmpty()) {
                    fixed = true;
                    subchild->setText(1, clipPath);
                    subchild->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-ok")));
                    subchild->setData(0, statusRole, CLIPOK);
                }
            }
        } else if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            bool perfectMatch = true;
            ClipType type = (ClipType) child->data(0, clipTypeRole).toInt();
            QString clipPath;
            if (type != SlideShow) {
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
                child->setIcon(0, perfectMatch ? KoIconUtils::themedIcon(QStringLiteral("dialog-ok")) : KoIconUtils::themedIcon(QStringLiteral("dialog-warning")));
                child->setData(0, statusRole, CLIPOK);
            }
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            QString fileName = searchLuma(searchDir, child->data(0, idRole).toString());
            if (!fileName.isEmpty()) {
                fixed = true;
                child->setText(1, fileName);
                child->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-ok")));
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
                child->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-ok")));
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
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), QStringLiteral("1"));
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
    searchPath.cd(QStringLiteral("../share/apps/kdenlive/lumas"));
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

QString DocumentChecker::searchPathRecursively(const QDir &dir, const QString &fileName, ClipType type) const
{
    QString foundFileName;
    QStringList filters;
    if (type == SlideShow) {
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
        if (type == SlideShow) {
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
    ClipType type = (ClipType) item->data(0, clipTypeRole).toInt();
    bool fixed = false;
    if (type == SlideShow && QFile::exists(url.adjusted(QUrl::RemoveFilename).toLocalFile())) {
        fixed = true;
    }
    if (fixed || QFile::exists(url.toLocalFile())) {
        item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-ok")));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) {
            item->setData(0, statusRole, CLIPOK);
        } else {
            item->setData(0, statusRole, LUMAOK);
        }
        checkStatus();
    } else {
        item->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-close")));
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

    // Mark document as modified
    m_doc.documentElement().setAttribute(QStringLiteral("modified"), 1);

    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
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
    //QDialog::accept();
}

void DocumentChecker::fixProxyClip(const QString &id, const QString &oldUrl, const QString &newUrl, const QDomNodeList &producers)
{
    QDomElement e, property;
    QDomNodeList properties;
    for (int i = 0; i < producers.count(); ++i) {
        e = producers.item(i).toElement();
        QString sourceId = e.attribute(QStringLiteral("id"));
        QString parentId = sourceId.section(QLatin1Char('_'), 0, 0);
        if (parentId.startsWith(QLatin1String("slowmotion"))) {
            parentId = parentId.section(QLatin1Char(':'), 1, 1);
        }
        if (parentId == id) {
            // Fix clip
            QString resource = EffectsList::property(e, QStringLiteral("resource"));
            // TODO: Slowmmotion clips
            if (resource.contains(QRegExp(QStringLiteral("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))) {
                //fixedResource.append(QLatin1Char('?') + resource.section(QLatin1Char('?'), -1));
            }
            if (resource == oldUrl) {
                EffectsList::setProperty(e, QStringLiteral("resource"), newUrl);
            }
            if (sourceId == id) {
                // Only set originalurl on master producer
                EffectsList::setProperty(e, QStringLiteral("kdenlive:proxy"), newUrl);
            }
        }
    }
}

void DocumentChecker::fixSourceClipItem(QTreeWidgetItem *child, const QDomNodeList &producers)
{
    QDomElement e, property;
    QDomNodeList properties;
    //int t = child->data(0, typeRole).toInt();
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString id = child->data(0, idRole).toString();
        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            QString sourceId = e.attribute(QStringLiteral("id"));
            QString parentId = sourceId.section(QLatin1Char('_'), 0, 0);
            if (parentId.startsWith(QLatin1String("slowmotion"))) {
                parentId = parentId.section(QLatin1Char(':'), 1, 1);
            }
            if (parentId == id) {
                // Fix clip
                QString resource = EffectsList::property(e, QStringLiteral("resource"));
                QString fixedResource = child->text(1);
                if (resource.contains(QRegExp(QStringLiteral("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))) {
                    fixedResource.append(QLatin1Char('?') + resource.section(QLatin1Char('?'), -1));
                }
                if (sourceId == id) {
                    // Only set originalurl on master producer
                    EffectsList::setProperty(e, QStringLiteral("kdenlive:originalurl"), fixedResource);
                }
                if (m_missingProxyIds.contains(parentId)) {
                    // Proxy is also missing, replace resource
                    EffectsList::setProperty(e, QStringLiteral("resource"), fixedResource);
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
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString id = child->data(0, idRole).toString();
        QString fixedResource = child->text(1);
        if (t == TITLE_IMAGE_ELEMENT) {
            // edit images embedded in titles
            for (int i = 0; i < producers.count(); ++i) {
                e = producers.item(i).toElement();
                if (e.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0) == id) {
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
                if (e.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0) == id || e.attribute(QStringLiteral("id")).section(QLatin1Char(':'), 1, 1) == id || e.attribute(QStringLiteral("id")) == id) {
                    // Fix clip
                    QString resource = getProperty(e, QStringLiteral("resource"));
                    QString service = getProperty(e, QStringLiteral("mlt_service"));
                    QString updatedResource = fixedResource;
                    if (resource.contains(QRegExp(QStringLiteral("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))) {
                        updatedResource.append(QLatin1Char('?') + resource.section(QLatin1Char('?'), -1));
                    }
                    if (service == QLatin1String("timewarp")) {
                        setProperty(e, QStringLiteral("warp_resource"), updatedResource);
                        updatedResource.prepend(getProperty(e, QStringLiteral("warp_speed")) + QLatin1Char(':'));
                    }
                    setProperty(e, QStringLiteral("resource"), updatedResource);
                }
            }
        }
    } else if (child->data(0, statusRole).toInt() == CLIPPLACEHOLDER && t != TITLE_FONT_ELEMENT && t != TITLE_IMAGE_ELEMENT) {
        //QString id = child->data(0, idRole).toString();
        /*for (int i = 0; i < infoproducers.count(); ++i) {
            e = infoproducers.item(i).toElement();
            if (e.attribute("id") == id) {
                // Fix clip
                e.setAttribute("placeholder", '1');
                break;
            }
        }*/
    } else if (child->data(0, statusRole).toInt() == LUMAOK) {
        for (int i = 0; i < trans.count(); ++i) {
            QString service = getProperty(trans.at(i).toElement(), QStringLiteral("mlt_service"));
            QString luma;
            if (service == QLatin1String("luma")) {
                luma = getProperty(trans.at(i).toElement(), QStringLiteral("resource"));
            } else if (service == QLatin1String("composite")) {
                luma = getProperty(trans.at(i).toElement(), QStringLiteral("luma"));
            }
            if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                setProperty(trans.at(i).toElement(), service == QLatin1String("luma") ? QStringLiteral("resource") : QStringLiteral("luma"), child->text(1));
                //qCDebug(KDENLIVE_LOG) << "replace with; " << child->text(1);
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
        for (int i = 0; i < trans.count(); ++i) {
            QString service = getProperty(trans.at(i).toElement(), QStringLiteral("mlt_service"));
            QString luma;
            if (service == QLatin1String("luma")) {
                luma = getProperty(trans.at(i).toElement(), QStringLiteral("resource"));
            } else if (service == QLatin1String("composite")) {
                luma = getProperty(trans.at(i).toElement(), QStringLiteral("luma"));
            }
            if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                setProperty(trans.at(i).toElement(), service == QLatin1String("luma") ? QStringLiteral("resource") : QStringLiteral("luma"), QString());
            }
        }
    }
}

void DocumentChecker::slotPlaceholders()
{
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            child->setData(0, statusRole, CLIPPLACEHOLDER);
            child->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-ok")));
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            child->setData(0, statusRole, LUMAPLACEHOLDER);
            child->setIcon(0, KoIconUtils::themedIcon(QStringLiteral("dialog-ok")));
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
    while (child) {
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
    if (KMessageBox::warningContinueCancel(m_dialog, i18np("This will remove the selected clip from this project", "This will remove the selected clips from this project", m_ui.treeWidget->selectedItems().count()), i18n("Remove clips")) == KMessageBox::Cancel) {
        return;
    }
    QStringList deletedIds;
    QStringList deletedLumas;
    QDomNodeList playlists = m_doc.elementsByTagName(QStringLiteral("playlist"));

    foreach (QTreeWidgetItem *child, m_ui.treeWidget->selectedItems()) {
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
        foreach (const QString &lumaPath, deletedLumas) {
            for (int i = 0; i < transitions.count(); ++i) {
                e = transitions.item(i).toElement();
                QString service = EffectsList::property(e, QStringLiteral("mlt_service"));
                QString resource;
                if (service == QLatin1String("luma")) {
                    resource = EffectsList::property(e, QStringLiteral("resource"));
                } else if (service == QLatin1String("composite")) {
                    resource = EffectsList::property(e, QStringLiteral("luma"));
                }
                if (resource == lumaPath) {
                    EffectsList::removeProperty(e, service == QLatin1String("luma") ? QStringLiteral("resource") : QStringLiteral("luma"));
                }
            }
        }
    }

    if (!deletedIds.isEmpty()) {
        QDomElement e;
        QDomNodeList producers = m_doc.elementsByTagName(QStringLiteral("producer"));
        //QDomNodeList infoproducers = m_doc.elementsByTagName("kdenlive_producer");

        QDomNode mlt = m_doc.elementsByTagName(QStringLiteral("mlt")).at(0);
        QDomNode kdenlivedoc = m_doc.elementsByTagName(QStringLiteral("kdenlivedoc")).at(0);

        /*for (int i = 0, j = 0; i < infoproducers.count() && j < deletedIds.count(); ++i) {
            e = infoproducers.item(i).toElement();
            if (deletedIds.contains(e.attribute("id"))) {
                // Remove clip
                kdenlivedoc.removeChild(e);
                --i;
                j++;
            }
        }*/

        for (int i = 0; i < producers.count(); ++i) {
            e = producers.item(i).toElement();
            if (deletedIds.contains(e.attribute(QStringLiteral("id")).section(QLatin1Char('_'), 0, 0)) || deletedIds.contains(e.attribute(QStringLiteral("id")).section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0))) {
                // Remove clip
                mlt.removeChild(e);
                --i;
            }
        }

        for (int i = 0; i < playlists.count(); ++i) {
            QDomNodeList entries = playlists.at(i).toElement().elementsByTagName(QStringLiteral("entry"));
            for (int j = 0; j < entries.count(); ++j) {
                e = entries.item(j).toElement();
                if (deletedIds.contains(e.attribute(QStringLiteral("producer")).section(QLatin1Char('_'), 0, 0)) || deletedIds.contains(e.attribute(QStringLiteral("producer")).section(QLatin1Char(':'), 1, 1).section(QLatin1Char('_'), 0, 0))) {
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
        m_doc.documentElement().setAttribute(QStringLiteral("modified"), QStringLiteral("1"));
        checkStatus();
    }
}

void DocumentChecker::checkMissingImagesAndFonts(const QStringList &images, const QStringList &fonts, const QString &id, const QString &baseClip)
{
    QDomDocument doc;
    foreach (const QString &img, images) {
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
    foreach (const QString &fontelement, fonts) {
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

