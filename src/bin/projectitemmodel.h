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

#ifndef PROJECTITEMMODEL_H
#define PROJECTITEMMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"
#include "mltcontroller/bincontroller.h"
#include "undohelper.hpp"
#include <QReadWriteLock>
#include <QSize>

class AbstractProjectItem;
class ProjectClip;
class ProjectFolder;
class Bin;

/**
 * @class ProjectItemModel
 * @brief Acts as an adaptor to be able to use BinModel with item views.
 */

class ProjectItemModel : public AbstractTreeModel
{
    Q_OBJECT

protected:
    explicit ProjectItemModel(Bin *bin, QObject *parent);

public:
    static std::shared_ptr<ProjectItemModel> construct(Bin *bin, QObject *parent);
    ~ProjectItemModel();

    /** @brief Returns a clip from the hierarchy, given its id
     */
    std::shared_ptr<ProjectClip> getClipByBinID(const QString &binId);

    /** @brief Gets a folder by its id. If none is found, the root is returned
     */
    std::shared_ptr<ProjectFolder> getFolderByBinId(const QString &binId);

    /** @brief This function change the global enabled state of the bin effects
     */
    void setBinEffectsEnabled(bool enabled);

    /** @brief Returns some info about the folder containing the given index */
    QStringList getEnclosingFolderInfo(const QModelIndex &index) const;

    /** @brief Deletes all element and start a fresh model */
    void clean();

    /** @brief Convenience method to access root folder */
    std::shared_ptr<ProjectFolder> getRootFolder() const;

    /** @brief Convenience method to access the bin associated with this model
        TODO remove that.
     */
    Bin *bin() const;

    /** @brief Create the subclips defined in the parent clip.
        @param id is the id of the parent clip
        @param data is a definition of the subclips (keys are subclips' names, value are "in:out")*/
    void loadSubClips(const QString &id, const QMap<QString, QString> &data);

    /* @brief Convenience method to retrieve a pointer to an element given its index */
    std::shared_ptr<AbstractProjectItem> getBinItemByIndex(const QModelIndex &index) const;

    /** @brief Returns item data depending on role requested */
    QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Called when user edits an item */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    /** @brief Allow selection and drag & drop */
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    /** @brief Returns column names in case we want to use columns in QTreeView */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    /** @brief Mandatory reimplementation from QAbstractItemModel */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    /** @brief Returns the mimetype used for Drag actions */
    QStringList mimeTypes() const override;
    /** @brief Create data that will be used for Drag events */
    QMimeData *mimeData(const QModelIndexList &indices) const override;
    /** @brief Set size for thumbnails */
    void setIconSize(QSize s);
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    Qt::DropActions supportedDropActions() const override;

    /* @brief Request deletion of a bin clip from the project bin
       @param binId : id of the bin clip to deleted
       @param undo,redo: lambdas that are updated to accumulate operation.
     */
    bool requestBinClipDeletion(std::shared_ptr<AbstractProjectItem> clip, Fun &undo, Fun &redo);

    /* @brief Request creation of a bin folder
       @param id Id of the requested bin. If this is empty, it will be used as a return parameter to give the automatic bin id used.
       @param name Name of the folder
       @param parentId Bin id of the parent folder
       @param undo,redo: lambdas that are updated to accumulate operation.
    */
    bool requestAddFolder(QString &id, const QString &name, const QString &parentId, Fun &undo, Fun &redo);
    /* @brief Manage insertion in the tree hierarchy.
       Note that the element has normally already been registered through registerItem,
       this function is called when its parent is defined.
       @param row is the new element
    */
    void notifyRowAppended(const std::shared_ptr<TreeItem> &row) override;
    /* @brief Manage deletion in the tree hierarchy
       @param item is the parent of the row being deleted
       @param row is the index of the row being deleted
    */
    void notifyRowAboutToDelete(std::shared_ptr<TreeItem> item, int row) override;

    /* @brief Register the existence of a new element
     */
    void registerItem(const std::shared_ptr<TreeItem> &item) override;
    void deregisterItem(int id, TreeItem *item) override;


protected:
    /* @brief This function updates the underlying binPlaylist object to reflect deletion of a bin item
       @param binElem is the bin item deleted. Note that exceptionnally, this function takes a raw pointer instead of a smart one.
       This is because the function will be called in the middle of the element's destructor, so no smart pointer is available at that time.
    */
    void manageBinClipDeletion(AbstractProjectItem *binElem);
    /* @brief This function updates the underlying binPlaylist object to reflect insertion of a bin item
       @param binElem is the bin item inserted
    */
    void manageBinClipInsertion(const std::shared_ptr<AbstractProjectItem> &binElem);

    /* @brief Retrieves the next id available for attribution to a folder */
    int getFreeFolderId();

    /* @brief Retrieves the next id available for attribution to a clip */
    int getFreeClipId();
public slots:
    /** @brief An item in the list was modified, notify */
    void onItemUpdated(std::shared_ptr<AbstractProjectItem> item);

private:
    /** @brief Return reference to column specific data */
    int mapToColumn(int column) const;

    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access

    /** @brief The MLT playlist holding our Producers */
    std::unique_ptr<Mlt::Playlist> m_binPlaylist;

    Bin *m_bin;

    int m_clipCounter;
    int m_folderCounter;
signals:
    void updateThumbProgress(long ms);
    void abortAudioThumb(const QString &id, long duration);
    void refreshAudioThumbs(const QString &id);
    void reloadProducer(const QString &id, const QDomElement &xml);
    void refreshPanel(const QString &id);
    void requestAudioThumbs(const QString &id, long duration);
    // TODO
    void markersNeedUpdate(const QString &id, const QList<int> &);
    void itemDropped(const QStringList &, const QModelIndex &);
    void itemDropped(const QList<QUrl> &, const QModelIndex &);
    void effectDropped(const QString &, const QModelIndex &);
    void addClipCut(const QString &, int, int);
};

#endif
