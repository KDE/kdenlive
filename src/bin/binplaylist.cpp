/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "binplaylist.hpp"
#include "abstractprojectitem.h"
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "profiles/profilemodel.hpp"
#include "projectclip.h"
#include <mlt++/Mlt.h>

QString BinPlaylist::binPlaylistId = QString("main_bin");

BinPlaylist::BinPlaylist(const QUuid &uuid)
    : m_binPlaylist(new Mlt::Playlist(pCore->getProjectProfile()))
    , m_uuid(uuid)
{
    m_binPlaylist->set("id", binPlaylistId.toUtf8().constData());
}

BinPlaylist::~BinPlaylist()
{
    Q_ASSERT(m_binPlaylist->count() == 0);
}

void BinPlaylist::manageBinItemInsertion(const std::shared_ptr<AbstractProjectItem> &binElem)
{
    const QString id = binElem->clipId();
    switch (binElem->itemType()) {
    case AbstractProjectItem::FolderItem: {
        // When a folder is inserted, we have to store its path into the properties
        if (binElem->parent()) {
            QString propertyName = "kdenlive:folder." + binElem->parent()->clipId() + QLatin1Char('.') + id;
            m_binPlaylist->set(propertyName.toUtf8().constData(), binElem->name().toUtf8().constData());
        }
        break;
    }
    case AbstractProjectItem::ClipItem: {
        Q_ASSERT(m_allClips.count(id) == 0);
        auto clip = std::static_pointer_cast<ProjectClip>(binElem);
        if (clip->isValid()) {
            if (clip->clipType() == ClipType::Timeline) {
                const QUuid uuid = clip->getSequenceUuid();
                m_sequenceClips.insert(uuid, id);
                m_binPlaylist->append(clip->originalProducer()->parent());
            } else {
                m_binPlaylist->append(*clip->originalProducer().get());
            }
        } else {
            // if clip is not loaded yet, we insert a dummy producer
            Mlt::Producer dummy(pCore->getProjectProfile(), "color", "blue");
            dummy.set("kdenlive:id", id.toUtf8().constData());
            m_binPlaylist->append(dummy);
        }
        m_allClips.insert(id);
        connect(clip.get(), &ProjectClip::producerChanged, this, &BinPlaylist::changeProducer);
        break;
    }
    default:
        break;
    }
}

const QString BinPlaylist::getSequenceId(const QUuid &uuid)
{
    if (m_sequenceClips.contains(uuid)) {
        return m_sequenceClips.value(uuid);
    }
    return QString();
}

bool BinPlaylist::hasSequenceId(const QUuid &uuid) const
{
    return m_sequenceClips.contains(uuid);
}

QMap<QUuid, QString> BinPlaylist::getAllSequenceClips() const
{
    return m_sequenceClips;
}

void BinPlaylist::manageBinItemDeletion(AbstractProjectItem *binElem)
{
    QString id = binElem->clipId();
    switch (binElem->itemType()) {
    case AbstractProjectItem::FolderItem: {
        // When a folder is removed, we clear the path info
        if (!binElem->lastParentId().isEmpty()) {
            QString propertyName = "kdenlive:folder." + binElem->lastParentId() + QLatin1Char('.') + binElem->clipId();
            m_binPlaylist->set(propertyName.toUtf8().constData(), nullptr);
        }
        break;
    }
    case AbstractProjectItem::ClipItem: {
        Q_ASSERT(m_allClips.count(id) > 0);
        m_allClips.erase(id);
        removeBinClip(id);
        break;
    }
    default:
        break;
    }
}

void BinPlaylist::removeBinClip(const QString &id)
{
    // we iterate on the clips of the timeline to find the correct one
    bool ok = false;
    int size = m_binPlaylist->count();
    for (int i = 0; !ok && i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        QString prodId(prod->parent().get("kdenlive:id"));
        if (prodId == id) {
            m_binPlaylist->remove(i);
            ok = true;
        }
    }
    Q_ASSERT(ok);
}

void BinPlaylist::changeProducer(const QString &id, Mlt::Producer producer)
{
    Q_ASSERT(m_allClips.count(id) > 0);
    removeBinClip(id);
    m_binPlaylist->append(producer);
}

void BinPlaylist::setRetainIn(Mlt::Tractor *modelTractor)
{
    QString retain = QStringLiteral("xml_retain %1").arg(binPlaylistId);
    modelTractor->set(retain.toUtf8().constData(), m_binPlaylist->get_service(), 0);
}

void BinPlaylist::saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata)
{
    // Clear previous properties
    Mlt::Properties playlistProps(m_binPlaylist->get_properties());
    Mlt::Properties docProperties;
    docProperties.pass_values(playlistProps, "kdenlive:docproperties.");
    for (int i = 0; i < docProperties.count(); i++) {
        QString propName = QStringLiteral("kdenlive:docproperties.%1").arg(docProperties.get_name(i));
        playlistProps.set(propName.toUtf8().constData(), nullptr);
    }

    // Clear previous metadata
    Mlt::Properties docMetadata;
    docMetadata.pass_values(playlistProps, "kdenlive:docmetadata.");
    for (int i = 0; i < docMetadata.count(); i++) {
        QString propName = QStringLiteral("kdenlive:docmetadata.%1").arg(docMetadata.get_name(i));
        playlistProps.set(propName.toUtf8().constData(), nullptr);
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

void BinPlaylist::saveProperty(const QString &name, const QString &value)
{
    m_binPlaylist->set(name.toUtf8().constData(), value.toUtf8().constData());
}

QMap<QString, QString> BinPlaylist::getProxies(const QString &root)
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

int BinPlaylist::count() const
{
    return m_binPlaylist->count();
}

void BinPlaylist::manageBinFolderRename(const std::shared_ptr<AbstractProjectItem> &binElem)
{
    QString id = binElem->clipId();
    if (binElem->itemType() != AbstractProjectItem::FolderItem) {
        qDebug() << "// ITEM IS NOT A FOLDER; ABORT RENAME";
    }
    // When a folder is inserted, we have to store its path into the properties
    if (binElem->parent()) {
        QString propertyName = "kdenlive:folder." + binElem->parent()->clipId() + QLatin1Char('.') + id;
        m_binPlaylist->set(propertyName.toUtf8().constData(), binElem->name().toUtf8().constData());
    }
}

const QStringList BinPlaylist::getAllMltIds()
{
    QStringList allIds;
    int size = m_binPlaylist->count();
    for (int i = 0; i < size; i++) {
        QScopedPointer<Mlt::Producer> prod(m_binPlaylist->get_clip(i));
        allIds << QString(prod->parent().get("id"));
    }
    return allIds;
}
