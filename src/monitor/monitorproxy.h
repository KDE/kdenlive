/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

/** @brief  This class is a wrapper around the monitor / glwidget and handles communication
 *          with the qml overlay through its properties.
 */

#ifndef MONITORPROXY_H
#define MONITORPROXY_H

#include <QImage>
#include <QObject>

class GLWidget;

class MonitorProxy : public QObject
{
    Q_OBJECT
    // Q_PROPERTY(int consumerPosition READ consumerPosition NOTIFY consumerPositionChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int seekPosition READ seekPosition WRITE setSeekPosition NOTIFY seekPositionChanged)
    Q_PROPERTY(int zoneIn READ zoneIn WRITE setZoneIn NOTIFY zoneChanged)
    Q_PROPERTY(int zoneOut READ zoneOut WRITE setZoneOut NOTIFY zoneChanged)
    Q_PROPERTY(int rulerHeight READ rulerHeight NOTIFY rulerHeightChanged)
    Q_PROPERTY(QString markerComment READ markerComment NOTIFY markerCommentChanged)
    Q_PROPERTY(int overlayType READ overlayType WRITE setOverlayType NOTIFY overlayTypeChanged)

public:
    MonitorProxy(GLWidget *parent);
    int seekPosition() const;
    /** brief: Returns true if we are still in a seek operation
     * */
    bool seeking() const;
    int position() const;
    int rulerHeight() const;
    int overlayType() const;
    void setOverlayType(int ix);
    QString markerComment() const;
    Q_INVOKABLE void requestSeekPosition(int pos);
    /** brief: Returns seek position or consumer position when not seeking
     * */
    int seekOrCurrentPosition() const;
    /** brief: update position and end seeking if we reached the requested seek position.
     *  returns true if the position was unchanged, false otherwise
     * */
    bool setPosition(int pos);
    void setMarkerComment(const QString &comment);
    void setSeekPosition(int pos);
    void pauseAndSeek(int pos);
    int zoneIn() const;
    int zoneOut() const;
    void setZoneIn(int pos);
    void setZoneOut(int pos);
    Q_INVOKABLE void setZone(int in, int out, bool sendUpdate = true);
    /** brief: Activate clip monitor if param is true, project monitor otherwise
     * */
    Q_INVOKABLE void activateClipMonitor(bool isClipMonitor);
    void setZone(QPoint zone, bool sendUpdate = true);
    void resetZone();
    QPoint zone() const;
    QImage extractFrame(int frame_position, const QString &path = QString(), int width = -1, int height = -1, bool useSourceProfile = false);
    Q_INVOKABLE QString toTimecode(int frames) const;

signals:
    void positionChanged();
    void seekPositionChanged();
    void seekRequestChanged();
    void zoneChanged();
    void saveZone();
    void markerCommentChanged();
    void rulerHeightChanged();
    void addSnap(int);
    void removeSnap(int);
    void triggerAction(const QString &name);
    void overlayTypeChanged();
    void seekNextKeyframe();
    void seekPreviousKeyframe();
    void addRemoveKeyframe();
    void seekToKeyframe();

private:
    GLWidget *q;
    int m_position;
    int m_seekPosition;
    int m_zoneIn;
    int m_zoneOut;
    QString m_markerComment;
};

#endif
