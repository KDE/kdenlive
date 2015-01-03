/***************************************************************************
 *   Copyright (C) 2014 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef BINCONTROLLER_H
#define BINCONTROLLER_H

#include <mlt++/Mlt.h>

#include <QString>
#include <QStringList>

#include "definitions.h"

class ClipController;

namespace Mlt
{
class Playlist;
class Producer;
class Profile;
}

/**
 * @class BinController
 * @brief This is where MLT's project clips (the bin clips) are managed
 * 
 * This is also where the Mlt::Factory is first initialized.
 * The project profile, used to build the monitors renderers is stored here
 */

class BinController
{
public:
    BinController(QString profileName = QString());
    virtual ~BinController();
    
    /** @brief Returns the MLT profile used everywhere in the project. */
    Mlt::Profile *profile();
    
    /** @brief Returns the project's fps. */
    double fps() const;
    
    /** @brief Returns the project's dar. */
    double dar() const;

    /** @brief Reset the profile to a new one, for example when loading a document with another profile.
     * @param newProfile The file name for the new MLT profile
     * */
    void resetProfile(const QString &newProfile);
    
    /** @brief Returns the service for the Bin's playlist, used to make sure MLT will save it correctly in its XML. */
    mlt_service service();
    
    /** @brief Add a new clip producer to the project.
     * @param id The clip's id
     * @param producer The MLT producer for this clip
     * */
    void addClipToBin(const QString &id, ClipController *controller);
    
    /** @brief Returns the name MLT will use to store our bin's playlist */
    static const QString binPlaylistId();
    
    /** @brief Clear the bin's playlist */
    void destroyBin();
    
    /** @brief Initialize the bin's playlist from MLT's data
     * @param playlist The MLT playlist containing our bin's clips
     */
    void initializeBin(Mlt::Playlist playlist);
    
    /** @brief If our bin's playlist does not exist, create a new one */
    void createIfNeeded();

     /** @brief Returns true if a clip with that id is in our bin's playlist
     * @param id The clip's id as stored in DocClipBase
     */
    bool hasClip(const QString &id);
    
    QStringList getClipIds() const;
    
    /** @brief Delete a clip from the bin from its id. 
     * @param id The clip's id as stored in DocClipBase
     * @return true on success, false on error
     */
    bool removeBinClip(const QString &id);
    
    /** @brief Get the MLT Producer for a given id. Since MLT requires 1 different producer for each track to correctly handle audio mix, 
     * we can should specify on which track the clip should be. 
     @param id The clip id as stored in the DocClipBase class
     @param track The track on which the producer will be put. Setting a value of -1 will return the master clip contained in the bin playlist
     @param clipState The state of the clip (if we need an audio only or video only producer).
     @param speed If the clip has a speed effect (framebuffer producer), we indicate the speed here
    */
    Mlt::Producer *getBinClip(const QString &id, int track, PlaylistState::ClipState clipState = PlaylistState::Original, double speed = 1.0);
    
    /** @brief Returns the clip data as rendered by MLT's XML consumer, used to duplicate a clip
     * @param producer The clip's original producer
     */
    QString getProducerXML(Mlt::Producer &producer);
    
    /** @brief Returns the clip data as rendered by MLT's XML consumer
     * @param id The clip's original id
     * @returns An XML element containing the clip xml
     */
    QString xmlFromId(const QString & id);
    int clipCount() const;
    Mlt::Producer *cloneProducer(Mlt::Producer &original);
    
    ClipController *getController(const QString &id);
    const QList <ClipController *> getControllerList() const;

    void replaceBinPlaylistClip(const QString &id, Mlt::Producer &producer);
    
    /** @brief Get the list of ids whose clip have the resource indicated by @param url */
    const QStringList getBinIdsByResource(const QUrl &url) const;
    void replaceProducer(const QString &id, Mlt::Producer &producer);

private:
    /** @brief The MLT playlist holding our Producers */
    Mlt::Playlist * m_binPlaylist;
    
    /** @brief The current MLT profile's filename */
    QString m_activeProfile;
    
    /** @brief The MLT profile */
    Mlt::Profile *m_mltProfile;
    
    /** @brief Can be used to copy filters from a clip to another */
    void duplicateFilters(Mlt::Producer original, Mlt::Producer clone);
    
    /** @brief This list holds all producer controllers for the playlist, indexed by id */
    QMap <QString, ClipController *> m_clipList;
    
    /** @brief Remove a clip from MLT's special bin playlist */
    void removeBinPlaylistClip(const QString &id);
};

#endif
