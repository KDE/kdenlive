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

#ifndef ABSTRACTPROJECTITEM_H
#define ABSTRACTPROJECTITEM_H

#include "project/jobs/abstractclipjob.h"

#include <QObject>
#include <QPixmap>
#include <QDateTime>

class ProjectClip;
class ProjectFolder;
class Bin;
class QDomElement;
class QDomDocument;

/**
 * @class AbstractProjectItem
 * @brief Base class for all project items (clips, folders, ...).
 *
 * Project items are stored in a tree like structure ...
 */

class AbstractProjectItem : public QObject, public QList<AbstractProjectItem *>
{
    Q_OBJECT

public:

    enum PROJECTITEMTYPE {
        FolderUpItem = 0,
        FolderItem = 1,
        ClipItem = 2,
        SubClipItem = 3
    };

    /**
     * @brief Constructor.
     * @param parent parent this item should be added to
     */
    AbstractProjectItem(PROJECTITEMTYPE type, const QString &id, AbstractProjectItem *parent = nullptr);
    /**
     * @brief Creates a project item upon project load.
     * @param description element for this item.
     * @param parent parent this item should be added to
     *
     * We try to read the attributes "name" and "description"
     */
    AbstractProjectItem(PROJECTITEMTYPE type, const QDomElement &description, AbstractProjectItem *parent = nullptr);
    virtual ~AbstractProjectItem();

    bool operator==(const AbstractProjectItem *projectItem) const;

    /** @brief Returns a pointer to the parent item (or NULL). */
    AbstractProjectItem *parent() const;
    /** @brief Removes the item from its current parent and adds it as a child to @param parent. */
    virtual void setParent(AbstractProjectItem *parent);

    /**
     * @brief Adds a new child item and notifies the bin model about it (before and after).
     * @param child project item which should be added as a child
     *
     * This function is called by setParent.
     */
    virtual void addChild(AbstractProjectItem *child);

    /**
     * @brief Removes a child item and notifies the bin model about it (before and after).
     * @param child project which sould be removed from the child list
     *
     * This function is called when a child's parent is changed through setParent
     */
    virtual void removeChild(AbstractProjectItem *child);

    /** @brief Returns a pointer to the bin model this item is child of (through its parent item). */
    virtual Bin *bin();

    /** @brief Returns the index this item has in its parent's child list. */
    int index() const;

    /** @brief Returns the type of this item (folder, clip, subclip, etc). */
    PROJECTITEMTYPE itemType() const;

    /** @brief Used to search for a clip with a specific id. */
    virtual ProjectClip *clip(const QString &id) = 0;
    /** @brief Used to search for a folder with a specific id. */
    virtual ProjectFolder *folder(const QString &id) = 0;
    virtual ProjectClip *clipAt(int ix) = 0;
    /** @brief Recursively disable/enable bin effects. */
    virtual void disableEffects(bool disable) = 0;

    /** @brief Returns the clip's id. */
    const QString &clipId() const;
    virtual QPoint zone() const;
    /** @brief Set current usage count. */
    void setRefCount(uint count);
    /** @brief Returns clip's current usage count in timeline. */
    uint refCount() const;
    /** @brief Increase usage count. */
    void addRef();
    /** @brief Decrease usage count. */
    void removeRef();

    enum DataType {
        // display name of item
        DataName = Qt::DisplayRole,
        // image thumbnail
        DataThumbnail = Qt::DecorationRole,
        // Tooltip text,usually full path
        ClipToolTip = Qt::ToolTipRole,
        // unique id of the project clip / folder
        DataId = Qt::UserRole,
        // creation date
        DataDate,
        // Description for item (user editable)
        DataDescription,
        // Number of occurrences used in timeline
        UsageCount,
        // Empty if clip has no effect, icon otherwise
        IconOverlay,
        // item type (clip, subclip, folder)
        ItemTypeRole,
        // Duration of the clip
        DataDuration,
        // If there is a running job, which type
        JobType,
        // Current progress of the job
        JobProgress,
        // error message if job crashes (not fully implemented)
        JobMessage,
        // Item status (ready or not, missing, waiting, ...)
        ClipStatus
    };

    enum CLIPSTATUS {
        StatusReady = 0,
        StatusMissing,
        StatusWaiting,
        StatusDeleting
    };

    void setClipStatus(AbstractProjectItem::CLIPSTATUS status);
    AbstractProjectItem::CLIPSTATUS clipStatus() const;
    bool statusReady() const;

    /** @brief Returns the data that describes this item.
     * @param type type of data to return
     *
     * This function is necessary for interaction with ProjectItemModel.
     */
    virtual QVariant data(DataType type) const;

    /**
     * @brief Returns the amount of different types of data this item supports.
     *
     * This base class supports only DataName and DataDescription, so the return value is always 2.
     * This function is necessary for interaction with ProjectItemModel.
     */
    virtual int supportedDataCount() const;

    /** @brief Returns the (displayable) name of this item. */
    QString name() const;
    /** @brief Sets a new (displayable) name. */
    virtual void setName(const QString &name);

    /** @brief Returns the (displayable) description of this item. */
    QString description() const;
    /** @brief Sets a new description. */
    virtual void setDescription(const QString &description);

    /** @brief Flags this item as being current (or not) and notifies the bin model about it. */
    virtual void setCurrent(bool current, bool notify = true) = 0;

    virtual QDomElement toXml(QDomDocument &document, bool includeMeta = false) = 0;
    virtual QString getToolTip() const = 0;
    virtual bool rename(const QString &name, int column) = 0;

signals:
    void childAdded(AbstractProjectItem *child);
    void aboutToRemoveChild(AbstractProjectItem *child);

protected:
    AbstractProjectItem *m_parent;
    QString m_name;
    QString m_description;
    QIcon m_thumbnail;
    QString m_duration;
    QDateTime m_date;
    QString m_id;
    uint m_usage;
    CLIPSTATUS m_clipStatus;
    AbstractClipJob::JOBTYPE m_jobType;
    int m_jobProgress;

    QString m_jobMessage;
    PROJECTITEMTYPE m_itemType;

    /** @brief Returns a rounded border pixmap from the @param source pixmap. */
    QPixmap roundedPixmap(const QPixmap &source);

private:
    bool m_isCurrent;
};

#endif
