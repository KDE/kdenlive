/*
 * SPDX-FileCopyrightText: 2013-2018 Meltytech LLC
 * SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef AUDIOLEVELSTASK_H
#define AUDIOLEVELSTASK_H

#include "abstracttask.h"

#include <QRunnable>
#include <QObject>

class AudioLevelsTask : public AbstractTask
{
public:
    AudioLevelsTask(const ObjectId &owner, QObject* object);
    static void start(const ObjectId &owner, QObject* object, bool force = false);

protected:
    void run() override;

};

#endif // AUDIOLEVELSTASK_H
