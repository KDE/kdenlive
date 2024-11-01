/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "playlistsubclip.h"
#include "bincommands.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "jobs/cachetask.h"
#include "jobs/cliploadtask.h"
#include "projectclip.h"
#include "projectitemmodel.h"
#include "utils/thumbnailcache.hpp"

#include <KLocalizedString>
#include <QDomElement>
#include <QPainter>
#include <QPainterPath>
#include <QTemporaryFile>
#include <QtMath>
#include <utility>

class ClipController;

PlaylistSubClip::PlaylistSubClip(const QString &id, const std::shared_ptr<ProjectClip> &parent, const std::shared_ptr<ProjectItemModel> &model,
                                 const QString &timecode, const QMap<QString, QString> &zoneProperties)
    : AbstractProjectItem(AbstractProjectItem::SubSequenceItem, id, model)
    , m_masterClip(parent)
{
    m_duration = timecode;
    m_parentClipId = m_masterClip->clipId();

    QPixmap pix(64, 36);
    pix.fill(Qt::lightGray);
    m_thumbnail = QIcon(pix);
    m_name = zoneProperties.value(QLatin1String("name"));
    m_sequenceUuid = QUuid(zoneProperties.value(QLatin1String("uuid")));
    m_rating = zoneProperties.value(QLatin1String("rating")).toUInt();
    m_tags = zoneProperties.value(QLatin1String("tags"));
    qDebug() << "=== LOADING SUBCLIP WITH RATING: " << m_rating << ", TAGS: " << m_tags;
    m_clipStatus = FileStatus::StatusReady;
    m_clipIdWithSequence = m_parentClipId;
    m_clipIdWithSequence.append(m_sequenceUuid.toString());

    QDomDocument doc;
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute(QStringLiteral("sequenceUuid"), m_sequenceUuid.toString());
    doc.appendChild(prod);
    ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, m_parentClipId.toInt(), QUuid()), prod, true, 0, -1, this);
}

std::shared_ptr<PlaylistSubClip> PlaylistSubClip::construct(const QString &id, const std::shared_ptr<ProjectClip> &parent,
                                                            const std::shared_ptr<ProjectItemModel> &model, const QString &timecode,
                                                            const QMap<QString, QString> &zoneProperties)
{
    std::shared_ptr<PlaylistSubClip> self(new PlaylistSubClip(id, parent, model, timecode, zoneProperties));
    baseFinishConstruct(self);
    return self;
}

PlaylistSubClip::~PlaylistSubClip()
{
    // controller is deleted in bincontroller
}

const QString &PlaylistSubClip::clipId(bool withSequence) const
{
    if (withSequence) {
        return m_clipIdWithSequence;
    }
    return m_binId;
}

const QString PlaylistSubClip::cutClipId() const
{
    return QString("%1/%2/%3").arg(m_parentClipId).arg(m_inPoint).arg(m_outPoint);
}

void PlaylistSubClip::gotThumb(int pos, const QImage &img)
{
    if (pos == m_inPoint) {
        setThumbnail(img);
        disconnect(m_masterClip.get(), &ProjectClip::thumbReady, this, &PlaylistSubClip::gotThumb);
    }
}

QString PlaylistSubClip::getToolTip() const
{
    return QString("%1-%2").arg(m_inPoint).arg(m_outPoint);
}

std::shared_ptr<ProjectClip> PlaylistSubClip::clip(const QString &id)
{
    Q_UNUSED(id);
    return std::shared_ptr<ProjectClip>();
}

std::shared_ptr<ProjectFolder> PlaylistSubClip::folder(const QString &id)
{
    Q_UNUSED(id);
    return std::shared_ptr<ProjectFolder>();
}

void PlaylistSubClip::setBinEffectsEnabled(bool) {}

GenTime PlaylistSubClip::duration() const
{
    // TODO
    return {};
}

QPoint PlaylistSubClip::zone() const
{
    return {m_inPoint, m_outPoint};
}

std::shared_ptr<ProjectClip> PlaylistSubClip::clipAt(int ix)
{
    Q_UNUSED(ix);
    return std::shared_ptr<ProjectClip>();
}

QDomElement PlaylistSubClip::toXml(QDomDocument &document, bool, bool)
{
    QDomElement sub = document.createElement(QStringLiteral("subclip"));
    sub.setAttribute(QStringLiteral("id"), m_masterClip->AbstractProjectItem::clipId());
    sub.setAttribute(QStringLiteral("in"), m_inPoint);
    sub.setAttribute(QStringLiteral("out"), m_outPoint);
    return sub;
}

std::shared_ptr<PlaylistSubClip> PlaylistSubClip::subClip(int in, int out)
{
    if (m_inPoint == in && m_outPoint == out) {
        return std::static_pointer_cast<PlaylistSubClip>(shared_from_this());
    }
    return std::shared_ptr<PlaylistSubClip>();
}

void PlaylistSubClip::setThumbnail(const QImage &img)
{
    if (img.isNull()) {
        return;
    }
    QPixmap thumb = roundedPixmap(QPixmap::fromImage(img));
    m_thumbnail = QIcon(thumb);
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<PlaylistSubClip>(shared_from_this()),
                                                                       {AbstractProjectItem::DataThumbnail});
}

QPixmap PlaylistSubClip::thumbnail(int width, int height)
{
    return m_thumbnail.pixmap(width, height);
}

bool PlaylistSubClip::rename(const QString &name, int column)
{
    // TODO refac: rework this
    Q_UNUSED(column)
    if (m_name == name) {
        return false;
    }
    // Rename folder
    auto *command = new RenameBinSubClipCommand(pCore->bin(), m_masterClip->clipId(), name, m_name, m_inPoint, m_outPoint);
    pCore->currentDoc()->commandStack()->push(command);
    return true;
}

std::shared_ptr<ProjectClip> PlaylistSubClip::getMasterClip() const
{
    return m_masterClip;
}

ClipType::ProducerType PlaylistSubClip::clipType() const
{
    return m_masterClip->clipType();
}

const QUuid PlaylistSubClip::sequenceUuid() const
{
    return m_sequenceUuid;
}

bool PlaylistSubClip::hasAudioAndVideo() const
{
    return m_masterClip->hasAudioAndVideo();
}

void PlaylistSubClip::getThumbFromPercent(int percent)
{
    // extract a maximum of 30 frames for bin preview
    if (percent < 0) {
        setThumbnail(ThumbnailCache::get()->getThumbnail(m_binId, m_inPoint));
        return;
    }
    int duration = m_outPoint - m_inPoint;
    int steps = qCeil(qMax(pCore->getCurrentFps(), double(duration) / 30));
    int framePos = duration * percent / 100;
    framePos -= framePos % steps;
    if (ThumbnailCache::get()->hasThumbnail(m_parentClipId, m_inPoint + framePos)) {
        setThumbnail(ThumbnailCache::get()->getThumbnail(m_parentClipId, m_inPoint + framePos));
    } else {
        // Generate percent thumbs
        CacheTask::start(ObjectId(KdenliveObjectType::BinClip, m_parentClipId.toInt(), QUuid()), 30, m_inPoint, m_outPoint, this);
    }
}

void PlaylistSubClip::setProperties(const QMap<QString, QString> &properties)
{
    bool propertyFound = false;
    if (properties.contains(QStringLiteral("kdenlive:tags"))) {
        propertyFound = true;
        m_tags = properties.value(QStringLiteral("kdenlive:tags"));
    }
    if (properties.contains(QStringLiteral("kdenlive:rating"))) {
        propertyFound = true;
        m_rating = properties.value(QStringLiteral("kdenlive:rating")).toUInt();
    }
    if (!propertyFound) {
        return;
    }
    if (auto ptr = m_model.lock()) {
        std::shared_ptr<AbstractProjectItem> parentItem = std::static_pointer_cast<ProjectItemModel>(ptr)->getItemByBinId(m_parentClipId);
        if (parentItem && parentItem->itemType() == AbstractProjectItem::ClipItem) {
            auto clipItem = std::static_pointer_cast<ProjectClip>(parentItem);
            clipItem->updateZones();
        }
    }
}

void PlaylistSubClip::setRating(uint rating)
{
    AbstractProjectItem::setRating(rating);
    if (auto ptr = m_model.lock()) {
        std::shared_ptr<AbstractProjectItem> parentItem = std::static_pointer_cast<ProjectItemModel>(ptr)->getItemByBinId(m_parentClipId);
        if (parentItem && parentItem->itemType() == AbstractProjectItem::ClipItem) {
            auto clipItem = std::static_pointer_cast<ProjectClip>(parentItem);
            clipItem->updateZones();
        }
    }
    pCore->currentDoc()->setModified(true);
}
