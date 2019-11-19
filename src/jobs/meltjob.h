/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *                                                                         *
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

#ifndef MELTJOB
#define MELTJOB

#include "abstractclipjob.h"

namespace Mlt {
class Profile;
class Producer;
class Consumer;
class Filter;
class Event;
} // namespace Mlt

/**
 * @class MeltJob
 * @brief This is an abstract class for jobs that rely on a melt filter to process the clips
 *
 */

class MeltJob : public AbstractClipJob
{
    Q_OBJECT

public:
    /** @brief Creates a melt job for the given bin clip
        consumerName is the melt name of the consumer that we use
        if useProducerProfile == true, the profile used will be the one of the producer
        in and out represent the portion of the clip we deal with. Leave to -1 for default (whole clip)
     */
    MeltJob(const QString &binId, JOBTYPE type, bool useProducerProfile = false, int in = -1, int out = -1);
    bool startJob() override;

    int length;

protected:
    // @brief extra configuration of the profile (eg: resize the profile)
    virtual void configureProfile() {}

    // @brief extra configuration of the producer
    virtual void configureProducer() {}

    // @brief create and configure consumer
    virtual void configureConsumer() = 0;

    // @brief create and configure filter
    virtual void configureFilter() = 0;

protected:
    std::unique_ptr<Mlt::Consumer> m_consumer;
    std::unique_ptr<Mlt::Producer> m_producer;
    std::unique_ptr<Mlt::Producer> m_wholeProducer; // in the case of a job on a part on the clip, this is set to the whole producer
    std::shared_ptr<Mlt::Profile> m_profile;
    std::unique_ptr<Mlt::Filter> m_filter;
    std::unique_ptr<Mlt::Event> m_showFrameEvent;

    bool m_done{false}, m_successful{false};

    QString m_url;
    QString m_filterName;
    bool m_useProducerProfile;
    int m_in, m_out;
    // @brief Does this job require a filter
    bool m_requiresFilter;
};

#endif
