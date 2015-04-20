/*
 * Kdenlive timeline track handling MLT playlist
 * Copyright 2015 Kdenlive team <kdenlive@kde.org>
 * Author: Vincent Pinon <vpinon@kde.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef TRACK_H
#define TRACK_H

#include <QObject>

#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProducer.h>

/** @brief Kdenlive timeline track, to access MLT playlist operations
 * The track as seen in the video editor is actually a playlist
 * in the MLT framework behind the scene.
 * When one adds, deletes, moves and resizes clips on the track,
 * it corresponds to playlist operations, simple or elaborate.
 * The Track class handles the edit time to MLT frame & index
 * correspondance, and builds complex operations on top of basic
 * commands.
 */
class Track : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mlt::Playlist playlist READ playlist WRITE setPlaylist)
    Q_PROPERTY(qreal fps READ fps WRITE setFps)

public:
    /** @brief Track constructor
     * @param playlist is the MLT object used for monitor/render
     * @param fps is the read speed (frames per seconds) */
    explicit Track(Mlt::Playlist &playlist, qreal fps);
    ~Track();

    /// Property access function
    Mlt::Playlist & playlist();
    qreal fps();

    /** @brief convertion utility function
     * @param time (in seconds)
     * @return frame number */
    int frame(qreal t);
    /** @brief get the playlist duration
     * @return play time in seconds */
    qreal length();

    /** @brief add a clip
     * @param cut is the MLT Producer (resource + in/out timecodes etc)
     * @param t is the time position to start the cut (in seconds)
     * @return true if success */
    bool add(Mlt::Producer *cut, qreal t);
    /** @brief delete a clip
     * @param time where clip is present (in seconds);
     * @return true if success */
    bool del(qreal t);
    /** delete a region
     * @param t is the start,
     * @param dt is the duration (in seconds)
     * @return true if success */
    bool del(qreal t, qreal dt);
    /** @brief change the clip length from start or end
     * @param told is the current edge position,
     * @param tnew is the target edge position (in seconds)
     * @param end precises if we move the end of the left clip (\em true)
     *  or the start of the right clip (\em false)
     * @return true if success */
    bool resize(qreal told, qreal tnew, bool end);
    /** @brief split the clip at given position
     * @param t is the cut time in playlist
     * @return true if success */
    bool cut(qreal t);

public Q_SLOTS:
    void setPlaylist(Mlt::Playlist &playlist);
    void setFps(qreal fps);

private:
    /** MLT playlist behind the scene */
    Mlt::Playlist m_playlist;
    /** frames per second (read speed) */
    qreal m_fps;
};

#endif // TRACK_H
