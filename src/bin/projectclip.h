/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#ifndef PROJECTCLIP_H
#define PROJECTCLIP_H

#include "abstractprojectitem.h"
#include "definitions.h"


#include <QUrl>
#include <QMutex>

class ProjectFolder;
class QDomElement;
class ClipController;
class ClipPropertiesController;

namespace Mlt {
  class Producer;
  class Properties;
};


/**
 * @class ProjectClip
 * @brief Represents a clip in the project (not timeline).
 * 

 */

class ProjectClip : public AbstractProjectItem
{
    Q_OBJECT

public:
    /**
     * @brief Constructor; used when loading a project and the producer is already available.
     */
    ProjectClip(const QString &id, ClipController *controller, ProjectFolder* parent);
    /**
     * @brief Constructor.
     * @param description element describing the clip; the "id" attribute and "resource" property are used
     */
    ProjectClip(const QDomElement &description, ProjectFolder *parent);
    virtual ~ProjectClip();

    void reloadProducer(bool thumbnailOnly = false);

    /** @brief Returns a unique hash identifier used to store clip thumbnails. */
    //virtual void hash() = 0;

    /** @brief Returns this if @param id matches the clip's id or NULL otherwise. */
    ProjectClip *clip(const QString &id);
    
    ProjectFolder* folder(const QString &id);

    /** @brief Returns this if @param ix matches the clip's index or NULL otherwise. */
    ProjectClip* clipAt(int ix);

    /** @brief Returns the clip type as defined in definitions.h */
    ClipType clipType() const;
    ClipPropertiesController *buildProperties(QWidget *parent);

    //TODO
    void setZone(const QPoint &zone);
    //TODO
    QPoint zone() const;
    //TODO
    void addMarker(int position);
    //TODO
    void removeMarker(int position);

    /** @brief Returns whether this clip has a url (=describes a file) or not. */
    bool hasUrl() const;

    /** @brief Returns the clip's url. */
    QUrl url() const;

    /** @brief Returns the clip's xml data by using MLT's XML consumer. */
    QString serializeClip();

    /** @brief Returns whether this clip has a limited duration or whether it is resizable ad infinitum. */
    virtual bool hasLimitedDuration() const;

    /** @brief Returns the clip's duration. */
    GenTime duration() const;

    /** @brief Calls AbstractProjectItem::setCurrent and sets the bin monitor to use the clip's producer. */
    virtual void setCurrent(bool current, bool notify = true);

    virtual QDomElement toXml(QDomDocument &document);
    
    /** @brief Set the Job status on a clip.
     * @param jobType The job type
     * @param status The job status (see definitions.h)
     * @param progress The job progress (in percents)
     * @param statusMessage The job info message */
    void setJobStatus(AbstractClipJob::JOBTYPE jobType, ClipJobStatus status, int progress = 0, const QString &statusMessage = QString());

    /** @brief Sets thumbnail for this clip. */
    void setThumbnail(QImage);

    /** @brief Sets the MLT producer associated with this clip
     *  @param producer The producer
     *  @param replaceProducer If true, we replace existing producer with this one
     * . */
    void setProducer(ClipController *controller, bool replaceProducer);
    
    /** @brief Returns true if this clip already has a producer. */
    bool isReady() const;
    
    /** @brief Returns this clip's producer. */
    Mlt::Producer *producer();

    //TODO
    QMap <QString, QString> properties();
    
    /** @brief Set properties on this clip. TODO: should we store all in MLT or use extra m_properties ?. */
    void setProperties(QMap <QString, QString> properties, bool refreshPanel = false);
    
    /** @brief Get an XML property from MLT produced xml. */
    static QString getXmlProperty(const QDomElement &producer, const QString &propertyName);
    
    /** @brief The clip hash created from the clip's resource. */
    const QString hash();
    
    /** @brief Set a property on the MLT producer. */
    void setProducerProperty(const char *name, int data);
    /** @brief Set a property on the MLT producer. */
    void setProducerProperty(const char *name, double data);
    /** @brief Set a property on the MLT producer. */
    void setProducerProperty(const char *name, const char *data);
    
    /** @brief Get a property from the MLT producer. */
    QString getProducerProperty(const QString &key) const;
    int getProducerIntProperty(const QString &key) const;
    
    QList < CommentedTime > commentedSnapMarkers() const;

    /** @brief Returns true if we are using a proxy for this clip. */
    bool hasProxy() const;
    
    /** Cache for every audio Frame with 10 Bytes */
    /** format is frame -> channel ->bytes */
    QMap<int, QMap<int, QByteArray> > audioFrameCache;
    bool audioThumbCreated() const;
    
    void setWaitingStatus(const QString &id);

public slots:
    //TODO
    QPixmap thumbnail(bool force = false);
    void updateAudioThumbnail(const audioByteArray& data);

protected:
    bool m_hasLimitedDuration;

private:
    //TODO: clip markers
    QList <int> m_markers;
    /** @brief The Clip controller for this clip. */
    ClipController *m_controller;
    /** @brief Generate and store file hash if not available. */
    const QString getFileHash() const;
    bool m_audioThumbCreated;
    
signals:
    void gotAudioData();
    void refreshPropertiesPanel();
};

#endif
