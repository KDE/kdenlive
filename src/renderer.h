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
#include <QFuture>
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
class Field;
class Producer;
class Repository;
class Filter;
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

    QString message() const {
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
    Render(Kdenlive::MonitorId rendererName, BinController *binController, GLWidget *qmlView, QWidget *parent = 0);

    /** @brief Destroy the MLT Renderer. */
    virtual ~Render();

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
    const QString sceneList();
    bool saveSceneList(QString path, QDomElement kdenliveData = QDomElement());

    /** @brief Tells the renderer to play the scene at the specified speed,
     * @param speed speed to play the scene to
     *
     * The speed is relative to normal playback, e.g. 1.0 is normal speed, 0.0
     * is paused, -1.0 means play backwards. It does not specify start/stop */
    void play(double speed);
    void switchPlay(bool play);
    void pause();

    /** @brief Stops playing.
     * @param startTime time to seek to */
    void stop(const GenTime &startTime);
    int volume() const;

    QImage extractFrame(int frame_position, const QString &path = QString(), int width = -1, int height = -1);

    /** @brief Plays the scene starting from a specific time.
     * @param startTime time to start playing the scene from */
    void play(const GenTime & startTime);
    bool playZone(const GenTime & startTime, const GenTime & stopTime);
    void loopZone(const GenTime & startTime, const GenTime & stopTime);
    
    /** @brief Save a clip in timeline to an xml playlist. */
    bool saveClip(int track, const GenTime &position, const QUrl &url, const QString &desc = QString());

    /** @brief Return true if we are currently playing */
    bool isPlaying() const;

    /** @brief Returns the speed at which the renderer is currently playing.
     *
     * It returns 0.0 when the renderer is not playing anything. */
    double playSpeed() const;

    /** @brief Returns the current seek position of the renderer. */
    GenTime seekPosition() const;
    int seekFramePosition() const;

    void emitFrameUpdated(Mlt::Frame&);

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
    /** @brief If monitor is active, refresh it. */
    void refreshIfActive();
    /** @brief Start the MLT monitor consumer. */
    void startConsumer();

    /*
     * Playlist manipulation.
     */
    void mltCheckLength(Mlt::Tractor *tractor);
    Mlt::Producer *getSlowmotionProducer(const QString &url);
    void mltInsertSpace(QMap <int, int> trackClipStartList, QMap <int, int> trackTransitionStartList, int track, const GenTime &duration, const GenTime &timeOffset);
    int mltGetSpaceLength(const GenTime &pos, int track, bool fromBlankStart);

    /** @brief Returns the duration/length of @param track as reported by the track producer. */
    int mltTrackDuration(int track);

    bool mltResizeClipCrop(ItemInfo info, GenTime newCropStart);

    /** @brief Deletes an effect from a clip in MLT's playlist. */
    bool mltRemoveEffect(int track, GenTime position, int index, bool updateIndex, bool doRefresh = true);
    static bool removeFilterFromService(Mlt::Service service, int effectIndex, bool updateIndex);
    bool mltRemoveTrackEffect(int track, int index, bool updateIndex);

    /** @brief Adds an effect to a clip in MLT's playlist. */
    bool mltAddEffect(int track, GenTime position, EffectsParameterList params, bool doRefresh = true);
    static bool addFilterToService(Mlt::Service service, EffectsParameterList params, int duration);
    bool mltAddEffect(Mlt::Service service, EffectsParameterList params, int duration, bool doRefresh);
    bool mltAddTrackEffect(int track, EffectsParameterList params);
    
    /** @brief Enable / disable clip effects.
     * @param track The track where the clip is
     * @param position The start position of the clip
     * @param effectIndexes The list of effect indexes to enable / disable
     * @param disable True if effects should be disabled, false otherwise */
    bool mltEnableEffects(int track, const GenTime &position, const QList<int> &effectIndexes, bool disable);
    /** @brief Enable / disable track effects.
     * @param track The track where the effect is
     * @param effectIndexes The list of effect indexes to enable / disable
     * @param disable True if effects should be disabled, false otherwise */
    bool mltEnableTrackEffects(int track, const QList<int> &effectIndexes, bool disable);

    /** @brief Edits an effect parameters in MLT's playlist. */
    bool mltEditEffect(int track, const GenTime &position, EffectsParameterList params, bool replaceEffect);
    bool mltEditTrackEffect(int track, EffectsParameterList params);

    /** @brief Updates the "kdenlive_ix" (index) value of an effect. */
    void mltUpdateEffectPosition(int track, const GenTime &position, int oldPos, int newPos);

    /** @brief Changes the order of effects in MLT's playlist.
     *
     * It switches effects from oldPos and newPos, updating the "kdenlive_ix"
     * (index) value. */
    void mltMoveEffect(int track, const GenTime &position, int oldPos, int newPos);
    void mltMoveTrackEffect(int track, int oldPos, int newPos);

    QList <TransitionInfo> mltInsertTrack(int ix, const QString &name, bool videoTrack);

    void mltPlantTransition(Mlt::Field *field, Mlt::Transition &tr, int a_track, int b_track);
    Mlt::Producer *invalidProducer(const QString &id);

    //const QList <Mlt::Producer *> producersList();
    void setDropFrames(bool show);
    /** @brief Sets an MLT consumer property. */
    void setConsumerProperty(const QString &name, const QString &value);

    void showAudio(Mlt::Frame&);
    
    QList <int> checkTrackSequence(int);
    void sendFrameUpdate();

    /** @brief Returns a pointer to the main producer. */
    Mlt::Producer *getProducer();
    /** @brief Returns a pointer to the bin's playlist. */

    /** @brief Returns the number of clips to process (When requesting clip info). */
    int processingItems();
    /** @brief Force processing of clip with selected id. */
    void forceProcessing(const QString &id);
    /** @brief Are we currently processing clip with selected id. */
    bool isProcessing(const QString &id);
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
    void prepareProfileReset();
    void updateSlowMotionProducers(const QString &id, QMap <QString, QString> passProperties);
    static QMap<QString, QString> mltGetTransitionParamsFromXml(const QDomElement &xml);

private:

    /** @brief The name of this renderer.
     *
     * Useful to identify the renderers by what they do - e.g. background
     * rendering, workspace monitor, etc. */
    Kdenlive::MonitorId m_name;
    Mlt::Consumer * m_mltConsumer;
    Mlt::Producer * m_mltProducer;
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

    /** @brief True when the monitor is in split view. */
    bool m_isSplitView;

    Mlt::Producer *m_blackClip;

    QTimer m_refreshTimer;
    QMutex m_mutex;
    QMutex m_infoMutex;

    QLocale m_locale;
    QFuture <void> m_infoThread;
    QList <requestClipInfo> m_requestList;
    /** @brief True if this monitor is active. */
    bool m_isActive;
    /** @brief True if the consumer is currently refreshing itself. */
    bool m_isRefreshing;

    void closeMlt();
    void mltPasteEffects(Mlt::Producer *source, Mlt::Producer *dest);
    QMap<QString, Mlt::Producer *> m_slowmotionProducers;
    /** @brief The ids of the clips that are currently being loaded for info query */
    QStringList m_processingClipId;

    /** @brief Build the MLT Consumer object with initial settings.
     *  @param profileName The MLT profile to use for the consumer */
    //void buildConsumer();
    void resetZoneMode();
    void fillSlowMotionProducers();
    /** @brief Make sure we inform MLT if we need a lot of threads for avformat producer */
    void checkMaxThreads();
    /** @brief Clone serialisable properties only */
    void cloneProperties(Mlt::Properties &dest, Mlt::Properties &source);
    /** @brief Get a track producer from a clip's id */
    Mlt::Producer *getProducerForTrack(Mlt::Playlist &trackPlaylist, const QString &clipId);
    ClipType getTypeForService(const QString &id, const QString &path) const;
    /** @brief Pass xml values to an MLT producer at build time */
    void processProducerProperties(Mlt::Producer *prod, QDomElement xml);
    
private slots:

    /** @brief Refreshes the monitor display. */
    void refresh();
    /** @brief Process the clip info requests (in a separate thread). */
    void processFileProperties();
    /** @brief A clip with multiple video streams was found, ask what to do. */
    void slotMultiStreamProducerFound(const QString &path, QList<int> audio_list, QList<int> video_list, stringMap data);
    void slotCheckSeeking();

signals:

    /** @brief The renderer received a reply to a getFileProperties request. */
    void gotFileProperties(requestClipInfo,ClipController *);

    /** @brief The renderer received a reply to a getImage request. */
    void replyGetImage(const QString &, const QImage &,bool fromFile = false);
    
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
    /** @brief The clip is not valid, should be removed from project. */
    void removeInvalidClip(const QString &, bool replaceProducer);
    /** @brief The proxy is not valid, should be deleted.
     *  @param id The original clip's id
     *  @param durationError Should be set to true if the proxy failed because it has not same length as original clip
     */
    void removeInvalidProxy(const QString &id, bool durationError);
    /** @brief A proxy clip is missing, ask for creation. */
    void requestProxy(QString);
    /** @brief A multiple stream clip was found. */
    void multiStreamFound(const QString &,QList<int>,QList<int>,stringMap data);
    /** @brief A clip has changed, we must reload timeline producers. */
    void replaceTimelineProducer(const QString&);
    void updateTimelineProducer(const QString&);
    /** @brief Load project notes. */
    void setDocumentNotes(const QString&);


    /** @brief A frame's image has to be shown.
     *
     * Used in Mac OS X. */
    void showImageSignal(QImage);
    void showAudioSignal(const QVector<double> &);
    void addClip(const QString &, const QMap<QString,QString>&);
    void checkSeeking();
    /** @brief Activate current monitor. */
    void activateMonitor(Kdenlive::MonitorId);
    void mltFrameReceived(Mlt::Frame *);
    void infoProcessingFinished();
    /** @brief We want to replace a clip with another, but before we need to change clip producer id so that there is no interference*/
    void prepareTimelineReplacement(const QString &);

public slots:

    /** @brief Starts the consumer. */
    void start();

    /** @brief Stops the consumer. */
    void stop();
    int getLength();

    /** @brief Checks if the file is readable by MLT. */
    bool isValid(const QUrl &url);

    void slotSplitView(bool doit);
    void slotSwitchFullscreen();
    void seekToFrame(int pos);
    /** @brief Starts a timer to query for a refresh. */
    void doRefresh();
    /** @brief Processing of this clip is over, producer was set on clip, remove from list. */
    void slotProcessingDone(const QString &id);
    void emitFrameUpdated(QImage img);
    
    /** @brief Save a part of current timeline to an xml file. */
     void saveZone(QPoint zone);

    /** @brief Requests the file properties for the specified URL (will be put in a queue list)
    @param xml The xml parameters for the clip
    @param clipId The clip Id string
    @param imageHeight The height (in pixels) of the returned thumbnail (height of a treewidgetitem in projectlist)
    @param replaceProducer If true, the MLT producer will be recreated */
    void getFileProperties(const QDomElement &xml, const QString &clipId, int imageHeight, bool replaceProducer = true);
    /** @brief Renderer moved to a new frame, check seeking */
    void checkFrameNumber(int pos);
    void storeSlowmotionProducer(const QString &url, Mlt::Producer *prod, bool replace = false);
    void seek(int time);
};

#endif
