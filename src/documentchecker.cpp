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
#include "docclipbase.h"
#include "titlewidget.h"
#include "definitions.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KGlobalSettings>
#include <KFileItem>
#include <KIO/NetAccess>
#include <KFileDialog>
#include <KApplication>
#include <KUrlRequesterDialog>
#include <KMessageBox>
#include <KStandardDirs>

#include <QTreeWidgetItem>
#include <QFile>
#include <QHeaderView>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QCryptographicHash>

const int hashRole = Qt::UserRole;
const int sizeRole = Qt::UserRole + 1;
const int idRole = Qt::UserRole + 2;
const int statusRole = Qt::UserRole + 3;
const int typeRole = Qt::UserRole + 4;
const int typeOriginalResource = Qt::UserRole + 5;
const int resetDurationRole = Qt::UserRole + 6;

const int CLIPMISSING = 0;
const int CLIPOK = 1;
const int CLIPPLACEHOLDER = 2;
const int CLIPWRONGDURATION = 3;
const int PROXYMISSING = 4;
const int SOURCEMISSING = 5;

const int LUMAMISSING = 10;
const int LUMAOK = 11;
const int LUMAPLACEHOLDER = 12;

enum TITLECLIPTYPE { TITLE_IMAGE_ELEMENT = 20, TITLE_FONT_ELEMENT = 21 };

DocumentChecker::DocumentChecker(QDomNodeList infoproducers, QDomDocument doc):
    m_info(infoproducers), m_doc(doc), m_dialog(NULL)
{

}


bool DocumentChecker::hasErrorInClips()
{
    int clipType;
    QDomElement e;
    QString resource;
    int max;
    QDomNodeList documentProducers = m_doc.elementsByTagName("producer");
    QList <QDomElement> wrongDurationClips;
    // List clips whose proxy is missing
    QList <QDomElement> missingProxies;
    // List clips who have a working proxy but no source clip
    QList <QDomElement> missingSources;
    m_safeImages.clear();
    m_safeFonts.clear();
    max = m_info.count();
    for (int i = 0; i < max; i++) {
        e = m_info.item(i).toElement();
        clipType = e.attribute("type").toInt();
        if (clipType == COLOR) continue;
        if (clipType != TEXT && clipType != IMAGE && clipType != SLIDESHOW) {
            QString id = e.attribute("id");
            int duration = e.attribute("duration").toInt();
            int mltDuration = -1;
            QDomElement mltProd;
            QString prodId;
            // Check that the duration is in sync between Kdenlive's info and MLT's playlist
            int prodsCount = documentProducers.count();
            for (int j = 0; j < prodsCount; j++) {
                mltProd = documentProducers.at(j).toElement();
                prodId = mltProd.attribute("id");
                // Don't check slowmotion clips for now... (TODO?)
                if (prodId.startsWith("slowmotion")) continue;
                if (prodId.contains('_')) prodId = prodId.section('_', 0, 0);
                if (prodId != id) continue;
                if (mltDuration > 0 ) {
                    // We have several MLT producers for the same clip (probably track producers)
                    int newLength = EffectsList::property(mltProd, "length").toInt();
                    if (newLength != mltDuration) {
                        // we have a different duration for the same clip, that is not safe
                        e.setAttribute("_resetDuration", 1);
                    }
                }
                mltDuration = EffectsList::property(mltProd, "length").toInt();
                if (mltDuration != duration) {
                    // Duration mismatch
                    e.setAttribute("_mismatch", mltDuration);
                    if (mltDuration == 15000) {
                        // a length of 15000 might indicate a wrong clip length since it is a default length
                        e.setAttribute("_resetDuration", 1);
                    }
                    if (!wrongDurationClips.contains(e)) wrongDurationClips.append(e);
                }
            }
        }
        
        if (clipType == TEXT) {
            //TODO: Check is clip template is missing (xmltemplate) or hash changed
            QStringList images = TitleWidget::extractImageList(e.attribute("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(e.attribute("xmldata"));
            checkMissingImagesAndFonts(images, fonts, e.attribute("id"), e.attribute("name"));
            continue;
        }
        resource = e.attribute("resource");
        if (e.hasAttribute("proxy")) {
            QString proxyresource = e.attribute("proxy");
            if (!proxyresource.isEmpty() && proxyresource != "-") {
                // clip has a proxy
                if (!KIO::NetAccess::exists(KUrl(proxyresource), KIO::NetAccess::SourceSide, 0)) {
                    // Missing clip found
                    missingProxies.append(e);
                }
                else if (!KIO::NetAccess::exists(KUrl(resource), KIO::NetAccess::SourceSide, 0)) {
                    // clip has proxy but original clip is missing
                    missingSources.append(e);
                    continue;
                }
            }
        }
        if (clipType == SLIDESHOW) resource = KUrl(resource).directory();
        if (!KIO::NetAccess::exists(KUrl(resource), KIO::NetAccess::SourceSide, 0)) {
            // Missing clip found
            m_missingClips.append(e);
        } else {
            // Check if the clip has changed
            if (clipType != SLIDESHOW && e.hasAttribute("file_hash")) {
                if (e.attribute("file_hash") != DocClipBase::getHash(e.attribute("resource")))
                    e.removeAttribute("file_hash");
            }
        }
    }

    // Get list of used Luma files
    QStringList missingLumas;
    QStringList filesToCheck;
    QString filePath;
    QString root = m_doc.documentElement().attribute("root");
    if (!root.isEmpty()) root = KUrl(root).path(KUrl::AddTrailingSlash);
    QDomNodeList trans = m_doc.elementsByTagName("transition");
    max = trans.count();
    for (int i = 0; i < max; i++) {
        QString luma = getProperty(trans.at(i).toElement(), "luma");
        if (!luma.isEmpty() && !filesToCheck.contains(luma))
            filesToCheck.append(luma);
    }
    // Check existence of luma files
    foreach (const QString &lumafile, filesToCheck) {
        filePath = lumafile;
        if (!filePath.startsWith('/')) filePath.prepend(root);
        if (!QFile::exists(filePath)) {
            missingLumas.append(lumafile);
        }
    }
    
    

    if (m_missingClips.isEmpty() && missingLumas.isEmpty() && wrongDurationClips.isEmpty() && missingProxies.isEmpty() && missingSources.isEmpty())
        return false;

    m_dialog = new QDialog();
    m_dialog->setFont(KGlobalSettings::toolBarFont());
    m_ui.setupUi(m_dialog);

    foreach(const QString &l, missingLumas) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Luma file") << l);
        item->setIcon(0, KIcon("dialog-close"));
        item->setData(0, idRole, l);
        item->setData(0, statusRole, LUMAMISSING);
    }

    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    max = m_missingClips.count();
    for (int i = 0; i < max; i++) {
        e = m_missingClips.at(i).toElement();
        QString clipType;
        int t = e.attribute("type").toInt();
        switch (t) {
        case AV:
            clipType = i18n("Video clip");
            break;
        case VIDEO:
            clipType = i18n("Mute video clip");
            break;
        case AUDIO:
            clipType = i18n("Audio clip");
            break;
        case PLAYLIST:
            clipType = i18n("Playlist clip");
            break;
        case IMAGE:
            clipType = i18n("Image clip");
            break;
        case SLIDESHOW:
            clipType = i18n("Slideshow clip");
            break;
        case TITLE_IMAGE_ELEMENT:
            clipType = i18n("Title Image");
            break;
        case TITLE_FONT_ELEMENT:
            clipType = i18n("Title Font");
            break;
        default:
            clipType = i18n("Video clip");
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        if (t == TITLE_IMAGE_ELEMENT) {
            item->setIcon(0, KIcon("dialog-warning"));
            item->setToolTip(1, e.attribute("name"));
            item->setText(1, e.attribute("resource"));
            item->setData(0, statusRole, CLIPPLACEHOLDER);
            item->setData(0, typeOriginalResource, e.attribute("resource"));
        } else if (t == TITLE_FONT_ELEMENT) {
            item->setIcon(0, KIcon("dialog-warning"));
            item->setToolTip(1, e.attribute("name"));
            QString ft = e.attribute("resource");
            QString newft = QFontInfo(QFont(ft)).family();
            item->setText(1, i18n("%1 will be replaced by %2", ft, newft));
            item->setData(0, statusRole, CLIPPLACEHOLDER);
        } else {
            item->setIcon(0, KIcon("dialog-close"));
            item->setText(1, e.attribute("resource"));
            item->setData(0, hashRole, e.attribute("file_hash"));
            item->setData(0, sizeRole, e.attribute("file_size"));
            item->setData(0, statusRole, CLIPMISSING);
        }
        item->setData(0, typeRole, t);
        item->setData(0, idRole, e.attribute("id"));
        item->setToolTip(0, i18n("Missing item"));
    }

    if (m_missingClips.count() > 0) {
        if (wrongDurationClips.count() > 0) {
            m_ui.infoLabel->setText(i18n("The project file contains missing clips or files and clip duration mismatch"));
        }
        else {
            m_ui.infoLabel->setText(i18n("The project file contains missing clips or files"));
        }
    }
    else if (wrongDurationClips.count() > 0) {
        m_ui.infoLabel->setText(i18n("The project file contains clips with duration mismatch"));
    }
    if (missingProxies.count() > 0) {
        if (!m_ui.infoLabel->text().isEmpty()) m_ui.infoLabel->setText(m_ui.infoLabel->text() + ". ");
        m_ui.infoLabel->setText(m_ui.infoLabel->text() + i18n("Missing proxies will be recreated after opening."));
    }
    if (missingSources.count() > 0) {
        if (!m_ui.infoLabel->text().isEmpty()) m_ui.infoLabel->setText(m_ui.infoLabel->text() + ". ");
        m_ui.infoLabel->setText(m_ui.infoLabel->text() + i18np("The project file contains a missing clip, you can still work with its proxy.", "The project file contains missing clips, you can still work with their proxies.", missingSources.count()));
    }

    m_ui.removeSelected->setEnabled(!m_missingClips.isEmpty());
    m_ui.recursiveSearch->setEnabled(!m_missingClips.isEmpty() || !missingLumas.isEmpty() || !missingSources.isEmpty());
    m_ui.usePlaceholders->setEnabled(!m_missingClips.isEmpty());
    m_ui.fixDuration->setEnabled(!wrongDurationClips.isEmpty());

    max = wrongDurationClips.count();
    for (int i = 0; i < max; i++) {
        e = wrongDurationClips.at(i).toElement();
        QString clipType;
        int t = e.attribute("type").toInt();
        switch (t) {
        case AV:
            clipType = i18n("Video clip");
            break;
        case VIDEO:
            clipType = i18n("Mute video clip");
            break;
        case AUDIO:
            clipType = i18n("Audio clip");
            break;
        case PLAYLIST:
            clipType = i18n("Playlist clip");
            break;
        case IMAGE:
            clipType = i18n("Image clip");
            break;
        case SLIDESHOW:
            clipType = i18n("Slideshow clip");
            break;
        default:
            clipType = i18n("Video clip");
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << clipType);
        item->setIcon(0, KIcon("timeadjust"));
        item->setText(1, e.attribute("resource"));
        item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, sizeRole, e.attribute("_mismatch"));
        e.removeAttribute("_mismatch");
        item->setData(0, resetDurationRole, (int) e.hasAttribute("_resetDuration"));
        e.removeAttribute("_resetDuration");
        item->setData(0, statusRole, CLIPWRONGDURATION);
        item->setData(0, typeRole, t);
        item->setData(0, idRole, e.attribute("id"));
        item->setToolTip(0, i18n("Duration mismatch"));
    }

    // Check missing proxies
    max = missingProxies.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Proxy clip"));
        item->setIcon(0, KIcon("dialog-warning"));
        item->setText(1, i18n("%1 missing proxy clips, will be recreated on project opening", max));
        item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, PROXYMISSING);
        item->setToolTip(0, i18n("Missing proxy"));
    }

    for (int i = 0; i < max; i++) {
        e = missingProxies.at(i).toElement();
        QString clipType;
        QString realPath = e.attribute("resource");
        QString id = e.attribute("id");
        // Tell Kdenlive to recreate proxy
        e.setAttribute("_replaceproxy", "1");
        // Replace proxy url with real clip in MLT producers
        QDomNodeList properties;
        QDomElement mltProd;
        QDomElement property;
        int prodsCount = documentProducers.count();
        for (int j = 0; j < prodsCount; j++) {
            mltProd = documentProducers.at(j).toElement();
            QString prodId = mltProd.attribute("id");
            bool slowmotion = false;
            if (prodId.startsWith("slowmotion")) {
                slowmotion = true;
                prodId = prodId.section(':', 1, 1);
            }
            if (prodId.contains('_')) prodId = prodId.section('_', 0, 0);
            if (prodId == id) {
                // Hit, we must replace url
                properties = mltProd.childNodes();
                for (int k = 0; k < properties.count(); ++k) {
                    property = properties.item(k).toElement();
                    if (property.attribute("name") == "resource") {
                        QString resource = property.firstChild().nodeValue();                    
                        QString suffix;
                        if (slowmotion) suffix = '?' + resource.section('?', -1);
                        property.firstChild().setNodeValue(realPath + suffix);
                        break;
                    }
                }
            }
        }
    }
    
    if (max > 0) {
        // original doc was modified
        QDomElement infoXml = m_doc.elementsByTagName("kdenlivedoc").at(0).toElement();
        infoXml.setAttribute("modified", "1");
    }
    
    // Check clips with available proxies but missing original source clips
    max = missingSources.count();
    if (max > 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Source clip"));
        item->setIcon(0, KIcon("dialog-warning"));
        item->setText(1, i18n("%1 missing source clips, you can only use the proxies", max));
        item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, statusRole, SOURCEMISSING);
        item->setToolTip(0, i18n("Missing source clip"));
        for (int i = 0; i < max; i++) {
            e = missingSources.at(i).toElement();
            QString clipType;
            QString realPath = e.attribute("resource");
            QString id = e.attribute("id");
            // Tell Kdenlive the source is missing
            e.setAttribute("_missingsource", "1");
            QTreeWidgetItem *subitem = new QTreeWidgetItem(item, QStringList() << i18n("Source clip"));
            kDebug()<<"// Adding missing source clip: "<<realPath;
            subitem->setIcon(0, KIcon("dialog-close"));
            subitem->setText(1, realPath);
            subitem->setData(0, hashRole, e.attribute("file_hash"));
            subitem->setData(0, sizeRole, e.attribute("file_size"));
            subitem->setData(0, statusRole, CLIPMISSING);
            int t = e.attribute("type").toInt();
            subitem->setData(0, typeRole, t);
            subitem->setData(0, idRole, id);
        }
    }
    
    if (max > 0) {
        // original doc was modified
        QDomElement infoXml = m_doc.elementsByTagName("kdenlivedoc").at(0).toElement();
        infoXml.setAttribute("modified", "1");
    }
    
    connect(m_ui.recursiveSearch, SIGNAL(pressed()), this, SLOT(slotSearchClips()));
    connect(m_ui.usePlaceholders, SIGNAL(pressed()), this, SLOT(slotPlaceholders()));
    connect(m_ui.removeSelected, SIGNAL(pressed()), this, SLOT(slotDeleteSelected()));
    connect(m_ui.fixDuration, SIGNAL(pressed()), this, SLOT(slotFixDuration()));
    connect(m_ui.treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotEditItem(QTreeWidgetItem *, int)));
    connect(m_ui.treeWidget, SIGNAL(itemSelectionChanged()), this, SLOT(slotCheckButtons()));
    //adjustSize();
    if (m_ui.treeWidget->topLevelItem(0)) m_ui.treeWidget->setCurrentItem(m_ui.treeWidget->topLevelItem(0));
    checkStatus();
    int acceptMissing = m_dialog->exec();
    if (acceptMissing == QDialog::Accepted) acceptDialog();
    return (acceptMissing != QDialog::Accepted);
}

DocumentChecker::~DocumentChecker()
{
    if (m_dialog) delete m_dialog;
}


QString DocumentChecker::getProperty(QDomElement effect, const QString &name)
{
    QDomNodeList params = effect.elementsByTagName("property");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("name") == name) {
            return e.firstChild().nodeValue();
        }
    }
    return QString();
}

void DocumentChecker::setProperty(QDomElement effect, const QString &name, const QString value)
{
    QDomNodeList params = effect.elementsByTagName("property");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("name") == name) {
            e.firstChild().setNodeValue(value);
        }
    }
}

void DocumentChecker::slotSearchClips()
{
    QString newpath = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///clipfolder"), kapp->activeWindow(), i18n("Clips folder"));
    if (newpath.isEmpty()) return;
    int ix = 0;
    bool fixed = false;
    m_ui.recursiveSearch->setEnabled(false);
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    QDir searchDir(newpath);
    while (child) {
        if (child->data(0, statusRole).toInt() == SOURCEMISSING) {
            for (int j = 0; j < child->childCount(); j++) {
                QTreeWidgetItem *subchild = child->child(j);
                QString clipPath = searchFileRecursively(searchDir, subchild->data(0, sizeRole).toString(), subchild->data(0, hashRole).toString());
                if (!clipPath.isEmpty()) {
                    fixed = true;
                    
                    subchild->setText(1, clipPath);
                    subchild->setIcon(0, KIcon("dialog-ok"));
                    subchild->setData(0, statusRole, CLIPOK);
                }
            }
        }
        else if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            QString clipPath = searchFileRecursively(searchDir, child->data(0, sizeRole).toString(), child->data(0, hashRole).toString());
            if (!clipPath.isEmpty()) {
                fixed = true;
                child->setText(1, clipPath);
                child->setIcon(0, KIcon("dialog-ok"));
                child->setData(0, statusRole, CLIPOK);
            }
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            QString fileName = searchLuma(searchDir, child->data(0, idRole).toString());
            if (!fileName.isEmpty()) {
                fixed = true;
                child->setText(1, fileName);
                child->setIcon(0, KIcon("dialog-ok"));
                child->setData(0, statusRole, LUMAOK);
            }
        }
        else if (child->data(0, typeRole).toInt() == TITLE_IMAGE_ELEMENT && child->data(0, statusRole).toInt() == CLIPPLACEHOLDER) {
            // Search missing title images
            QString missingFileName = KUrl(child->text(1)).fileName();
            QString newPath = searchPathRecursively(searchDir, missingFileName);
            if (!newPath.isEmpty()) {
                // File found
                fixed = true;
                child->setText(1, newPath);
                child->setIcon(0, KIcon("dialog-ok"));
                child->setData(0, statusRole, CLIPOK);
            }
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    m_ui.recursiveSearch->setEnabled(true);
    if (fixed) {
        // original doc was modified
        QDomElement infoXml = m_doc.elementsByTagName("kdenlivedoc").at(0).toElement();
        infoXml.setAttribute("modified", "1");
    }
    checkStatus();
}


QString DocumentChecker::searchLuma(const QDir &dir, const QString &file) const
{
    KUrl searchPath(KdenliveSettings::mltpath());
    QString fname = KUrl(file).fileName();
    if (file.contains("PAL"))
        searchPath.cd("../lumas/PAL");
    else
        searchPath.cd("../lumas/NTSC");
    QString result = searchPath.path(KUrl::AddTrailingSlash) + fname;
    if (QFile::exists(result))
        return result;
    // try to find luma in application path
    searchPath.clear();
    searchPath = KUrl(QCoreApplication::applicationDirPath());
    searchPath.cd("../share/apps/kdenlive/lumas");
    result = searchPath.path(KUrl::AddTrailingSlash) + fname;
    if (QFile::exists(result))
        return result;
    // Try in Kdenlive's standard KDE path
    result = KStandardDirs::locate("appdata", "lumas/" + fname);
    if (!result.isEmpty()) return result;
    // Try in user's chosen folder 
    return searchPathRecursively(dir, fname);
}

QString DocumentChecker::searchPathRecursively(const QDir &dir, const QString &fileName) const
{
    QString foundFileName;
    QStringList filters;
    filters << fileName;
    QDir searchDir(dir);
    searchDir.setNameFilters(filters);
    QStringList filesAndDirs = searchDir.entryList(QDir::Files | QDir::Readable);
    if (!filesAndDirs.isEmpty()) return searchDir.absoluteFilePath(filesAndDirs.at(0));
    searchDir.setNameFilters(QStringList());
    filesAndDirs = searchDir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); i++) {
        foundFileName = searchPathRecursively(searchDir.absoluteFilePath(filesAndDirs.at(i)), fileName);
        if (!foundFileName.isEmpty())
            break;
    }
    return foundFileName;
}

QString DocumentChecker::searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const
{
    QString foundFileName;
    QByteArray fileData;
    QByteArray fileHash;
    QStringList filesAndDirs = dir.entryList(QDir::Files | QDir::Readable);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); i++) {
        QFile file(dir.absoluteFilePath(filesAndDirs.at(i)));
        if (QString::number(file.size()) == matchSize) {
            if (file.open(QIODevice::ReadOnly)) {
                /*
                * 1 MB = 1 second per 450 files (or faster)
                * 10 MB = 9 seconds per 450 files (or faster)
                */
                if (file.size() > 1000000 * 2) {
                    fileData = file.read(1000000);
                    if (file.seek(file.size() - 1000000))
                        fileData.append(file.readAll());
                } else
                    fileData = file.readAll();
                file.close();
                fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
                if (QString(fileHash.toHex()) == matchHash)
                    return file.fileName();
            }
        }
        //kDebug() << filesAndDirs.at(i) << file.size() << fileHash.toHex();
    }
    filesAndDirs = dir.entryList(QDir::Dirs | QDir::Readable | QDir::Executable | QDir::NoDotAndDotDot);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); i++) {
        foundFileName = searchFileRecursively(dir.absoluteFilePath(filesAndDirs.at(i)), matchSize, matchHash);
        if (!foundFileName.isEmpty())
            break;
    }
    return foundFileName;
}

void DocumentChecker::slotEditItem(QTreeWidgetItem *item, int)
{
    int t = item->data(0, typeRole).toInt();
    if (t == TITLE_FONT_ELEMENT || t == UNKNOWN) return;
    //|| t == TITLE_IMAGE_ELEMENT) {

    KUrl url = KUrlRequesterDialog::getUrl(item->text(1), m_dialog, i18n("Enter new location for file"));
    if (url.isEmpty()) return;
    item->setText(1, url.path());
    if (KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, 0)) {
        item->setIcon(0, KIcon("dialog-ok"));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) item->setData(0, statusRole, CLIPOK);
        else item->setData(0, statusRole, LUMAOK);
        checkStatus();
    } else {
        item->setIcon(0, KIcon("dialog-close"));
        int id = item->data(0, statusRole).toInt();
        if (id < 10) item->setData(0, statusRole, CLIPMISSING);
        else item->setData(0, statusRole, LUMAMISSING);
        checkStatus();
    }
}


void DocumentChecker::acceptDialog()
{
    QDomNodeList producers = m_doc.elementsByTagName("producer");
    QDomNodeList infoproducers = m_doc.elementsByTagName("kdenlive_producer");
    int ix = 0;

    // prepare transitions
    QDomNodeList trans = m_doc.elementsByTagName("transition");

    // Mark document as modified
    m_doc.documentElement().setAttribute("modified", 1);

    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == SOURCEMISSING) {
            for (int j = 0; j < child->childCount(); j++) {
                fixClipItem(child->child(j), producers, infoproducers, trans);
            }
        }
        else fixClipItem(child, producers, infoproducers, trans);
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    //QDialog::accept();
}

void DocumentChecker::fixClipItem(QTreeWidgetItem *child, QDomNodeList producers, QDomNodeList infoproducers, QDomNodeList trans)
{
    QDomElement e, property;
    QDomNodeList properties;
    int t = child->data(0, typeRole).toInt();
    if (child->data(0, statusRole).toInt() == CLIPOK) {
        QString id = child->data(0, idRole).toString();
        if (t == TITLE_IMAGE_ELEMENT) {
            // edit images embedded in titles
            for (int i = 0; i < infoproducers.count(); i++) {
                e = infoproducers.item(i).toElement();
                if (e.attribute("id") == id) {
                    // Fix clip
                    QString xml = e.attribute("xmldata");
                    xml.replace(child->data(0, typeOriginalResource).toString(), child->text(1));
                    e.setAttribute("xmldata", xml);
                    break;
                }
            }
            for (int i = 0; i < producers.count(); i++) {
                e = producers.item(i).toElement();
                if (e.attribute("id").section('_', 0, 0) == id) {
                    // Fix clip
                    properties = e.childNodes();
                    for (int j = 0; j < properties.count(); ++j) {
                        property = properties.item(j).toElement();
                        if (property.attribute("name") == "xmldata") {
                            QString xml = property.firstChild().nodeValue();
                            xml.replace(child->data(0, typeOriginalResource).toString(), child->text(1));
                            property.firstChild().setNodeValue(xml);
                            break;
                        }
                    }
                }
            }
        } else {
            // edit clip url
            for (int i = 0; i < infoproducers.count(); i++) {
                e = infoproducers.item(i).toElement();
                if (e.attribute("id") == id) {
                    // Fix clip
                    e.setAttribute("resource", child->text(1));
                    e.setAttribute("name", KUrl(child->text(1)).fileName());
                    e.removeAttribute("_missingsource");
                    break;
                }
            }
            for (int i = 0; i < producers.count(); i++) {
                e = producers.item(i).toElement();
                if (e.attribute("id").section('_', 0, 0) == id || e.attribute("id").section(':', 1, 1) == id) {
                    // Fix clip
                    properties = e.childNodes();
                    for (int j = 0; j < properties.count(); ++j) {
                        property = properties.item(j).toElement();
                        if (property.attribute("name") == "resource") {
                            QString resource = property.firstChild().nodeValue();
                            if (resource.contains(QRegExp("\\?[0-9]+\\.[0-9]+(&amp;strobe=[0-9]+)?$")))
                                property.firstChild().setNodeValue(child->text(1) + '?' + resource.section('?', -1));
                            else
                                property.firstChild().setNodeValue(child->text(1));
                            break;
                        }
                    }
                }
            }
        }
    } else if (child->data(0, statusRole).toInt() == CLIPPLACEHOLDER && t != TITLE_FONT_ELEMENT && t != TITLE_IMAGE_ELEMENT) {
        QString id = child->data(0, idRole).toString();
        for (int i = 0; i < infoproducers.count(); i++) {
            e = infoproducers.item(i).toElement();
            if (e.attribute("id") == id) {
                // Fix clip
                e.setAttribute("placeholder", '1');
                break;
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAOK) {
        for (int i = 0; i < trans.count(); i++) {
            QString luma = getProperty(trans.at(i).toElement(), "luma");
            if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                setProperty(trans.at(i).toElement(), "luma", child->text(1));
                kDebug() << "replace with; " << child->text(1);
            }
        }
    } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
        for (int i = 0; i < trans.count(); i++) {
            QString luma = getProperty(trans.at(i).toElement(), "luma");
            if (!luma.isEmpty() && luma == child->data(0, idRole).toString()) {
                setProperty(trans.at(i).toElement(), "luma", QString());
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
            child->setIcon(0, KIcon("dialog-ok"));
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            child->setData(0, statusRole, LUMAPLACEHOLDER);
            child->setIcon(0, KIcon("dialog-ok"));
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    checkStatus();
}

void DocumentChecker::slotFixDuration()
{
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    QDomNodeList documentProducers = m_doc.elementsByTagName("producer");
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPWRONGDURATION) {
            QString id = child->data(0, idRole).toString();
            bool resetDuration = child->data(0, resetDurationRole).toInt();

            for (int i = 0; i < m_info.count(); i++) {
                QDomElement e = m_info.at(i).toElement();
                if (e.attribute("id") == id) {
                    if (m_missingClips.contains(e)) {
                        // we cannot fix duration of missing clips
                        resetDuration = false;
                    }
                    else {
                        if (resetDuration) e.removeAttribute("duration");
                        else e.setAttribute("duration", child->data(0, sizeRole).toString());
                        child->setData(0, statusRole, CLIPOK);
                        child->setIcon(0, KIcon("dialog-ok"));
                    }
                    break;
                }
            }
            if (resetDuration) {
                // something is wrong in clip durations, so remove them so mlt fetches them again
                for (int j = 0; j < documentProducers.count(); j++) {
                    QDomElement mltProd = documentProducers.at(j).toElement();
                    QString prodId = mltProd.attribute("id");
                    if (prodId == id || prodId.startsWith(id + '_')) {
                        EffectsList::removeProperty(mltProd, "length");
                    }
                }
            }
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    QDomElement infoXml = m_doc.elementsByTagName("kdenlivedoc").at(0).toElement();
    infoXml.setAttribute("modified", "1");
    m_ui.fixDuration->setEnabled(false);
    checkStatus();
}


void DocumentChecker::checkStatus()
{
    bool status = true;
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
        int status = child->data(0, statusRole).toInt();
        if (status == CLIPMISSING || status == LUMAMISSING || status == CLIPWRONGDURATION) {
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
    if (KMessageBox::warningContinueCancel(m_dialog, i18np("This will remove the selected clip from this project", "This will remove the selected clips from this project", m_ui.treeWidget->selectedItems().count()), i18n("Remove clips")) == KMessageBox::Cancel)
        return;
    QStringList deletedIds;
    QStringList deletedLumas;
    QDomNodeList playlists = m_doc.elementsByTagName("playlist");

    foreach(QTreeWidgetItem *child, m_ui.treeWidget->selectedItems()) {
        int id = child->data(0, statusRole).toInt();
        if (id == CLIPMISSING) {
            deletedIds.append(child->data(0, idRole).toString());
            delete child;
        }
        else if (id == LUMAMISSING) {
            deletedLumas.append(child->data(0, idRole).toString());
            delete child;
        }
    }

    if (!deletedLumas.isEmpty()) {
        QDomElement e;
        QDomNodeList transitions = m_doc.elementsByTagName("transition");
        foreach (const QString &lumaPath, deletedLumas) {
            for (int i = 0; i < transitions.count(); i++) {
                e = transitions.item(i).toElement();
                QString resource = EffectsList::property(e, "luma");
                if (resource == lumaPath) EffectsList::removeProperty(e, "luma");
            }
        }
    }

    if (!deletedIds.isEmpty()) {
        QDomElement e;
        QDomNodeList producers = m_doc.elementsByTagName("producer");
        QDomNodeList infoproducers = m_doc.elementsByTagName("kdenlive_producer");

        QDomNode mlt = m_doc.elementsByTagName("mlt").at(0);
        QDomNode kdenlivedoc = m_doc.elementsByTagName("kdenlivedoc").at(0);

        for (int i = 0, j = 0; i < infoproducers.count() && j < deletedIds.count(); i++) {
            e = infoproducers.item(i).toElement();
            if (deletedIds.contains(e.attribute("id"))) {
                // Remove clip
                kdenlivedoc.removeChild(e);
                i--;
                j++;
            }
        }

        for (int i = 0; i < producers.count(); i++) {
            e = producers.item(i).toElement();
            if (deletedIds.contains(e.attribute("id").section('_', 0, 0)) || deletedIds.contains(e.attribute("id").section(':', 1, 1).section('_', 0, 0))) {
                // Remove clip
                mlt.removeChild(e);
                i--;
            }
        }

        for (int i = 0; i < playlists.count(); i++) {
            QDomNodeList entries = playlists.at(i).toElement().elementsByTagName("entry");
            for (int j = 0; j < entries.count(); j++) {
                e = entries.item(j).toElement();
                if (deletedIds.contains(e.attribute("producer").section('_', 0, 0)) || deletedIds.contains(e.attribute("producer").section(':', 1, 1).section('_', 0, 0))) {
                    // Replace clip with blank
                    while (e.childNodes().count() > 0)
                        e.removeChild(e.firstChild());
                    e.setTagName("blank");
                    e.removeAttribute("producer");
                    int length = e.attribute("out").toInt() - e.attribute("in").toInt();
                    e.setAttribute("length", length);
                    j--;
                }
            }
        }
        QDomElement infoXml = m_doc.elementsByTagName("kdenlivedoc").at(0).toElement();
        infoXml.setAttribute("modified", "1");
        checkStatus();
    }
}

void DocumentChecker::checkMissingImagesAndFonts(QStringList images, QStringList fonts, const QString &id, const QString &baseClip)
{
    QDomDocument doc;
    foreach(const QString &img, images) {
        if (m_safeImages.contains(img)) continue;
        if (!KIO::NetAccess::exists(KUrl(img), KIO::NetAccess::SourceSide, 0)) {
            QDomElement e = doc.createElement("missingclip");
            e.setAttribute("type", TITLE_IMAGE_ELEMENT);
            e.setAttribute("resource", img);
            e.setAttribute("id", id);
            e.setAttribute("name", baseClip);
            m_missingClips.append(e);
        }
        else m_safeImages.append(img);
    }
    foreach(const QString &fontelement, fonts) {
        if (m_safeFonts.contains(fontelement)) continue;
        QFont f(fontelement);
        //kDebug() << "/ / / CHK FONTS: " << fontelement << " = " << QFontInfo(f).family();
        if (fontelement != QFontInfo(f).family()) {
            QDomElement e = doc.createElement("missingclip");
            e.setAttribute("type", TITLE_FONT_ELEMENT);
            e.setAttribute("resource", fontelement);
            e.setAttribute("id", id);
            e.setAttribute("name", baseClip);
            m_missingClips.append(e);
        }
        else m_safeFonts.append(fontelement);
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
        } else m_ui.removeSelected->setEnabled(true);
    }

}

#include "documentchecker.moc"


