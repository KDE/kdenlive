/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#ifndef BINPLAYLIST_H
#define BINPLAYLIST_H

#include "definitions.h"
#include <QObject>
#include <memory>
#include <unordered_set>

/** @brief This class is a wrapper around a melt playlist that allows one to store the Bin.
    Clips that are in the bin must be added into this playlist so that they are savedn in the project's xml even if not inserted in the actual timeline.
    The folder structure is also saved as properties.
 */

class AbstractProjectItem;
namespace Mlt {
class Playlist;
class Producer;
class Tractor;
} // namespace Mlt

class MarkerListModel;

class BinPlaylist : public QObject
{

public:
    BinPlaylist();

    /* @brief This function updates the underlying binPlaylist object to reflect deletion of a bin item
       @param binElem is the bin item deleted. Note that exceptionnally, this function takes a raw pointer instead of a smart one.
       This is because the function will be called in the middle of the element's destructor, so no smart pointer is available at that time.
    */
    void manageBinItemDeletion(AbstractProjectItem *binElem);

    /* @brief This function updates the underlying binPlaylist object to reflect insertion of a bin item
       @param binElem is the bin item inserted
    */
    void manageBinItemInsertion(const std::shared_ptr<AbstractProjectItem> &binElem);

    /* @brief This function stores a renamed folder in bin playlise
     */
    void manageBinFolderRename(const std::shared_ptr<AbstractProjectItem> &binElem);

    /* @brief Make sure bin playlist is saved in given tractor.
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

    // id of the mlt object
    static QString binPlaylistId;

protected:
    /* @brief This is an helper function that removes a clip from the playlist given its id
     */
    void removeBinClip(const QString &id);

    /* @brief This handles the fact that a clip has changed its producer (for example, loading is done)
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

#endif
