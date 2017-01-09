/***************************************************************************
                         krender.h  -  description
                            -------------------
   begin                : Fri Nov 22 2002
   copyright            : (C) 2002 by Jason Wood (jasonwood@blueyonder.co.uk)
   copyright            : (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * @class Render
 * @brief Client side of the interface to a renderer.
 *
 * REFACTORING NOTE -- There is most likely no point in trying to refactor
 * the renderer, it is better re-written directly (see refactoring branch)
 * since there is a lot of code duplication, no documentation, and several
 * hacks that have emerged from the previous two problems.
 *
 * From Kdenlive's point of view, you treat the Render object as the renderer,
 * and simply use it as if it was local. Calls are asynchronous - you send a
 * call out, and then receive the return value through the relevant signal that
 * get's emitted once the call completes.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "gentime.h"
#include "definitions.h"
#include "monitor/abstractmonitor.h"
#include "mltcontroller/effectscontroller.h"
#include <mlt/framework/mlt_types.h>

#include <QUrl>

#include <QtXml/qdom.h>
#include <QString>
#include <QMap>
#include <QList>
#include <QEvent>
#include <QMutex>
#include <QSemaphore>
#include <QTimer>

class KComboBox;
class BinController;
class ClipController;
class GLWidget;

namespace Mlt
{
class Consumer;
class Playlist;
class Properties;
class Tractor;
class Transition;
class Frame;
class Producer;
class Profile;
class Service;
class Event;
}

class MltErrorEvent : public QEvent
{
public:
    explicit MltErrorEvent(const QString &message)
        : QEvent(QEvent::User),
          m_message(message)
    {

    }

    QString message() const
    {
        return m_message;
    }

private:
    QString m_message;
};

class Render: public AbstractRender
{
Q_OBJECT public:

    enum FailStates { OK = 0,
                      APP_NOEXIST
                    };
    /** @brief Build a MLT Renderer
     *  @param rendererName A unique identifier for this renderer
     *  @param winid The parent widget identifier (required for SDL display). Set to 0 for OpenGL rendering
     *  @param profile The MLT profile used for the renderer (default one will be used if empty). */
    Render(Kdenlive::MonitorId rendererName, BinController *binController, GLWidget *qmlView, QWidget *parent = nullptr);

    /** @brief Destroy the MLT Renderer. */
    virtual ~Render();
    /** @brief In some trim modes, arrow keys move the trim pos but not timeline cursor.
     *  if byPassSeek is true, we don't seek renderer but emit a signal for timeline. */
    bool byPassSeek;

    /** @brief Seeks the renderer clip to the given time. */
    void seek(const GenTime &time);
    void seekToFrameDiff(int diff);

    /** @brief Sets the current MLT producer playlist.
     * @param list The xml describing the playlist
     * @param position (optional) time to seek to */
    int setSceneList(const QDomDocument &list, int position = 0);

    /** @brief Sets the current MLT producer playlist.
     * @param list new playlist
     * @param position (optional) time to seek to
     * @return 0 when it has success, different from 0 otherwise
     *
     * Creates the producer from the text playlist. */
    int setSceneList(QString playlist, int position = 0);
    bool updateProducer(Mlt::Producer *producer);
    bool setProducer(Mlt::Producer *producer, int position, bool isActive);

    /** @brief Get the current MLT producer playlist.
     * @return A string describing the playlist */
    const QString sceneList(const QString &root);

    /** @brief Tells the renderer to play the scene at the specified speed,
     * @param speed speed to play the scene to
     *
     * The speed is relative to normal playback, e.g. 1.0 is normal speed, 0.0
     * is paused, -1.0 means play backwards. It does not specify start/stop */
    void play(double speed);
    void switchPlay(bool play, double speed = 1.0);

    /** @brief Stops playing.
     * @param startTime time to seek to */
    void stop(const GenTime &startTime);
    int volume() const;

    QImage extractFrame(int frame_position, const QString &path = QString(), int width = -1, int height = -1);

    /** @brief Plays the scene starting from a specific time.
     * @param startTime time to start playing the scene from */
    void play(const GenTime &startTime);
    bool playZone(const GenTime &startTime, const GenTime &stopTime);
    void loopZone(const GenTime &startTime, const GenTime &stopTime);

    /** @brief Return true if we are currently playing */
    bool isPlaying() const;

    /** @brief Returns the speed at which the renderer is currently playing.
     *
     * It returns 0.0 when the renderer is not playing anything. */
    double playSpeed() const;

    /** @brief Returns the current seek position of the renderer. */
    GenTime seekPosition() const;
    int seekFramePosition() const;

    void emitFrameUpdated(Mlt::Frame &);

    double fps() const;

    /** @brief Returns the width of a frame for this profile. */
    int frameRenderWidth() const;
    /** @brief Returns the display width of a frame for this profile. */
    int renderWidth() const;
    /** @brief Returns the height of a frame for this profile. */
    int renderHeight() const;

    /** @brief Returns display aspect ratio. */
    double dar() const;
    /** @brief Returns sample aspect ratio. */
    double sar() const;
    /** @brief Start the MLT monitor consumer. */
    void startConsumer();

    /*
     * Playlist manipulation.
     */
    void mltCheckLength(Mlt::Tractor *tractor);
    Mlt::Producer *getSlowmotionProducer(const QString &url);
    void mltInsertSpace(const QMap<int, int> &trackClipStartList, const QMap<int, int> &trackTransitionStartList, int track, const GenTime &duration, const GenTime &timeOffset);
    int mltGetSpaceLength(const GenTime &pos, int track, bool fromBlankStart);
    bool mltResizeClipCrop(const ItemInfo &info, GenTime newCropStart);

    QList<TransitionInfo> mltInsertTrack(int ix, const QString &name, bool videoTrack);

    //const QList<Mlt::Producer *> producersList();
    void setDropFrames(bool show);
    /** @brief Sets an MLT consumer property. */
    void setConsumerProperty(const QString &name, const QString &value);

    void showAudio(Mlt::Frame &);

    QList<int> checkTrackSequence(int);
    void sendFrameUpdate() Q_DECL_OVERRIDE;

    /** @brief Returns a pointer to the main producer. */
    Mlt::Producer *getProducer();
    /** @brief Returns a pointer to the bin's playlist. */

    /** @brief Lock the MLT service */
    Mlt::Tractor *lockService();
    /** @brief Unlock the MLT service */
    void unlockService(Mlt::Tractor *tractor);
    const QString activeClipId();
    /** @brief Fill a combobox with the found blackmagic devices */
    static bool getBlackMagicDeviceList(KComboBox *devicelist, bool force = false);
    static bool getBlackMagicOutputDeviceList(KComboBox *devicelist, bool force = false);
    /** @brief Get current seek pos requested or SEEK_INACTIVE if we are not currently seeking */
    int requestedSeekPosition;
    /** @brief Get current seek pos requested of current producer pos if not seeking */
    int getCurrentSeekPosition() const;
    /** @brief Create a producer from url and load it in the monitor  */
    void loadUrl(const QString &url);
    /** @brief Check if the installed FFmpeg / Libav supports x11grab */
    static bool checkX11Grab();

    /** @brief Get a track producer from a clip's id
     *  Deprecated, track producers are now handled in timeline/track.cpp
     */
    Q_DECL_DEPRECATED Mlt::Producer *getTrackProducer(const QString &id, int track, bool audioOnly = false, bool videoOnly = false);

    /** @brief Ask to set this monitor as active */
    void setActiveMonitor();

    QSemaphore showFrameSemaphore;
    bool externalConsumer;
    /** @brief Returns the current version of an MLT's producer module */
    double getMltVersionInfo(const QString &tag);
    /** @brief Get a clip's master producer */
    Mlt::Producer *getBinProducer(const QString &id);
    /** @brief Get a clip's video only producer */
    Mlt::Producer *getBinVideoProducer(const QString &id);
    /** @brief Load extra producers (video only, slowmotion) from timeline */
    void loadExtraProducer(const QString &id, Mlt::Producer *prod);
    /** @brief Get a property from the bin's playlist */
    const QString getBinProperty(const QString &name);
    void setVolume(double volume);
    /** @brief Stop all activities in preparation for a change in profile */
    void prepareProfileReset(double fps);
    void finishProfileReset();
    void updateSlowMotionProducers(const QString &id, const QMap<QString, QString> &passProperties);
    void preparePreviewRendering(const QString &sceneListFile);
    void silentSeek(int time);

private:

    /** @brief The name of this renderer.
     *
     * Useful to identify the renderers by what they do - e.g. background
     * rendering, workspace monitor, etc. */
    Kdenlive::MonitorId m_name;
    Mlt::Consumer *m_mltConsumer;
    Mlt::Producer *m_mltProducer;
    Mlt::Event *m_showFrameEvent;
    Mlt::Event *m_pauseEvent;
    BinController *m_binController;
    GLWidget *m_qmlView;
    double m_fps;

    /** @brief True if we are playing a zone.
     *
     * It's determined by the "in" and "out" properties being temporarily
     * changed. */
    bool m_isZoneMode;
    bool m_isLoopMode;
    GenTime m_loopStart;

    Mlt::Producer *m_blackClip;

    QTimer m_refreshTimer;
    QMutex m_mutex;
    QMutex m_infoMutex;

    QLocale m_locale;
    /** @brief True if this monitor is active. */
    bool m_isActive;
    /** @brief True if the consumer is currently refreshing itself. */
    bool m_isRefreshing;
    void closeMlt();
    QMap<QString, Mlt::Producer *> m_slowmotionProducers;

    /** @brief Build the MLT Consumer object with initial settings.
     *  @param profileName The MLT profile to use for the consumer */
    //void buildConsumer();
    /** @brief Restore normal mode */
    void resetZoneMode();
    void fillSlowMotionProducers();
    /** @brief Make sure we inform MLT if we need a lot of threads for avformat producer */
    void checkMaxThreads();
    /** @brief Clone serialisable properties only */
    void cloneProperties(Mlt::Properties &dest, Mlt::Properties &source);
    /** @brief Get a track producer from a clip's id */
    Mlt::Producer *getProducerForTrack(Mlt::Playlist &trackPlaylist, const QString &clipId);

private slots:

    /** @brief Refreshes the monitor display. */
    void refresh();
    void slotCheckSeeking();

signals:
    /** @brief The renderer stopped, either playing or rendering. */
    void stopped();

    /** @brief The renderer started playing. */
    void playing(double);

    /** @brief The renderer started rendering. */
    void rendering(const GenTime &);

    /** @brief An error occurred within this renderer. */
    void error(const QString &, const QString &);
    void durationChanged(int, int offset = 0);
    void rendererPosition(int);
    void rendererStopped(int);

    /** @brief A clip has changed, we must reload timeline producers. */
    void replaceTimelineProducer(const QString &);
    void updateTimelineProducer(const QString &);
    /** @brief Load project notes. */
    void setDocumentNotes(const QString &);
    /** @brief The renderer received a reply to a getFileProperties request. */
    void gotFileProperties(requestClipInfo, ClipController *);

    /** @brief A frame's image has to be shown.
     *
     * Used in Mac OS X. */
    void showImageSignal(const QImage &);
    void showAudioSignal(const QVector<double> &);
    void checkSeeking();
    /** @brief Activate current monitor. */
    void activateMonitor(Kdenlive::MonitorId);
    void mltFrameReceived(Mlt::Frame *);
    /** @brief We want to replace a clip with another, but before we need to change clip producer id so that there is no interference*/
    void prepareTimelineReplacement(const QString &);
    /** @brief When in bypass seek mode, we don't seek but pass over the position diff. */
    void renderSeek(int);

public slots:

    /** @brief Starts the consumer. */
    void start();

    /** @brief Stops the consumer. */
    void stop();
    int getLength();

    /** @brief Checks if the file is readable by MLT. */
    bool isValid(const QUrl &url);

    void slotSwitchFullscreen();
    void seekToFrame(int pos);
    /** @brief Starts a timer to query for a refresh. */
    void doRefresh();

    /** @brief Save a part of current timeline to an xml file. */
    void saveZone(const QString &projectFolder, QPoint zone);

    /** @brief Renderer moved to a new frame, check seeking */
    bool checkFrameNumber(int pos);
    /** @brief Keep a reference to slowmo producer. Returns false is producer is already stored */
    bool storeSlowmotionProducer(const QString &url, Mlt::Producer *prod, bool replace = false);
    void seek(int time);
};

#endif
