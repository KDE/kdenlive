/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectsubclip.h"
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
#include <QtMath>
#include <utility>

class ClipController;

ProjectSubClip::ProjectSubClip(const QString &id, const std::shared_ptr<ProjectClip> &parent, const std::shared_ptr<ProjectItemModel> &model, int in, int out,
                               const QString &timecode, const QMap<QString, QString> &zoneProperties)
    : AbstractProjectItem(AbstractProjectItem::SubClipItem, id, model)
    , m_masterClip(parent)
{
    m_inPoint = in;
    m_outPoint = out;
    m_duration = timecode;
    m_parentDuration = int(m_masterClip->frameDuration());
    m_parentClipId = m_masterClip->clipId();
    m_date = parent->date.addSecs(in);
    QPixmap pix(64, 36);
    pix.fill(Qt::lightGray);
    m_thumbnail = QIcon(pix);
    m_name = zoneProperties.value(QLatin1String("name"));
    if (m_name.isEmpty()) {
        m_name = i18n("Zone %1", parent->childCount() + 1);
    }
    m_rating = zoneProperties.value(QLatin1String("rating")).toUInt();
    m_tags = zoneProperties.value(QLatin1String("tags"));
    qDebug() << "=== LOADING SUBCLIP WITH RATING: " << m_rating << ", TAGS: " << m_tags;
    m_clipStatus = FileStatus::StatusReady;
    ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, m_parentClipId.toInt(), QUuid()), QDomElement(), true, in, out, this);
}

std::shared_ptr<ProjectSubClip> ProjectSubClip::construct(const QString &id, const std::shared_ptr<ProjectClip> &parent,
                                                          const std::shared_ptr<ProjectItemModel> &model, int in, int out, const QString &timecode,
                                                          const QMap<QString, QString> &zoneProperties)
{
    std::shared_ptr<ProjectSubClip> self(new ProjectSubClip(id, parent, model, in, out, timecode, zoneProperties));
    baseFinishConstruct(self);
    return self;
}

ProjectSubClip::~ProjectSubClip()
{
    // controller is deleted in bincontroller
}

const QString ProjectSubClip::cutClipId() const
{
    return QStringLiteral("%1/%2/%3").arg(m_parentClipId).arg(m_inPoint).arg(m_outPoint);
}

void ProjectSubClip::gotThumb(int pos, const QImage &img)
{
    if (pos == m_inPoint) {
        setThumbnail(img);
        disconnect(m_masterClip.get(), &ProjectClip::thumbReady, this, &ProjectSubClip::gotThumb);
    }
}

QString ProjectSubClip::getToolTip() const
{
    return QStringLiteral("%1-%2").arg(m_inPoint).arg(m_outPoint);
}

std::shared_ptr<ProjectClip> ProjectSubClip::clip(const QString &id)
{
    Q_UNUSED(id);
    return std::shared_ptr<ProjectClip>();
}

std::shared_ptr<ProjectFolder> ProjectSubClip::folder(const QString &id)
{
    Q_UNUSED(id);
    return std::shared_ptr<ProjectFolder>();
}

void ProjectSubClip::setBinEffectsEnabled(bool) {}

GenTime ProjectSubClip::duration() const
{
    // TODO
    return {};
}

QPoint ProjectSubClip::zone() const
{
    return {m_inPoint, m_outPoint};
}

std::shared_ptr<ProjectClip> ProjectSubClip::clipAt(int ix)
{
    Q_UNUSED(ix);
    return std::shared_ptr<ProjectClip>();
}

QDomElement ProjectSubClip::toXml(QDomDocument &document, bool, bool)
{
    QDomElement sub = document.createElement(QStringLiteral("subclip"));
    sub.setAttribute(QStringLiteral("id"), m_masterClip->AbstractProjectItem::clipId());
    sub.setAttribute(QStringLiteral("in"), m_inPoint);
    sub.setAttribute(QStringLiteral("out"), m_outPoint);
    return sub;
}

std::shared_ptr<ProjectSubClip> ProjectSubClip::subClip(int in, int out)
{
    if (m_inPoint == in && m_outPoint == out) {
        return std::static_pointer_cast<ProjectSubClip>(shared_from_this());
    }
    return std::shared_ptr<ProjectSubClip>();
}

void ProjectSubClip::setThumbnail(const QImage &img)
{
    if (img.isNull()) {
        return;
    }
    QPixmap thumb = roundedPixmap(QPixmap::fromImage(img));
    int duration = m_parentDuration;
    double factor = double(thumb.width()) / duration;
    int zoneOut = m_outPoint - duration;
    QRect zoneRect(0, 0, thumb.width(), thumb.height());
    zoneRect.adjust(0, int(zoneRect.height() * 0.9), 0, int(-zoneRect.height() * 0.05));
    QPainter painter(&thumb);
    painter.fillRect(zoneRect, Qt::darkGreen);
    zoneRect.adjust(int(m_inPoint * factor), 0, int(zoneOut * factor), 0);
    painter.fillRect(zoneRect, Qt::green);
    painter.end();
    m_thumbnail = QIcon(thumb);
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectSubClip>(shared_from_this()),
                                                                       {AbstractProjectItem::DataThumbnail});
}

QPixmap ProjectSubClip::thumbnail(int width, int height)
{
    return m_thumbnail.pixmap(width, height);
}

bool ProjectSubClip::rename(const QString &name, int column)
{
    // TODO refac: rework this
    Q_UNUSED(column)
    if (m_name == name || name.isEmpty()) {
        return false;
    }
    // Rename folder
    auto *command = new RenameBinSubClipCommand(pCore->bin(), m_masterClip->clipId(), name, m_name, m_inPoint, m_outPoint);
    pCore->currentDoc()->commandStack()->push(command);
    return true;
}

std::shared_ptr<ProjectClip> ProjectSubClip::getMasterClip() const
{
    return m_masterClip;
}

ClipType::ProducerType ProjectSubClip::clipType() const
{
    return m_masterClip->clipType();
}

bool ProjectSubClip::hasAudioAndVideo() const
{
    return m_masterClip->hasAudioAndVideo();
}

void ProjectSubClip::getThumbFromPercent(int percent)
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

void ProjectSubClip::setProperties(const QMap<QString, QString> &properties)
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

void ProjectSubClip::setRating(uint rating)
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
