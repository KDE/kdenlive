/***************************************************************************
 *   Copyright (C) 2014 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "bincontroller.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectitemmodel.h"
#include "clip.h"
#include "clipcontroller.h"
#include "core.h"
#include "kdenlivesettings.h"

static const char *kPlaylistTrackId = "main bin";

BinController::BinController(const QString &profileName)
    : QObject()
{
    Q_UNUSED(profileName)
    // resetProfile(profileName.isEmpty() ? KdenliveSettings::current_profile() : profileName);
}

BinController::~BinController()
{
    qDebug() << "/// delete bincontroller";
    qDebug() << "REMAINING CLIPS: " << m_clipList.keys();
    destroyBin();
}

void BinController::destroyBin()
{
    if (m_binPlaylist) {
        // m_binPlaylist.release();
        m_binPlaylist->clear();
    }
    qDeleteAll(m_extraClipList);
    m_extraClipList.clear();
    m_clipList.clear();
}

void BinController::loadExtraProducer(const QString &id, Mlt::Producer *prod)
{
    if (m_extraClipList.contains(id)) {
        return;
    }
    m_extraClipList.insert(id, prod);
}

QStringList BinController::getProjectHashes()
{
    QStringList hashes;
    QMapIterator<QString, std::shared_ptr<ClipController>> i(m_clipList);
    hashes.reserve(m_clipList.count());
    while (i.hasNext()) {
        i.next();
        hashes << i.value()->getClipHash();
    }
    hashes.removeDuplicates();
    return hashes;
}


void BinController::slotStoreFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName)
{
    if (!oldParentId.isEmpty()) {
        // Folder was moved, remove old reference
        QString oldPropertyName = "kdenlive:folder." + oldParentId + QLatin1Char('.') + folderId;
        m_binPlaylist->set(oldPropertyName.toUtf8().constData(), (char *)nullptr);
    }
    QString propertyName = "kdenlive:folder." + parentId + QLatin1Char('.') + folderId;
    if (folderName.isEmpty()) {
        // Remove this folder info
        m_binPlaylist->set(propertyName.toUtf8().constData(), (char *)nullptr);
    } else {
        m_binPlaylist->set(propertyName.toUtf8().constData(), folderName.toUtf8().constData());
    }
}


const QString BinController::binPlaylistId()
{
    return kPlaylistTrackId;
}

int BinController::clipCount() const
{
    return m_clipList.size();
}


void BinController::replaceBinPlaylistClip(const QString &id, const std::shared_ptr<Mlt::Producer> &producer)
{
    removeBinPlaylistClip(id);
    m_binPlaylist->append(*producer.get());
}


void BinController::removeBinPlaylistClip(const QString &id)
{
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        QString prodId = prod->parent().get("kdenlive:id");
        if (prodId == id) {
            m_binPlaylist->remove(i);
            break;
        }
    }
}

bool BinController::hasClip(const QString &id)
{
    return m_clipList.contains(id);
}



std::shared_ptr<Mlt::Producer> BinController::getBinProducer(const QString &id)
{
    // TODO: framebuffer speed clips
    if (!m_clipList.contains(id)) {
        qDebug() << "ERROR: requesting invalid bin producer: " << id;
        return nullptr;
    }
    return m_clipList[id]->originalProducer();
}

void BinController::duplicateFilters(std::shared_ptr<Mlt::Producer> original, Mlt::Producer clone)
{
    Mlt::Service clipService(original->get_service());
    Mlt::Service dupService(clone.get_service());
    for (int ix = 0; ix < clipService.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> filter(clipService.filter(ix));
        // Only duplicate Kdenlive filters
        if (filter->is_valid()) {
            QString effectId = filter->get("kdenlive_id");
            if (effectId.isEmpty()) {
                continue;
            }
            // looks like there is no easy way to duplicate a filter,
            // so we will create a new one and duplicate its properties
            auto *dup = new Mlt::Filter(*original->profile(), filter->get("mlt_service"));
            if ((dup != nullptr) && dup->is_valid()) {
                for (int i = 0; i < filter->count(); ++i) {
                    QString paramName = filter->get_name(i);
                    if (paramName.at(0) != QLatin1Char('_')) {
                        dup->set(filter->get_name(i), filter->get(i));
                    }
                }
                dupService.attach(*dup);
            }
            delete dup;
        }
    }
}


QString BinController::xmlFromId(const QString &id)
{
    if (!m_clipList.contains(id)) {
        qDebug() << "Error: impossible to retrieve xml from unknown bin clip";
        return QString();
    }
    std::shared_ptr<ClipController> controller = m_clipList[id];
    std::shared_ptr<Mlt::Producer> original = controller->originalProducer();
    QString xml = getProducerXML(original);
    QDomDocument mltData;
    mltData.setContent(xml);
    QDomElement producer = mltData.documentElement().firstChildElement(QStringLiteral("producer"));
    QString str;
    QTextStream stream(&str);
    producer.save(stream, 4);
    return str;
}

// static
QString BinController::getProducerXML(const std::shared_ptr<Mlt::Producer> &producer, bool includeMeta)
{
    Mlt::Consumer c(*producer->profile(), "xml", "string");
    Mlt::Service s(producer->get_service());
    if (!s.is_valid()) {
        return QString();
    }
    int ignore = s.get_int("ignore_points");
    if (ignore != 0) {
        s.set("ignore_points", 0);
    }
    c.set("time_format", "frames");
    if (!includeMeta) {
        c.set("no_meta", 1);
    }
    c.set("store", "kdenlive");
    c.set("no_root", 1);
    c.set("root", "/");
    c.connect(s);
    c.start();
    if (ignore != 0) {
        s.set("ignore_points", ignore);
    }
    return QString::fromUtf8(c.get("string"));
}

std::shared_ptr<ClipController> BinController::getController(const QString &id)
{
    if (!m_clipList.contains(id)) {
        qDebug() << "Error: invalid bin clip requested" << id;
        Q_ASSERT(false);
    }
    return m_clipList.value(id);
}

const QList<std::shared_ptr<ClipController>> BinController::getControllerList() const
{
    return m_clipList.values();
}

const QStringList BinController::getBinIdsByResource(const QFileInfo &url) const
{
    QStringList controllers;
    QMapIterator<QString, std::shared_ptr<ClipController>> i(m_clipList);
    while (i.hasNext()) {
        i.next();
        auto ctrl = i.value();
        if (QFileInfo(ctrl->clipUrl()) == url) {
            controllers << i.key();
        }
    }
    return controllers;
}

void BinController::updateTrackProducer(const QString &id)
{
    emit updateTimelineProducer(id);
}



void BinController::saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata,
                                           std::shared_ptr<MarkerListModel> guideModel)
{
    Q_UNUSED(guideModel)
    // Clear previous properites
    Mlt::Properties playlistProps(m_binPlaylist->get_properties());
    Mlt::Properties docProperties;
    docProperties.pass_values(playlistProps, "kdenlive:docproperties.");
    for (int i = 0; i < docProperties.count(); i++) {
        QString propName = QStringLiteral("kdenlive:docproperties.") + docProperties.get_name(i);
        playlistProps.set(propName.toUtf8().constData(), (char *)nullptr);
    }

    // Clear previous metadata
    Mlt::Properties docMetadata;
    docMetadata.pass_values(playlistProps, "kdenlive:docmetadata.");
    for (int i = 0; i < docMetadata.count(); i++) {
        QString propName = QStringLiteral("kdenlive:docmetadata.") + docMetadata.get_name(i);
        playlistProps.set(propName.toUtf8().constData(), (char *)nullptr);
    }

    QMapIterator<QString, QString> i(props);
    while (i.hasNext()) {
        i.next();
        playlistProps.set(("kdenlive:docproperties." + i.key()).toUtf8().constData(), i.value().toUtf8().constData());
    }

    QMapIterator<QString, QString> j(metadata);
    while (j.hasNext()) {
        j.next();
        playlistProps.set(("kdenlive:docmetadata." + j.key()).toUtf8().constData(), j.value().toUtf8().constData());
    }
}

void BinController::saveProperty(const QString &name, const QString &value)
{
    m_binPlaylist->set(name.toUtf8().constData(), value.toUtf8().constData());
}

const QString BinController::getProperty(const QString &name)
{
    return QString(m_binPlaylist->get(name.toUtf8().constData()));
}

QMap<QString, QString> BinController::getProxies(const QString &root)
{
    QMap<QString, QString> proxies;
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        if (!prod->is_valid() || prod->is_blank()) {
            continue;
        }
        QString proxy = prod->parent().get("kdenlive:proxy");
        if (proxy.length() > 2) {
            if (QFileInfo(proxy).isRelative()) {
                proxy.prepend(root);
            }
            QString sourceUrl(prod->parent().get("kdenlive:originalurl"));
            if (QFileInfo(sourceUrl).isRelative()) {
                sourceUrl.prepend(root);
            }
            proxies.insert(proxy, sourceUrl);
        }
    }
    return proxies;
}
