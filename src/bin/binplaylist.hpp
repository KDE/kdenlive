/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QObject>
#include <memory>
#include <unordered_set>

class AbstractProjectItem;
namespace Mlt {
class Playlist;
class Producer;
class Tractor;
} // namespace Mlt

class MarkerListModel;

/** @class BinPlaylist
    @brief This class is a wrapper around a melt playlist that allows one to store the Bin.
    Clips that are in the bin must be added into this playlist so that they are savedn in the project's xml even if not inserted in the actual timeline.
    The folder structure is also saved as properties.
 */
class BinPlaylist : public QObject
{

public:
    BinPlaylist();

    /** @brief This function updates the underlying binPlaylist object to reflect deletion of a bin item
       @param binElem is the bin item deleted. Note that exceptionnally, this function takes a raw pointer instead of a smart one.
       This is because the function will be called in the middle of the element's destructor, so no smart pointer is available at that time.
    */
    void manageBinItemDeletion(AbstractProjectItem *binElem);

    /** @brief This function updates the underlying binPlaylist object to reflect insertion of a bin item
       @param binElem is the bin item inserted
    */
    void manageBinItemInsertion(const std::shared_ptr<AbstractProjectItem> &binElem);

    /** @brief This function stores a renamed folder in bin playlise
     */
    void manageBinFolderRename(const std::shared_ptr<AbstractProjectItem> &binElem);

    /** @brief Make sure bin playlist is saved in given tractor.
       This has a side effect on the tractor
    */
    void setRetainIn(Mlt::Tractor *modelTractor);

    /** @brief Save document properties in MLT's bin playlist */
    void saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata, std::shared_ptr<MarkerListModel> guideModel);

    /** @brief Save a property to main bin */
    void saveProperty(const QString &name, const QString &value);

    /** @brief Retrieve a list of proxy/original urls */
    QMap<QString, QString> getProxies(const QString &root);

    /** @brief The number of clips in the Bin Playlist */
    int count() const;

    /** @brief id of the mlt object */
    static QString binPlaylistId;

protected:
    /** @brief This is an helper function that removes a clip from the playlist given its id
     */
    void removeBinClip(const QString &id);

    /** @brief This handles the fact that a clip has changed its producer (for example, loading is done)
       It should be called directly as a slot of ClipController's signal, so you probably don't want to call this directly.
       @param id: binId of the producer
       @param producer : new producer
    */
    void changeProducer(const QString &id, const std::shared_ptr<Mlt::Producer> &producer);

private:
    /** @brief The MLT playlist holding our Producers */
    std::unique_ptr<Mlt::Playlist> m_binPlaylist;

    /** @brief Set of the bin inserted */
    std::unordered_set<QString> m_allClips;
};
