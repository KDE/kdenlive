/*
Copyright (C) 2021  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIPLOADTASK_H
#define CLIPLOADTASK_H

#include "definitions.h"
#include "abstracttask.h"
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <QRunnable>
#include <QDomElement>
#include <QObject>
#include <QList>

class ProjectClip;

class ClipLoadTask : public AbstractTask
{
public:
    ClipLoadTask(const ObjectId &owner, const QDomElement &xml, bool thumbOnly, int in, int out, QObject* object, std::function<void()> readyCallBack);
    virtual ~ClipLoadTask();
    static void start(const ObjectId &owner, const QDomElement &xml, bool thumbOnly, int in, int out, QObject* object, bool force = false, std::function<void()> readyCallBack = []() {});
    static ClipType::ProducerType getTypeForService(const QString &id, const QString &path);
    std::shared_ptr<Mlt::Producer> loadResource(QString resource, const QString &type);
    std::shared_ptr<Mlt::Producer> loadPlaylist(QString &resource);
    void processProducerProperties(const std::shared_ptr<Mlt::Producer> &prod, const QDomElement &xml);
    void processSlideShow(std::shared_ptr<Mlt::Producer> producer);
    // Do some checks on the profile
    static void checkProfile(const QString &clipId, const QDomElement &xml, const std::shared_ptr<Mlt::Producer> &producer);

protected:
    void run() override;

private:
    //QString cacheKey();
    QDomElement m_xml;
    int m_in;
    int m_out;
    bool m_thumbOnly;
    std::function<void()> m_readyCallBack;
    QString m_errorMessage;
    void generateThumbnail(std::shared_ptr<ProjectClip>binClip, std::shared_ptr<Mlt::Producer> producer);
    void abort();
};

#endif // CLIPLOADTASK_H
