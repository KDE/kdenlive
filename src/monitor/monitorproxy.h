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

#include "definitions.h"
#include <QImage>
#include <QUrl>
#include <QObject>

class GLWidget;

class MonitorProxy : public QObject
{
    Q_OBJECT
    // Q_PROPERTY(int consumerPosition READ consumerPosition NOTIFY consumerPositionChanged)
    Q_PROPERTY(int position MEMBER m_position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QPoint profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(int seekFinished MEMBER m_seekFinished NOTIFY seekFinishedChanged)
    Q_PROPERTY(int zoneIn READ zoneIn WRITE setZoneIn NOTIFY zoneChanged)
    Q_PROPERTY(int zoneOut READ zoneOut WRITE setZoneOut NOTIFY zoneChanged)
    Q_PROPERTY(int rulerHeight READ rulerHeight WRITE setRulerHeight NOTIFY rulerHeightChanged)
    Q_PROPERTY(QString markerComment READ markerComment NOTIFY markerCommentChanged)
    Q_PROPERTY(QList <int> audioStreams MEMBER m_audioStreams NOTIFY audioThumbChanged)
    Q_PROPERTY(QList <int> audioChannels MEMBER m_audioChannels NOTIFY audioThumbChanged)
    Q_PROPERTY(int overlayType READ overlayType WRITE setOverlayType NOTIFY overlayTypeChanged)
    Q_PROPERTY(QColor thumbColor1 READ thumbColor1 NOTIFY colorsChanged)
    Q_PROPERTY(QColor thumbColor2 READ thumbColor2 NOTIFY colorsChanged)
    Q_PROPERTY(bool autoKeyframe READ autoKeyframe NOTIFY autoKeyframeChanged)
    Q_PROPERTY(bool audioThumbFormat READ audioThumbFormat NOTIFY audioThumbFormatChanged)
    /** @brief: Returns true if current clip in monitor has Audio and Video
     * */
    Q_PROPERTY(bool clipHasAV MEMBER m_hasAV NOTIFY clipHasAVChanged)
    /** @brief: Contains the name of clip currently displayed in monitor
     * */
    Q_PROPERTY(QString clipName MEMBER m_clipName NOTIFY clipNameChanged)
    Q_PROPERTY(QString clipStream MEMBER m_clipStream NOTIFY clipStreamChanged)
    /** @brief: Contains the name of clip currently displayed in monitor
     * */
    Q_PROPERTY(int clipType MEMBER m_clipType NOTIFY clipTypeChanged)
    Q_PROPERTY(int clipId MEMBER m_clipId NOTIFY clipIdChanged)

public:
    MonitorProxy(GLWidget *parent);
    /** brief: Returns true if we are still in a seek operation
     * */
    int rulerHeight() const;
    int overlayType() const;
    void setOverlayType(int ix);
    QString markerComment() const;
    /** brief: update position and end seeking if we reached the requested seek position.
     *  returns true if the position was unchanged, false otherwise
     * */
    int getPosition() const;
    Q_INVOKABLE bool setPosition(int pos);
    Q_INVOKABLE void seek(int delta, uint modifiers);
    Q_INVOKABLE QColor thumbColor1() const;
    Q_INVOKABLE QColor thumbColor2() const;
    Q_INVOKABLE bool audioThumbFormat() const;
    void positionFromConsumer(int pos, bool playing);
    void setMarkerComment(const QString &comment);
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
    Q_INVOKABLE void startZoneMove();
    Q_INVOKABLE void endZoneMove();
    Q_INVOKABLE double fps() const;
    Q_INVOKABLE void switchAutoKeyframe();
    Q_INVOKABLE bool autoKeyframe() const;
    QPoint profile();
    void setClipProperties(int clipId, ClipType::ProducerType type, bool hasAV, const QString clipName);
    void setAudioThumb(const QList <int> streamIndexes = QList <int>(), QList <int> channels = QList <int>());
    void setAudioStream(const QString &name);
    void setRulerHeight(int height);

signals:
    void positionChanged(int);
    void seekFinishedChanged();
    void requestSeek(int pos);
    void zoneChanged();
    void saveZone(const QPoint zone);
    void saveZoneWithUndo(const QPoint, const QPoint&);
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
    void clipHasAVChanged();
    void clipNameChanged();
    void clipStreamChanged();
    void clipTypeChanged();
    void clipIdChanged();
    void audioThumbChanged();
    void colorsChanged();
    void audioThumbFormatChanged();
    void profileChanged();
    void autoKeyframeChanged();

private:
    GLWidget *q;
    int m_position;
    int m_zoneIn;
    int m_zoneOut;
    bool m_hasAV;
    QList <int> m_audioStreams;
    QList <int> m_audioChannels;
    QString m_markerComment;
    QString m_clipName;
    QString m_clipStream;
    int m_clipType;
    int m_clipId;
    bool m_seekFinished;
    QPoint m_undoZone;
};

#endif
