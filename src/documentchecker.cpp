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
#include "definitions.h"

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

const int CLIPMISSING = 0;
const int CLIPOK = 1;
const int CLIPPLACEHOLDER = 2;

DocumentChecker::DocumentChecker(QDomNodeList producers, QDomNodeList infoproducers, QList <QDomElement> missingClips, QDomDocument doc, QWidget * parent) :
        QDialog(parent),
        m_doc(doc)
{
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    QDomElement e;

    m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    for (int i = 0; i < missingClips.count(); i++) {
        e = missingClips.at(i).toElement();
        QString clipType;
        switch (e.attribute("type").toInt()) {
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
        QTreeWidgetItem *item = new QTreeWidgetItem(m_view.treeWidget, QStringList() << clipType << e.attribute("resource"));
        item->setIcon(0, KIcon("dialog-close"));
        item->setData(0, hashRole, e.attribute("file_hash"));
        item->setData(0, sizeRole, e.attribute("file_size"));
        item->setData(0, idRole, e.attribute("id"));
        item->setData(0, statusRole, CLIPMISSING);
    }
    connect(m_view.recursiveSearch, SIGNAL(pressed()), this, SLOT(slotSearchClips()));
    connect(m_view.usePlaceholders, SIGNAL(pressed()), this, SLOT(slotPlaceholders()));
    connect(m_view.removeSelected, SIGNAL(pressed()), this, SLOT(slotDeleteSelected()));
    connect(m_view.treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(slotEditItem(QTreeWidgetItem *, int)));
    //adjustSize();
}

DocumentChecker::~DocumentChecker() {}

void DocumentChecker::slotSearchClips()
{
    QString newpath = KFileDialog::getExistingDirectory(KUrl("kfiledialog:///clipfolder"), kapp->activeWindow(), i18n("Clips folder"));
    if (newpath.isEmpty()) return;
    int ix = 0;
    m_view.recursiveSearch->setEnabled(false);
    QTreeWidgetItem *child = m_view.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            QString clipPath = searchFileRecursively(QDir(newpath), child->data(0, sizeRole).toString(), child->data(0, hashRole).toString());
            if (!clipPath.isEmpty()) {
                child->setText(1, clipPath);
                child->setIcon(0, KIcon("dialog-ok"));
                child->setData(0, statusRole, CLIPOK);
            }
        }
        ix++;
        child = m_view.treeWidget->topLevelItem(ix);
    }
    m_view.recursiveSearch->setEnabled(true);
    checkStatus();
}

QString DocumentChecker::searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const
{
    QString foundFileName;
    QByteArray fileData;
    QByteArray fileHash;
    QStringList filesAndDirs = dir.entryList(QDir::Files | QDir::Readable);
    for (int i = 0; i < filesAndDirs.size() && foundFileName.isEmpty(); i++) {
        QFile file(dir.absoluteFilePath(filesAndDirs.at(i)));
        if (file.open(QIODevice::ReadOnly)) {
            if (QString::number(file.size()) == matchSize) {
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
    KUrl url = KUrlRequesterDialog::getUrl(item->text(1), this, i18n("Enter new location for file"));
    if (url.isEmpty()) return;
    item->setText(1, url.path());
    if (KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, 0)) {
        item->setIcon(0, KIcon("dialog-ok"));
        item->setData(0, statusRole, CLIPOK);
        checkStatus();
    }
}

// virtual
void DocumentChecker::accept()
{
    QDomElement e, property;
    QDomNodeList producers = m_doc.elementsByTagName("producer");
    QDomNodeList infoproducers = m_doc.elementsByTagName("kdenlive_producer");
    QDomNodeList properties;
    int ix = 0;
    QTreeWidgetItem *child = m_view.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPOK) {
            QString id = child->data(0, idRole).toString();
            for (int i = 0; i < infoproducers.count(); i++) {
                e = infoproducers.item(i).toElement();
                if (e.attribute("id") == id) {
                    // Fix clip
                    e.setAttribute("resource", child->text(1));
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
                    break;
                }
            }
        } else if (child->data(0, statusRole).toInt() == CLIPPLACEHOLDER) {
            QString id = child->data(0, idRole).toString();
            for (int i = 0; i < infoproducers.count(); i++) {
                e = infoproducers.item(i).toElement();
                if (e.attribute("id") == id) {
                    // Fix clip
                    e.setAttribute("placeholder", '1');
                    break;
                }
            }
        }
        ix++;
        child = m_view.treeWidget->topLevelItem(ix);
    }
    QDialog::accept();
}

void DocumentChecker::slotPlaceholders()
{
    int ix = 0;
    QTreeWidgetItem *child = m_view.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            child->setData(0, statusRole, CLIPPLACEHOLDER);
            child->setIcon(0, KIcon("dialog-ok"));
        }
        ix++;
        child = m_view.treeWidget->topLevelItem(ix);
    }
    checkStatus();
}


void DocumentChecker::checkStatus()
{
    bool status = true;
    int ix = 0;
    QTreeWidgetItem *child = m_view.treeWidget->topLevelItem(ix);
    while (child) {
        if (child->data(0, statusRole).toInt() == CLIPMISSING) {
            status = false;
            break;
        }
        ix++;
        child = m_view.treeWidget->topLevelItem(ix);
    }
    m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}


void DocumentChecker::slotDeleteSelected()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will remove the selected clips from this project"), i18n("Remove clips")) == KMessageBox::Cancel) return;
    int ix = 0;
    QStringList deletedIds;
    QTreeWidgetItem *child = m_view.treeWidget->topLevelItem(ix);
    QDomNodeList playlists = m_doc.elementsByTagName("playlist");

    while (child) {
        if (child->isSelected()) {
            QString id = child->data(0, idRole).toString();
            deletedIds.append(id);
            for (int j = 0; j < playlists.count(); j++)
                deletedIds.append(id + '_' + QString::number(j));
            delete child;
        } else ix++;
        child = m_view.treeWidget->topLevelItem(ix);
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

#include "documentchecker.moc"


