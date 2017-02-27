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
#include <QDir>
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
 * The project profile, used to build the monitors renderers is stored here
 */

class BinController : public QObject
{
    Q_OBJECT

public:
    explicit BinController(const QString &profileName = QString());
    virtual ~BinController();

    /** @brief Returns the MLT profile used everywhere in the project. */
    Mlt::Profile *profile();

    /** @brief Returns the project's fps. */
    double fps() const;

    /** @brief Returns the project's dar. */
    double dar() const;

    /** @brief Returns the service for the Bin's playlist, used to make sure MLT will save it correctly in its XML. */
    mlt_service service();

    /** @brief Add a new clip producer to the project.
     * @param id The clip's id
     * @param producer The MLT producer for this clip
     * */
    void addClipToBin(const QString &id, ClipController *controller);

    /** @brief Store a timeline producer in clip list for later re-use
     * @param id The clip's id
     * @param producer The MLT producer for this clip
     * */
    void loadExtraProducer(const QString &id, Mlt::Producer *prod);

    /** @brief Returns the name MLT will use to store our bin's playlist */
    static const QString binPlaylistId();

    /** @brief Clear the bin's playlist */
    void destroyBin();

    /** @brief Initialize the bin's playlist from MLT's data
     * @param playlist The MLT playlist containing our bin's clips
     */
    void initializeBin(Mlt::Playlist playlist);

    /** @brief If our bin's playlist does not exist, create a new one */
    void createIfNeeded(Mlt::Profile *profile);

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

    /** @brief Get the MLT Producer for a given id.
     @param id The clip id as stored in the DocClipBase class */
    // TODO? Since MLT requires 1 different producer for each track to correctly handle audio mix,
    // we should specify on which track the clip should be.
    // @param track The track on which the producer will be put. Setting a value of -1 will return the master clip contained in the bin playlist
    // @param clipState The state of the clip (if we need an audio only or video only producer).
    // @param speed If the clip has a speed effect (framebuffer producer), we indicate the speed here
    Mlt::Producer *getBinProducer(const QString &id);

    /** @brief returns a video only (no audio) version of this producer  */
    Mlt::Producer *getBinVideoProducer(const QString &id);

    /** @brief Returns the clip data as rendered by MLT's XML consumer, used to duplicate a clip
     * @param producer The clip's original producer
     */
    QString getProducerXML(Mlt::Producer &producer, bool includeMeta = false);

    /** @brief Returns the clip data as rendered by MLT's XML consumer
     * @param id The clip's original id
     * @returns An XML element containing the clip xml
     */
    QString xmlFromId(const QString &id);
    int clipCount() const;
    Mlt::Producer *cloneProducer(Mlt::Producer &original);

    ClipController *getController(const QString &id);
    const QList<ClipController *> getControllerList() const;
    void replaceBinPlaylistClip(const QString &id, Mlt::Producer &producer);

    /** @brief Get the list of ids whose clip have the resource indicated by @param url */
    const QStringList getBinIdsByResource(const QFileInfo &url) const;
    void replaceProducer(const QString &id, Mlt::Producer &producer);
    void storeMarker(const QString &markerId, const QString &markerHash);
    QMap<double, QString> takeGuidesData();

    /** @brief A Bin clip effect was changed, update track producers */
    void updateTrackProducer(const QString &id);

    /** @brief Load thumbnails for all producers */
    void checkThumbnails(const QDir &thumbFolder);

    /** @brief Request audio thumbnails for all producers */
    void checkAudioThumbs();

    /** @brief Save document properties in MLT's bin playlist */
    void saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata, const QMap<double, QString> &guidesData);

    /** @brief Save a property to main bin */
    void saveProperty(const QString &name, const QString &value);

    /** @brief Save a property from the main bin */
    const QString getProperty(const QString &name);

    /** @brief Return a list of proxy / original url */
    QMap<QString, QString> getProxies();

    /** @brief Loads the MLT's XML document root */
    void setDocumentRoot(const QString &);

    /** @brief Returns the MLT's XML document root */
    const QString documentRoot() const;

    /** @brief Returns a list of all clips hashes. */
    QStringList getProjectHashes();

public slots:
    /** @brief Stored a Bin Folder id / name to MLT's bin playlist. Using an empry folderName deletes the property */
    void slotStoreFolder(const QString &folderId, const QString &parentId, const QString &oldParentId, const QString &folderName);

private:
    /** @brief The MLT playlist holding our Producers */
    Mlt::Playlist *m_binPlaylist;

    /** @brief The current MLT profile's filename */
    QString m_activeProfile;

    /** @brief Can be used to copy filters from a clip to another */
    void duplicateFilters(Mlt::Producer original, Mlt::Producer clone);

    /** @brief This list holds all producer controllers for the playlist, indexed by id */
    QMap<QString, ClipController *> m_clipList;

    /** @brief This list holds all extra controllers (slowmotion, video only, ... that are in timeline, indexed by id */
    QMap<QString, Mlt::Producer *> m_extraClipList;

    /** @brief Stores MLT's xml playlist document root, useful to recover full urls */
    QString m_documentRoot;

    /** @brief Remove a clip from MLT's special bin playlist */
    void removeBinPlaylistClip(const QString &id);

    /** @brief Duplicate effects from stored producer */
    void pasteEffects(const QString &id, Mlt::Producer &producer);

signals:
    void loadFolders(const QMap<QString, QString> &);
    void loadThumb(const QString &, const QImage&, bool);
    void createThumb(const QDomElement &, const QString &, int);
    void requestAudioThumb(const QString &);
    void abortAudioThumbs();
    void replaceTimelineProducer(const QString &id);
    void setDocumentNotes(const QString &);
    void updateTimelineProducer(const QString &);
    /** @brief We want to replace a clip with another, but before we need to change clip producer id so that there is no interference*/
    void prepareTimelineReplacement(const QString &);
    /** @brief Indicate which clip we are loading */
    void loadingBin(int);
};

#endif
