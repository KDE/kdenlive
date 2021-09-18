/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef CACHETASK_H
#define CACHETASK_H

#include "definitions.h"
#include "abstracttask.h"
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <QRunnable>
#include <QDomElement>
#include <QObject>
#include <QList>

class ProjectClip;

class CacheTask : public AbstractTask
{
public:
    CacheTask(const ObjectId &owner, int thumbsCount, int in, int out, QObject* object);
    virtual ~CacheTask();
    static void start(const ObjectId &owner, int thumbsCount = 30, int in = 0, int out = 0, QObject* object = nullptr, bool force = false);

protected:
    void run() override;

private:
    int m_fullWidth;
    int m_thumbsCount;
    int m_in;
    int m_out;
    bool m_thumbOnly;
    std::function<void()> m_readyCallBack;
    QString m_errorMessage;
    void generateThumbnail(std::shared_ptr<ProjectClip>binClip);
};

#endif // CLIPLOADTASK_H
