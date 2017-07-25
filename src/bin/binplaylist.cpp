/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "binplaylist.hpp"
#include <mlt++/Mlt.h>
#include "abstractprojectitem.h"
#include "core.h"
#include "projectclip.h"
#include "profiles/profilemodel.hpp"


BinPlaylist::BinPlaylist()
    : m_binPlaylist(new Mlt::Playlist(pCore->getCurrentProfile()->profile()))
{
}

void BinPlaylist::manageBinClipInsertion(const std::shared_ptr<AbstractProjectItem> &binElem)
{
    QString id = binElem->clipId();
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
        qDebug() << "BBBBBBBBBBBBBPPPPPPPPPPPP insert"<<id << clip->isValid();
        if (clip->isValid()) {
            m_binPlaylist->append(*clip->originalProducer().get());
        } else {
            // if clip is not loaded yet, we insert a dummy producer
            Mlt::Producer dummy(pCore->getCurrentProfile()->profile(), "color:blue");
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

void BinPlaylist::manageBinClipDeletion(AbstractProjectItem *binElem)
{
    QString id = binElem->clipId();
    switch (binElem->itemType()) {
    case AbstractProjectItem::FolderItem: {
        // When a folder is removed, we clear the path info
        if (!binElem->lastParentId().isEmpty()) {
            QString propertyName = "kdenlive:folder." + binElem->lastParentId() + QLatin1Char('.') + binElem->clipId();
            m_binPlaylist->set(propertyName.toUtf8().constData(), (char *)nullptr);
        }
        break;
    }
    case AbstractProjectItem::ClipItem: {
        qDebug() << "BBBBBBBBBBBBBPPPPPPPPPPPP delete"<<id;
        Q_ASSERT(m_allClips.count(id) > 0);
        m_allClips.erase(id);
        removeBinClip(id);
        disconnect(static_cast<ProjectClip*>(binElem), &ProjectClip::producerChanged, this, &BinPlaylist::changeProducer);
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
        QString prodId = prod->parent().get("kdenlive:id");
        if (prodId == id) {
            m_binPlaylist->remove(i);
            ok = true;
        }
    }
    Q_ASSERT(ok);
}

void BinPlaylist::changeProducer(const QString &id, const std::shared_ptr<Mlt::Producer> &producer)
{
    qDebug() << "BBBBBBBBBBBBBPPPPPPPPPPPP change"<<id;
    Q_ASSERT(m_allClips.count(id) > 0);
    removeBinClip(id);
    m_binPlaylist->append(*producer.get());
}
