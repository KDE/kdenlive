/*
 * Copyright (c) 2013-2018 Meltytech, LLC
 * Copyright (c) 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
