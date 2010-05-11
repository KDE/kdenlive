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

const int CLIPMISSING = 0;
const int CLIPOK = 1;
const int CLIPPLACEHOLDER = 2;
const int LUMAMISSING = 10;
const int LUMAOK = 11;
const int LUMAPLACEHOLDER = 12;

enum TITLECLIPTYPE { TITLE_IMAGE_ELEMENT = 20, TITLE_FONT_ELEMENT = 21 };

DocumentChecker::DocumentChecker(QDomNodeList infoproducers, QDomDocument doc):
        m_info(infoproducers), m_doc(doc), m_dialog(NULL)
{

}


bool DocumentChecker::hasMissingClips()
{
    int clipType;
    QDomElement e;
    QString id;
    QString resource;
    QList <QDomElement> missingClips;
    for (int i = 0; i < m_info.count(); i++) {
        e = m_info.item(i).toElement();
        clipType = e.attribute("type").toInt();
        if (clipType == COLOR) continue;
        if (clipType == TEXT) {
            //TODO: Check is clip template is missing (xmltemplate) or hash changed
            QStringList images = TitleWidget::extractImageList(e.attribute("xmldata"));
            QStringList fonts = TitleWidget::extractFontList(e.attribute("xmldata"));
            checkMissingImages(missingClips, images, fonts, e.attribute("id"), e.attribute("name"));
            continue;
        }
        id = e.attribute("id");
        resource = e.attribute("resource");
        if (clipType == SLIDESHOW) resource = KUrl(resource).directory();
        if (!KIO::NetAccess::exists(KUrl(resource), KIO::NetAccess::SourceSide, 0)) {
            // Missing clip found
            missingClips.append(e);
        } else {
            // Check if the clip has changed
            if (clipType != SLIDESHOW && e.hasAttribute("file_hash")) {
                if (e.attribute("file_hash") != DocClipBase::getHash(e.attribute("resource")))
                    e.removeAttribute("file_hash");
            }
        }
    }

    QStringList missingLumas;
    QString root = m_doc.documentElement().attribute("root");
    if (!root.isEmpty()) root = KUrl(root).path(KUrl::AddTrailingSlash);
    QDomNodeList trans = m_doc.elementsByTagName("transition");
    for (int i = 0; i < trans.count(); i++) {
        QString luma = getProperty(trans.at(i).toElement(), "luma");
        if (!luma.startsWith('/')) luma.prepend(root);
        if (!luma.isEmpty() && !QFile::exists(luma)) {
            if (!missingLumas.contains(luma)) {
                missingLumas.append(luma);
            }
        }
    }

    if (missingClips.isEmpty() && missingLumas.isEmpty()) {
        return false;
    }
    m_dialog = new QDialog();
    m_dialog->setFont(KGlobalSettings::toolBarFont());
    m_ui.setupUi(m_dialog);

    foreach(const QString l, missingLumas) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << i18n("Luma file") << l);
        item->setIcon(0, KIcon("dialog-close"));
        item->setData(0, idRole, l);
        item->setData(0, statusRole, LUMAMISSING);
    }

    m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    for (int i = 0; i < missingClips.count(); i++) {
        e = missingClips.at(i).toElement();
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
    }
    connect(m_ui.recursiveSearch, SIGNAL(pressed()), this, SLOT(slotSearchClips()));
    connect(m_ui.usePlaceholders, SIGNAL(pressed()), this, SLOT(slotPlaceholders()));
    connect(m_ui.removeSelected, SIGNAL(pressed()), this, SLOT(slotDeleteSelected()));
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
    m_ui.recursiveSearch->setEnabled(false);
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            QString clipPath = searchFileRecursively(QDir(newpath), child->data(0, sizeRole).toString(), child->data(0, hashRole).toString());
            if (!clipPath.isEmpty()) {
                child->setText(1, clipPath);
                child->setIcon(0, KIcon("dialog-ok"));
                child->setData(0, statusRole, CLIPOK);
            }
        } else if (child->data(0, statusRole).toInt() == LUMAMISSING) {
            QString fileName = searchLuma(child->data(0, idRole).toString());
            if (!fileName.isEmpty()) {
                child->setText(1, fileName);
                child->setIcon(0, KIcon("dialog-ok"));
                child->setData(0, statusRole, LUMAOK);
            }
        }
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    m_ui.recursiveSearch->setEnabled(true);
    checkStatus();
}


QString DocumentChecker::searchLuma(QString file) const
{
    KUrl searchPath(KdenliveSettings::mltpath());
    if (file.contains("PAL"))
        searchPath.cd("../lumas/PAL");
    else
        searchPath.cd("../lumas/NTSC");
    QString result = searchPath.path(KUrl::AddTrailingSlash) + KUrl(file).fileName();
    if (QFile::exists(result))
        return result;
    return QString();
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
                if (file.size() > 1000000*2) {
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
    if (t == TITLE_FONT_ELEMENT) return;
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
    QDomElement e, property;
    QDomNodeList producers = m_doc.elementsByTagName("producer");
    QDomNodeList infoproducers = m_doc.elementsByTagName("kdenlive_producer");
    QDomNodeList properties;
    int ix = 0;

    // prepare transitions
    QDomNodeList trans = m_doc.elementsByTagName("transition");

    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
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
                            if (property.attribute("name") == "resource") {
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
                kDebug() << "luma: " << luma;
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
        ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    //QDialog::accept();
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


void DocumentChecker::checkStatus()
{
    bool status = true;
    int ix = 0;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING || child->data(0, statusRole).toInt() == LUMAMISSING) {
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
    if (KMessageBox::warningContinueCancel(m_dialog, i18np("This will remove the selected clip from this project", "This will remove the selected clips from this project", m_ui.treeWidget->selectedItems().count()), i18n("Remove clips")) == KMessageBox::Cancel) return;
    int ix = 0;
    QStringList deletedIds;
    QTreeWidgetItem *child = m_ui.treeWidget->topLevelItem(ix);
    QDomNodeList playlists = m_doc.elementsByTagName("playlist");

    while (child) {
        int id = child->data(0, statusRole).toInt();
        if (child->isSelected() && id < 10) {
            QString id = child->data(0, idRole).toString();
            deletedIds.append(id);
            for (int j = 0; j < playlists.count(); j++)
                deletedIds.append(id + '_' + QString::number(j));
            delete child;
        } else ix++;
        child = m_ui.treeWidget->topLevelItem(ix);
    }
    kDebug() << "// Clips to delete: " << deletedIds;

    if (!deletedIds.isEmpty()) {
        QDomElement e;
        QDomNodeList producers = m_doc.elementsByTagName("producer");
        QDomNodeList infoproducers = m_doc.elementsByTagName("kdenlive_producer");

        QDomElement mlt = m_doc.firstChildElement("mlt");
        QDomElement kdenlivedoc = mlt.firstChildElement("kdenlivedoc");

        for (int i = 0; i < infoproducers.count(); i++) {
            e = infoproducers.item(i).toElement();
            if (deletedIds.contains(e.attribute("id"))) {
                // Remove clip
                kdenlivedoc.removeChild(e);
                break;
            }
        }

        for (int i = 0; i < producers.count(); i++) {
            e = producers.item(i).toElement();
            if (deletedIds.contains(e.attribute("id"))) {
                // Remove clip
                mlt.removeChild(e);
                break;
            }
        }

        for (int i = 0; i < playlists.count(); i++) {
            QDomNodeList entries = playlists.at(i).toElement().elementsByTagName("entry");
            for (int j = 0; j < playlists.count(); j++) {
                e = entries.item(j).toElement();
                if (deletedIds.contains(e.attribute("producer"))) {
                    // Replace clip with blank
                    e.setTagName("blank");
                    e.removeAttribute("producer");
                    int length = e.attribute("out").toInt() - e.attribute("in").toInt();
                    e.setAttribute("length", length);
                }
            }
        }
        checkStatus();
    }
}

void DocumentChecker::checkMissingImages(QList <QDomElement>&missingClips, QStringList images, QStringList fonts, QString id, QString baseClip)
{
    QDomDocument doc;
    foreach(const QString &img, images) {
        if (!KIO::NetAccess::exists(KUrl(img), KIO::NetAccess::SourceSide, 0)) {
            QDomElement e = doc.createElement("missingclip");
            e.setAttribute("type", TITLE_IMAGE_ELEMENT);
            e.setAttribute("resource", img);
            e.setAttribute("id", id);
            e.setAttribute("name", baseClip);
            missingClips.append(e);
        }
    }
    kDebug() << "/ / / CHK FONTS: " << fonts;
    foreach(const QString &fontelement, fonts) {
        QFont f(fontelement);
        kDebug() << "/ / / CHK FONTS: " << fontelement << " = " << QFontInfo(f).family();
        if (fontelement != QFontInfo(f).family()) {
            QDomElement e = doc.createElement("missingclip");
            e.setAttribute("type", TITLE_FONT_ELEMENT);
            e.setAttribute("resource", fontelement);
            e.setAttribute("id", id);
            e.setAttribute("name", baseClip);
            missingClips.append(e);
        }
    }
}


void DocumentChecker::slotCheckButtons()
{
    if (m_ui.treeWidget->currentItem()) {
        QTreeWidgetItem *item = m_ui.treeWidget->currentItem();
        int t = item->data(0, typeRole).toInt();
        if (t == TITLE_FONT_ELEMENT || t == TITLE_IMAGE_ELEMENT) {
            m_ui.removeSelected->setEnabled(false);
        } else m_ui.removeSelected->setEnabled(true);
    }

}

#include "documentchecker.moc"


