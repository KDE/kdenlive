/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "mltcontroller/clipcontroller.h"
#include "projectclip.h"
#include "timeline2/model/timelinemodel.hpp"

#include <QFuture>
#include <QMutex>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>
#include <memory>

class ClipPropertiesController;
class ProjectClip;
class QDomElement;

namespace Mlt {
class Producer;
class Properties;
} // namespace Mlt

/**
 * @class SequenceClip
 * @brief Represents a Timeline Sequence clip in the project.
 * It will be displayed as a bin item that can be dragged onto the timeline.
 * A single bin clip can be inserted several times on the timeline, and the ProjectClip
 * keeps track of all the ids of the corresponding ClipModel.
 * Note that because of a limitation in melt and AvFilter, it is currently difficult to
 * mix the audio of two producers that are cut from the same master producer
 * (that produces small but noticeable clicking artifacts)
 * To workaround this, we need to have a master clip for each instance of the audio clip in the timeline. This class is tracking them all. This track also holds
 * a master clip for each clip where the timewarp producer has been applied
 */
class SequenceClip : public ProjectClip
{
    Q_OBJECT

public:
    friend class Bin;
    friend bool TimelineModel::checkConsistency(const std::vector<int> &guideSnaps); // for testing
    /**
     * @brief Constructor; used when loading a project and the producer is already available.
     */
    static std::shared_ptr<SequenceClip> construct(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model,
                                                   std::shared_ptr<Mlt::Producer> &producer);
    /**
     * @brief Constructor.
     * @param description element describing the clip; the "kdenlive:id" attribute and "resource" property are used
     */
    static std::shared_ptr<SequenceClip> construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                   std::shared_ptr<ProjectItemModel> model);

protected:
    SequenceClip(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> &producer);
    SequenceClip(const QString &id, const QDomElement &description, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model);
    void createDisabledMasterProducer() override;
    const QString getSequenceResource() override;
    const QString getFileHash() override;
    /** @brief Remove temporary warp producer resource files */
    void removeSequenceWarpResources() override;

public:
    ~SequenceClip() override;
    const QString hashForThumbs() override;
    int getThumbFrame() const override;
    /** @brief Return the last frame used for this clip in other sequences of the project
     */
    int lastBound() override;
    void setThumbFrame(int frame) override;
    int getThumbFromPercent(int percent, bool storeFrame = false) override;
    /** @brief Returns false if the clip is or embeds a timeline with uuid. */
    bool canBeDropped(const QUuid &uuid) const override;
    /** @brief Get the sequence's unique identifier, empty if not a sequence clip. */
    const QUuid getSequenceUuid() const override;
    void setProperties(const QMap<QString, QString> &properties, bool refreshPanel = false) override;
    std::unique_ptr<Mlt::Producer> getThumbProducer(const QUuid &) override;
    QDomElement toXml(QDomDocument &document, bool includeMeta = false, bool includeProfile = true) override;

public Q_SLOTS:
    bool setProducer(std::shared_ptr<Mlt::Producer> producer, bool generateThumb = false, bool clearTrackProducers = true) override;

private:
    // The sequence unique identifier
    QUuid m_sequenceUuid;
    QTemporaryFile m_sequenceThumbFile;
};
