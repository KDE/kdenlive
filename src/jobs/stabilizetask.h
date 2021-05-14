/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef STABILIZETASK_H
#define STABILIZETASK_H

#include "abstracttask.h"
#include <memory>
#include <unordered_map>
#include <mlt++/MltConsumer.h>

class QProcess;

class StabilizeTask : public AbstractTask
{
public:
    StabilizeTask(const ObjectId &owner, const QString &binId, const QString &destination, int in, int out, std::unordered_map<QString, QVariant> filterParams, QObject* object);
    static void start(QObject* object, bool force = false);
    int length;

private slots:
    void processLogInfo();

protected:
    void run() override;

private:
    QString m_binId;
    int m_inPoint;
    int m_outPoint;
    std::unordered_map<QString, QVariant> m_filterParams;
    const QString m_destination;
    QStringList m_consumerArgs;
    QString m_errorMessage;
    QString m_logDetails;
    std::unique_ptr<QProcess> m_jobProcess;
};


#endif
