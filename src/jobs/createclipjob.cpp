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

#include "klocalizedstring.h"
#include "thumbjob.hpp"

#include <QScopedPointer>

ThumbJob::ThumbJob(const QString &binId)
    : AbstractClipJob(THUMBJOB, binId)
{
}

const QString ThumbJob::getDescription() const
{
    return i18n("Extracting thumb from clip %1", m_clipId);
}

bool ThumbJob::startJob()
{
    // Special case, we just want the thumbnail for existing producer
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(info.clipId);
    Mlt::Producer *prod = new Mlt::Producer(*binClip->originalProducer());
    if ((prod == nullptr) || !prod->is_valid()) {
        return false;
    }

    int frameNumber = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:thumbnailFrame"), QStringLiteral("-1")).toInt();
    if (frameNumber > 0) {
        prod->seek(frameNumber);
    }
    QScopedPointer<Mlt::Frame> frame = prod->get_frame();
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1);
    if ((frame != nullptr) && frame->is_valid()) {
        int fullWidth = info.imageHeight * pCore->getCurrentDar() + 0.5;
        QImage img = KThumb::getFrame(frame, fullWidth, info.imageHeight, forceThumbScale);
        emit replyGetImage(info.clipId, img);
    }
    delete frame;
    delete prod;
    if (info.xml.hasAttribute(QStringLiteral("refreshOnly"))) {
        // inform timeline about change
        emit refreshTimelineProducer(info.clipId);
    }
    return true;
}
