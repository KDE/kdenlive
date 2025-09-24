/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "videowidget.h"
#include <QImage>
#include <QObject>
#include <QUrl>

class TimecodeDisplay;

/** @class MonitorProxy
    @brief This class is a wrapper around the monitor / glwidget and handles communication
    with the qml overlay through its properties.
 */
class MonitorProxy : public QObject
{
    friend class AutomaskHelper;

    Q_OBJECT

    // Q_PROPERTY(int consumerPosition READ consumerPosition NOTIFY consumerPositionChanged)
    Q_PROPERTY(int position MEMBER m_position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QPoint profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(int seekFinished MEMBER m_seekFinished NOTIFY seekFinishedChanged)
    Q_PROPERTY(int zoneIn READ zoneIn WRITE setZoneIn NOTIFY zoneChanged)
    Q_PROPERTY(int zoneOut READ zoneOut WRITE setZoneOut NOTIFY zoneChanged)
    Q_PROPERTY(int rulerHeight READ rulerHeight WRITE setRulerHeight NOTIFY rulerHeightChanged)
    Q_PROPERTY(QString markerComment MEMBER m_markerComment NOTIFY markerChanged)
    Q_PROPERTY(QColor markerColor MEMBER m_markerColor NOTIFY markerChanged)
    Q_PROPERTY(QString timecode READ timecode NOTIFY timecodeChanged)
    Q_PROPERTY(QString trimmingTC1 READ trimmingTC1 NOTIFY trimmingTC1Changed)
    Q_PROPERTY(QString trimmingTC2 READ trimmingTC2 NOTIFY trimmingTC2Changed)
    Q_PROPERTY(QList <int> audioStreams MEMBER m_audioStreams NOTIFY audioThumbChanged)
    Q_PROPERTY(QList <int> audioChannels MEMBER m_audioChannels NOTIFY audioThumbChanged)
    Q_PROPERTY(int clipBounds MEMBER m_boundsCount NOTIFY clipBoundsChanged)
    Q_PROPERTY(int overlayType READ overlayType WRITE setOverlayType NOTIFY overlayTypeChanged)
    Q_PROPERTY(int maskOpacity READ maskOpacity WRITE setMaskOpacity NOTIFY maskOpacityChanged)
    Q_PROPERTY(bool maskInverted READ maskInverted WRITE setMaskInverted)
    Q_PROPERTY(MaskModeType::MaskCreationMode maskMode MEMBER m_maskMode NOTIFY maskModeChanged)
    Q_PROPERTY(double speed MEMBER m_speed NOTIFY speedChanged)
    Q_PROPERTY(QStringList lastClips MEMBER m_lastClips NOTIFY lastClipsChanged)
    /** @brief Returns true if current clip in monitor has Audio and Video
     * */
    Q_PROPERTY(bool clipHasAV MEMBER m_hasAV NOTIFY clipHasAVChanged)
    /** @brief Contains the name of clip currently displayed in monitor
     * */
    Q_PROPERTY(QString clipName MEMBER m_clipName NOTIFY clipNameChanged)
    Q_PROPERTY(QUrl previewOverlay MEMBER m_previewOverlay NOTIFY previewOverlayChanged)
    Q_PROPERTY(QString clipStream MEMBER m_clipStream NOTIFY clipStreamChanged)
    /** @brief Contains the name of clip currently displayed in monitor
     * */
    Q_PROPERTY(int clipType MEMBER m_clipType NOTIFY clipTypeChanged)
    Q_PROPERTY(int clipId MEMBER m_clipId NOTIFY clipIdChanged)
    Q_PROPERTY(QStringList runningJobs MEMBER m_runningJobs NOTIFY runningJobsChanged)
    Q_PROPERTY(QList<int> jobsProgress MEMBER m_jobsProgress NOTIFY jobsProgressChanged)
    Q_PROPERTY(QStringList jobsUuids MEMBER m_jobsUuids NOTIFY jobsProgressChanged)
    Q_PROPERTY(bool monitorIsActive READ monitorIsActive NOTIFY activeMonitorChanged)
    Q_PROPERTY(bool isKeyframe READ isKeyframe WRITE setIsKeyframe NOTIFY isKeyframeChanged)
    Q_PROPERTY(bool cursorOutsideEffect READ cursorOutsideEffect WRITE setCursorOutsideEffect NOTIFY cursorOutsideEffectChanged)

public:
    MonitorProxy(VideoWidget *parent);
    /** brief: Returns true if we are still in a seek operation
     * */
    int rulerHeight() const;
    int overlayType() const;
    void setOverlayType(int ix);
    int maskOpacity() const;
    void setMaskOpacity(int opacity);
    bool maskInverted() const;
    void setMaskInverted(bool);
    void setMaskMode(MaskModeType::MaskCreationMode ix);
    MaskModeType::MaskCreationMode maskMode() const;
    const QString trimmingTC1() const;
    const QString trimmingTC2() const;
    const QString timecode() const;
    int getPosition() const;
    /** @brief update position and end seeking if we reached the requested seek position.
     *  returns true if the position was unchanged, false otherwise
     * */
    Q_INVOKABLE bool setPosition(int pos);
    bool setPositionAdvanced(int pos, bool noAudioScrub);
    Q_INVOKABLE void seek(int delta, uint modifiers);
    Q_INVOKABLE QByteArray getUuid() const;
    Q_INVOKABLE void selectClip(int ix);
    Q_INVOKABLE const QPoint clipBoundary(int ix);
    void positionFromConsumer(int pos, bool playing);
    void setMarker(const QString &comment, const QColor &color);
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
    Q_INVOKABLE QString toTimecode(int frames) const;
    Q_INVOKABLE void startZoneMove();
    Q_INVOKABLE void endZoneMove();
    Q_INVOKABLE void switchGrid();
    Q_INVOKABLE double fps() const;
    Q_INVOKABLE void setWidgetKeyBinding(const QString &text = QString()) const;
    Q_INVOKABLE void addEffect(const QString &effectData, const QString &effectSource);
    Q_INVOKABLE void terminateJob(const QString &uuid);
    /** @brief Resize a range marker in monitor view
     * @param position The marker position in frames
     * @param duration The new duration in frames
     * @param isStart True if resizing from start, false if resizing from end
     * @param newPosition The new start position when resizing from start (optional)
     */
    Q_INVOKABLE void resizeMarker(int position, int duration, bool isStart = false, int newPosition = -1);
    /** @brief Create a range marker from the current monitor zone
     * @param comment Optional comment for the marker
     * @param type Marker type
     * @return true if successful
     */
    Q_INVOKABLE bool createRangeMarkerFromZone(const QString &comment = QString(), int type = -1);
    QPoint profile();
    QImage extractFrame(const QString &path = QString(), int width = -1, int height = -1, bool useSourceProfile = false);
    void setClipProperties(int clipId, ClipType::ProducerType type, bool hasAV, const QString &clipName);
    void setAudioThumb(const QList <int> &streamIndexes = QList <int>(), const QList <int> &channels = QList <int>());
    void setAudioStream(const QString &name);
    void setRulerHeight(int height);
    /** @brief Store a reference to the timecode display */
    void setTimeCode(TimecodeDisplay *td);
    /** @brief Set position in frames to be displayed in the monitor overlay for preview tile one
     *  @param frames Position in frames
     *  @param isRelative Whether @p frames is the absolute position (overwrite current) or an offset position (subtract from current)
     */
    void setTrimmingTC1(int frames, bool isRelativ = false);
    /** @brief Set position in frames to be displayed in the monitor overlay for preview tile two
     *  @see setTrimmingTC1
     */
    void setTrimmingTC2(int frames, bool isRelativ = false);
    /** @brief When the producer changes, ensure we reset the stored position*/
    void resetPosition();
    /** @brief Used to display qml info about speed*/
    void setSpeed(double speed);
    /** @brief Temporarily set timeline cursor position (-1 to hide it)*/
    void setCursorPosition(int pos);
    void setJobsProgress(const ObjectId &owner, const QStringList &jobNames, const QList<int> &jobProgress, const QStringList &jobUuids);
    void clearJobsProgress();
    bool monitorIsActive() const;
    void setIsKeyframe(bool isKeyframe);
    bool isKeyframe() const;
    void setCursorOutsideEffect(bool isOutside);
    bool cursorOutsideEffect() const;

Q_SIGNALS:
    void positionChanged(int);
    void seekFinishedChanged();
    void requestSeek(int pos, bool noAudioScrub);
    void zoneChanged();
    void saveZone(const QPoint zone);
    void saveZoneWithUndo(const QPoint, const QPoint&);
    void markerChanged();
    void rulerHeightChanged();
    void addSnap(int);
    void removeSnap(int);
    void triggerAction(const QString &name);
    void overlayTypeChanged();
    void maskOpacityChanged();
    void maskModeChanged();
    void builtinEffectsEnabledChanged();
    void addRemoveKeyframe(bool addOnly = false);
    /** @brief Seek to an effect keyframe
     *  @param ix the index of the keyframe we want to reach
     *  @param offset if offset != 0, the ix param is ignored and we seek to previous (-1) or next(+1) keyframe
     */
    void seekToKeyframe(int ix, int offset);
    void clipHasAVChanged();
    void clipNameChanged();
    void clipStreamChanged();
    void clipTypeChanged();
    void clipIdChanged();
    void audioThumbChanged();
    void audioThumbFormatChanged();
    void audioThumbNormalizeChanged();
    void profileChanged();
    void timecodeChanged();
    void trimmingTC1Changed();
    void trimmingTC2Changed();
    void speedChanged();
    void clipBoundsChanged();
    void runningJobsChanged();
    void jobsProgressChanged();
    void addTimelineEffect(const QStringList &);
    void lastClipsChanged();
    /** @brief Switch to another clip at the same time position that uses the same effect scene*/
    void switchFocusClip();
    /** @brief Enable build-in transform effect*/
    void enableTransform();
    void previewOverlayChanged();
    void refreshMask();
    void activeMonitorChanged();
    void isKeyframeChanged();
    void cursorOutsideEffectChanged();

private:
    VideoWidget *q;
    int m_position;
    int m_zoneIn;
    int m_zoneOut;
    bool m_hasAV;
    double m_speed;
    QList <int> m_audioStreams;
    QList <int> m_audioChannels;
    QString m_markerComment;
    QColor m_markerColor;
    QString m_clipName;
    QString m_clipStream;
    int m_clipType;
    int m_clipId;
    bool m_seekFinished;
    QPoint m_undoZone;
    TimecodeDisplay *m_td;
    int m_trimmingFrames1;
    int m_trimmingFrames2;
    QVector <QPoint> m_clipBounds;
    int m_boundsCount;
    QStringList m_runningJobs;
    QList<int> m_jobsProgress;
    QStringList m_jobsUuids;
    QVector<std::pair<int, QString>> m_lastClipsIds;
    QStringList m_lastClips;
    bool m_switchFlag{false};
    bool m_isKeyframe{false};
    bool m_cursorOutsideEffect{true};

protected:
    QUrl m_previewOverlay;
    /** @brief Mode for mask overlay. 0 = preview keyframe image, 1 = preview video mask*/
    MaskModeType::MaskCreationMode m_maskMode{MaskModeType::MaskNone};

public Q_SLOTS:
    void updateClipBounds(const QVector <QPoint>&bounds);
    void clipDeleted(int cid);
    void documentClosed();
    void extractFrameToFile(int frame_position, const QStringList &pathInfo, bool addToProject = false, bool useSourceProfile = false);

private Q_SLOTS:
    void updateClipName(int id, const QString newName);
};
