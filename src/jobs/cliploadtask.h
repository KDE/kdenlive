/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
    Q_OBJECT
public:
    ClipLoadTask(const ObjectId &owner, const QDomElement &xml, bool thumbOnly, int in, int out, QObject* object);
    ~ClipLoadTask() override;
    static void start(const ObjectId &owner, const QDomElement &xml, bool thumbOnly, int in, int out, QObject* object, bool force = false, const std::function<void()> &readyCallBack = []() {});
    static ClipType::ProducerType getTypeForService(const QString &id, const QString &path);
    std::shared_ptr<Mlt::Producer> loadResource(QString resource, const QString &type);
    std::shared_ptr<Mlt::Producer> loadPlaylist(QString &resource);
    void processProducerProperties(const std::shared_ptr<Mlt::Producer> &prod, const QDomElement &xml);
    void processSlideShow(std::shared_ptr<Mlt::Producer> producer);

protected:
    void run() override;

private:
    //QString cacheKey();
    QDomElement m_xml;
    int m_in;
    int m_out;
    bool m_thumbOnly;
    QString m_errorMessage;
    void generateThumbnail(std::shared_ptr<ProjectClip>binClip, std::shared_ptr<Mlt::Producer> producer);
    void abort();

signals:
    void taskDone();
};
