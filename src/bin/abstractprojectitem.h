/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractmodel/treeitem.hpp"
#include "undohelper.hpp"

#include <QDateTime>
#include <QIcon>
#include <QObject>
#include <QReadWriteLock>

class ProjectClip;
class ProjectFolder;
class Bin;
class QDomElement;
class QDomDocument;
class ProjectItemModel;

/**
 * @class AbstractProjectItem
 * @brief Base class for all project items (clips, folders, ...).
 *
 * Project items are stored in a tree like structure ...
 */
class AbstractProjectItem : public QObject, public TreeItem
{
    Q_OBJECT

public:
    enum PROJECTITEMTYPE { FolderItem, ClipItem, SubClipItem };

    /**
     * @brief Constructor.
     * @param type is the type of the bin item
     * @param id is the binId
     * @param model is the ptr to the item model
     * @param isRoot is true if this is the topmost folder
     */
    AbstractProjectItem(PROJECTITEMTYPE type, QString id, const std::shared_ptr<ProjectItemModel> &model, bool isRoot = false);

    bool operator==(const std::shared_ptr<AbstractProjectItem> &projectItem) const;

    /** @brief Returns a pointer to the parent item (or NULL). */
    std::shared_ptr<AbstractProjectItem> parent() const;

    /** @brief Returns the type of this item (folder, clip, subclip, etc). */
    PROJECTITEMTYPE itemType() const;

    /** @brief Used to search for a clip with a specific id. */
    virtual std::shared_ptr<ProjectClip> clip(const QString &id) = 0;
    /** @brief Used to search for a folder with a specific id. */
    virtual std::shared_ptr<ProjectFolder> folder(const QString &id) = 0;
    virtual std::shared_ptr<ProjectClip> clipAt(int ix) = 0;
    /** @brief Recursively disable/enable bin effects. */
    virtual void setBinEffectsEnabled(bool enabled) = 0;
    /** @brief Returns true if item has both audio and video enabled. */
    virtual bool hasAudioAndVideo() const = 0;

    /** @brief This function executes what should be done when the item is deleted
        but without deleting effectively.
        For example, the item will deregister itself from the model and delete the
        clips from the timeline.
        However, the object is NOT actually deleted, and the tree structure is preserved.
        @param Undo,Redo are the lambdas accumulating the update.
     */
    virtual bool selfSoftDelete(Fun &undo, Fun &redo);
    virtual Fun getAudio_lambda();
    /** @brief Returns the clip's id. */
    const QString &clipId() const;
    virtual QPoint zone() const;

    // TODO refac : these ref counting are probably deprecated by smart ptrs
    /** @brief Set current usage count.
     *  @param sequenceCount the usage count in the active sequence
     *  @param totalCount the usage count in the whole project */
    void setRefCount(uint sequenceCount, uint totalCount);
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
        AudioUsed,
        VideoUsed,
        // Empty if clip has no effect, icon otherwise
        IconOverlay,
        // item type (clip, subclip, folder)
        ItemTypeRole,
        // Duration of the clip as displayabe string
        DataDuration,
        // Tag of the clip as colors
        DataTag,
        // Rating of the clip (0-5)
        DataRating,
        // Duration of the clip in frames
        ParentDuration,
        // Inpoint of the subclip (0 for clips)
        DataInPoint,
        // Outpoint of the subclip (0 for clips)
        DataOutPoint,
        // Current progress of the job
        JobProgress,
        // error message if job crashes (not fully implemented)
        JobSuccess,
        JobStatus,
        // Item status (ready or not, missing, waiting, ...)
        ClipStatus,
        ClipType,
        ClipHasAudioAndVideo,
        // Item is the folder containing sequences
        SequenceFolder
    };

    virtual void setClipStatus(FileStatus::ClipStatus status);
    FileStatus::ClipStatus clipStatus() const;
    bool statusReady() const;

    /** @brief Returns the data that describes this item.
     * @param type type of data to return
     *
     * This function is necessary for interaction with ProjectItemModel.
     */
    virtual const QVariant getData(DataType type) const;
    /**
     * @brief Returns the item icon.
     */
    const QIcon icon() const;

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

    virtual QDomElement toXml(QDomDocument &document, bool includeMeta = false, bool includeProfile = true) = 0;
    virtual QString getToolTip() const = 0;
    virtual bool rename(const QString &name, int column) = 0;

    /** @brief Return the bin id of the last parent that this element got, even if this
       parent has already been destroyed.
       Return the empty string if the element was parentless */
    QString lastParentId() const;

    /** @brief This is an overload of TreeItem::updateParent that tracks the id of the id of the parent */
    void updateParent(std::shared_ptr<TreeItem> newParent) override;

    /** @brief Returns a ptr to the enclosing dir, and nullptr if none is found.
       @param strict if set to false, the enclosing dir of a dir is itself, otherwise we try to find a "true" parent
    */
    std::shared_ptr<AbstractProjectItem> getEnclosingFolder(bool strict = false);

    /** @brief Returns true if a clip corresponding to this bin is inserted in a timeline.
        Note that this function does not account for children, use TreeItem::accumulate if you want to get that information as well.
    */
    virtual bool isIncludedInTimeline() { return false; }
    virtual ClipType::ProducerType clipType() const = 0;
    uint rating() const;
    virtual void setRating(uint rating);
    const QString &tags() const;
    void setTags(const QString &tags);

Q_SIGNALS:
    void childAdded(AbstractProjectItem *child);
    void aboutToRemoveChild(AbstractProjectItem *child);

protected:
    QString m_name;
    QString m_description;
    QIcon m_thumbnail;
    QString m_duration;
    int m_parentDuration;
    int m_inPoint;
    int m_outPoint;
    QDateTime m_date;
    QString m_binId;
    uint m_totalUsage;
    uint m_currentSequenceUsage;
    uint m_AudioUsage;
    uint m_VideoUsage;
    QString m_usageText;
    uint m_rating;
    QString m_tags;
    FileStatus::ClipStatus m_clipStatus;

    PROJECTITEMTYPE m_itemType;

    QString m_lastParentId;

    /** @brief Returns a rounded border pixmap from the @param source pixmap. */
    QPixmap roundedPixmap(const QPixmap &source);
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;

private:
    bool m_isCurrent;
};
