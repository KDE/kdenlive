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
 * From Kdenlive's point of view, you treat the Render object as the renderer,
 * and simply use it as if it was local. Calls are asynchronous - you send a
 * call out, and then receive the return value through the relevant signal that
 * get's emitted once the call completes.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "gentime.h"
#include "definitions.h"
#include "abstractmonitor.h"

#include <mlt/framework/mlt_types.h>

#include <kurl.h>

#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <QList>
#include <QEvent>
#include <QMutex>
#include <QFuture>


class QTimer;
class QPixmap;

class KComboBox;

namespace Mlt
{
class FilteredConsumer;
class Playlist;
class Tractor;
class Transition;
class Frame;
class Field;
class Producer;
class Filter;
class Profile;
class Service;
class Event;
};

struct requestClipInfo {
    QDomElement xml;
    QString clipId;
    int imageHeight;
    bool replaceProducer;

bool operator==(const requestClipInfo &a)
{
    return clipId == a.clipId;
}
};

class MltErrorEvent : public QEvent
{
public:
    MltErrorEvent(QString message) : QEvent(QEvent::User), m_message(message) {}
    QString message() const {
        return m_message;
    }

private:
    QString m_message;
};

/* transport slave namespace */
namespace Slave
{
	namespace Perm
	{
		const static unsigned int Internal 	= (1<<0);
		const static unsigned int Jack 		= (1<<1);
	};

    enum Type
    {
    	Internal = 0,
    	Jack
    };
};

/* device namespace */
namespace Device
{
	enum Type
	{
		Mlt	= 0,
		Jack
	};
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
    Render(Kdenlive::MONITORID rendererName, int winid, QString profile = QString(), QWidget *parent = 0);

    /** @brief Destroy the MLT Renderer. */
    virtual ~Render();

    /** @brief Seeks the renderer clip to the given time. */
    void seek(GenTime time);
    void seek(int time, bool slave = false);
    void seekToFrameDiff(int diff);

//#ifdef USE_JACK
    /** @brief*/
    void openDevice(Device::Type dev);
    void closeDevice(Device::Type dev);

    inline bool isDeviceActive(Device::Type dev)
		{return (m_activeDevice == dev);}

    /** @brief */
    inline bool isSlaveActive(Slave::Type slave)
    	{return (m_activeSlave == slave);}

    void enableSlave(Slave::Type slave);

    inline bool isSlavePermSet(unsigned int perm)
    	{return ((perm & m_slavePerm) == perm);}

    inline void setSlavePerm(unsigned int perm)
    	{m_slavePerm |= perm;}

    inline void resetSlavePerm(unsigned int perm)
    	{m_slavePerm |= perm;}

    QPixmap getImageThumbnail(KUrl url, int width, int height);

    /** @brief Sets the current MLT producer playlist.
     * @param list The xml describing the playlist
     * @param position (optional) time to seek to */
    int setSceneList(QDomDocument list, int position = 0);

    /** @brief Sets the current MLT producer playlist.
     * @param list new playlist
     * @param position (optional) time to seek to
     * @return 0 when it has success, different from 0 otherwise
     *
     * Creates the producer from the text playlist. */
    int setSceneList(QString playlist, int position = 0);
    int setProducer(Mlt::Producer *producer, int position);

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
    void switchPlay(bool play, bool slave = false);
    void pause();

    /** @brief Stops playing.
     * @param startTime time to seek to */
    void stop(const GenTime &startTime);
    int volume() const;

    QImage extractFrame(int frame_position, QString path = QString(), int width = -1, int height = -1);

    /** @brief Plays the scene starting from a specific time.
     * @param startTime time to start playing the scene from */
    void play(const GenTime & startTime);
    void playZone(const GenTime & startTime, const GenTime & stopTime);
    void loopZone(const GenTime & startTime, const GenTime & stopTime);

    void saveZone(KUrl url, QString desc, QPoint zone);
    
    /** @brief Save a clip in timeline to an xml playlist. */
    bool saveClip(int track, GenTime position, KUrl url, QString desc = QString());

    /** @brief Returns the speed at which the renderer is currently playing.
     *
     * It returns 0.0 when the renderer is not playing anything. */
    double playSpeed() const;

    /** @brief Returns the current seek position of the renderer. */
    GenTime seekPosition() const;
    int seekFramePosition() const;

    void emitFrameUpdated(Mlt::Frame&);
    void emitFrameNumber();
    void emitConsumerStopped();

    /** @brief Returns the aspect ratio of the consumer. */
    double consumerRatio() const;

    /** @brief Saves current producer frame as an image. */
    void exportCurrentFrame(KUrl url, bool notify);

    /** @brief Change the Mlt PROFILE
     * @param profileName The MLT profile name
     * @param dropSceneList If true, the current playlist will be deleted
     * @return true if the profile was changed
     * . */
    int resetProfile(const QString& profileName, bool dropSceneList = false);
    /** @brief Returns true if the render uses profileName as current profile. */
    bool hasProfile(const QString& profileName) const;
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
    Mlt::Producer *checkSlowMotionProducer(Mlt::Producer *prod, QDomElement element);
    int mltInsertClip(ItemInfo info, QDomElement element, Mlt::Producer *prod, bool overwrite = false, bool push = false);
    bool mltUpdateClip(Mlt::Tractor *tractor, ItemInfo info, QDomElement element, Mlt::Producer *prod);
    bool mltCutClip(int track, GenTime position);
    void mltInsertSpace(QMap <int, int> trackClipStartList, QMap <int, int> trackTransitionStartList, int track, const GenTime &duration, const GenTime &timeOffset);
    int mltGetSpaceLength(const GenTime &pos, int track, bool fromBlankStart);

    /** @brief Returns the duration/length of @param track as reported by the track producer. */
    int mltTrackDuration(int track);

    bool mltResizeClipEnd(ItemInfo info, GenTime clipDuration);
    bool mltResizeClipStart(ItemInfo info, GenTime diff);
    bool mltResizeClipCrop(ItemInfo info, GenTime newCropStart);
    bool mltMoveClip(int startTrack, int endTrack, GenTime pos, GenTime moveStart, Mlt::Producer *prod, bool overwrite = false, bool insert = false);
    bool mltMoveClip(int startTrack, int endTrack, int pos, int moveStart, Mlt::Producer *prod, bool overwrite = false, bool insert = false);
    bool mltRemoveClip(int track, GenTime position);

    /** @brief Deletes an effect from a clip in MLT's playlist. */
    bool mltRemoveEffect(int track, GenTime position, int index, bool updateIndex, bool doRefresh = true);
    bool mltRemoveTrackEffect(int track, int index, bool updateIndex);

    /** @brief Adds an effect to a clip in MLT's playlist. */
    bool mltAddEffect(int track, GenTime position, EffectsParameterList params, bool doRefresh = true);
    bool addFilterToService(Mlt::Service service, EffectsParameterList params, int duration);
    bool mltAddEffect(Mlt::Service service, EffectsParameterList params, int duration, bool doRefresh);
    bool mltAddTrackEffect(int track, EffectsParameterList params);
    
    /** @brief Enable / disable clip effects. 
     * @param track The track where the clip is
     * @param position The start position of the clip
     * @param effectIndexes The list of effect indexes to enable / disable
     * @param disable True if effects should be disabled, false otherwise */
    bool mltEnableEffects(int track, GenTime position, QList <int> effectIndexes, bool disable);
    /** @brief Enable / disable track effects.
     * @param track The track where the effect is
     * @param effectIndexes The list of effect indexes to enable / disable
     * @param disable True if effects should be disabled, false otherwise */
    bool mltEnableTrackEffects(int track, QList <int> effectIndexes, bool disable);

    /** @brief Edits an effect parameters in MLT's playlist. */
    bool mltEditEffect(int track, GenTime position, EffectsParameterList params);
    bool mltEditTrackEffect(int track, EffectsParameterList params);

    /** @brief Updates the "kdenlive_ix" (index) value of an effect. */
    void mltUpdateEffectPosition(int track, GenTime position, int oldPos, int newPos);

    /** @brief Changes the order of effects in MLT's playlist.
     *
     * It switches effects from oldPos and newPos, updating the "kdenlive_ix"
     * (index) value. */
    void mltMoveEffect(int track, GenTime position, int oldPos, int newPos);
    void mltMoveTrackEffect(int track, int oldPos, int newPos);

    /** @brief Enables/disables audio/video in a track. */
    void mltChangeTrackState(int track, bool mute, bool blind);
    bool mltMoveTransition(QString type, int startTrack,  int newTrack, int newTransitionTrack, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut);
    bool mltAddTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool refresh = true);
    void mltDeleteTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool refresh = true);
    void mltUpdateTransition(QString oldTag, QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool force = false);
    void mltUpdateTransitionParams(QString type, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml);
    void mltAddClipTransparency(ItemInfo info, int transitiontrack, int id);
    void mltMoveTransparency(int startTime, int endTime, int startTrack, int endTrack, int id);
    void mltDeleteTransparency(int pos, int track, int id);
    void mltResizeTransparency(int oldStart, int newStart, int newEnd, int track, int id);
    QList <TransitionInfo> mltInsertTrack(int ix, bool videoTrack);
    void mltDeleteTrack(int ix);
    bool mltUpdateClipProducer(Mlt::Tractor *tractor, int track, int pos, Mlt::Producer *prod);
    void mltPlantTransition(Mlt::Field *field, Mlt::Transition &tr, int a_track, int b_track);
    Mlt::Producer *invalidProducer(const QString &id);

    /** @brief Changes the speed of a clip in MLT's playlist.
     *
     * It creates a new "framebuffer" producer, which must have its "resource"
     * property set to "video.mpg?0.6", where "video.mpg" is the path to the
     * clip and "0.6" is the speed in percentage. The newly created producer
     * will have its "id" property set to "slowmotion:parentid:speed", where
     * "parentid" is the id of the original clip in the ClipManager list and
     * "speed" is the current speed. */
    int mltChangeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, double speed, double oldspeed, int strobe, Mlt::Producer *prod);

    const QList <Mlt::Producer *> producersList();
    void updatePreviewSettings();
    void setDropFrames(bool show);
    QString updateSceneListFps(double current_fps, double new_fps, QString scene);
    void showFrame(Mlt::Frame&);

    void showAudio(Mlt::Frame&);
    
    QList <int> checkTrackSequence(int);
    void sendFrameUpdate();

    /** @brief Returns a pointer to the main producer. */
    Mlt::Producer *getProducer();
    /** @brief Returns the number of clips to process (When requesting clip info). */
    int processingItems();
    /** @brief Force processing of clip with selected id. */
    void forceProcessing(const QString &id);
    /** @brief Are we currently processing clip with selected id. */
    bool isProcessing(const QString &id);

    /** @brief Requests the file properties for the specified URL (will be put in a queue list)
        @param xml The xml parameters for the clip
        @param clipId The clip Id string
        @param imageHeight The height (in pixels) of the returned thumbnail (height of a treewidgetitem in projectlist)
        @param replaceProducer If true, the MLT producer will be recreated */
    void getFileProperties(const QDomElement &xml, const QString &clipId, int imageHeight, bool replaceProducer = true);

    /** @brief Lock the MLT service */
    Mlt::Tractor *lockService();
    /** @brief Unlock the MLT service */
    void unlockService(Mlt::Tractor *tractor);
    const QString activeClipId();
    /** @brief Fill a combobox with the found blackmagic devices */
    static bool getBlackMagicDeviceList(KComboBox *devicelist);
    static bool getBlackMagicOutputDeviceList(KComboBox *devicelist);
    /** @brief Frame rendering is handeled by Kdenlive, don't show video through SDL display */
    void disablePreview(bool disable);
    int requestedSeekPosition;

private:

    /** @brief The name of this renderer.
     *
     * Useful to identify the renderers by what they do - e.g. background
     * rendering, workspace monitor, etc. */
    Kdenlive::MONITORID m_name;
    Mlt::FilteredConsumer * m_mltConsumer;
    Mlt::Producer * m_mltProducer;
    Mlt::Profile *m_mltProfile;
    Mlt::Event *m_showFrameEvent;
    Mlt::Event *m_pauseEvent;
    double m_fps;
    bool m_externalConsumer;

    /** @brief True if we are playing a zone.
     *
     * It's determined by the "in" and "out" properties being temporarily
     * changed. */
    bool m_isZoneMode;
    bool m_isLoopMode;
    GenTime m_loopStart;
    int m_originalOut;

    /** @brief True when the monitor is in split view. */
    bool m_isSplitView;

    Mlt::Producer *m_blackClip;
    QString m_activeProfile;

    QTimer *m_osdTimer;
    QTimer m_refreshTimer;
    QMutex m_mutex;
    QMutex m_infoMutex;

    /** @brief A human-readable description of this renderer. */
    int m_winid;

    QLocale m_locale;
    QFuture <void> m_infoThread;
    QList <requestClipInfo> m_requestList;

//#ifdef USE_JACK
    Slave::Type m_activeSlave;
    Device::Type m_activeDevice;
    unsigned int m_slavePerm;
//#endif

    void closeMlt();
    void mltCheckLength(Mlt::Tractor *tractor);
    void mltPasteEffects(Mlt::Producer *source, Mlt::Producer *dest);
    QMap<QString, QString> mltGetTransitionParamsFromXml(QDomElement xml);
    QMap<QString, Mlt::Producer *> m_slowmotionProducers;
    /** @brief The id of the clip that is currently being loaded for info query */
    QString m_processingClipId;

    /** @brief Build the MLT Consumer object with initial settings.
     *  @param profileName The MLT profile to use for the consumer */
    void buildConsumer(const QString& profileName);
    void resetZoneMode();
    void fillSlowMotionProducers();
    /** @brief Get the track number of the lowest audible (non muted) audio track
     *  @param return The track number */
    int getLowestNonMutedAudioTrack(Mlt::Tractor tractor);

    /** @brief Make sure our audio mixing transitions are applied to the lowest track */
    void fixAudioMixing(Mlt::Tractor tractor);
    /** @brief Make sure we inform MLT if we need a lot of threads for avformat producer */
    void checkMaxThreads();

private slots:

    /** @brief Refreshes the monitor display. */
    void refresh();
    void slotOsdTimeout();
    /** @brief Process the clip info requests (in a separate thread). */
    void processFileProperties();
    /** @brief A clip with multiple video streams was found, ask what to do. */
    void slotMultiStreamProducerFound(const QString path, QList<int> audio_list, QList<int> video_list, stringMap data);

    void slotCheckSeeking();

signals:

    /** @brief The renderer received a reply to a getFileProperties request. */
    void replyGetFileProperties(const QString &clipId, Mlt::Producer*, const stringMap &, const stringMap &, bool replaceProducer);

    /** @brief The renderer received a reply to a getImage request. */
    void replyGetImage(const QString &, const QString &, int, int);
    void replyGetImage(const QString &, const QImage &);

    /** @brief The renderer stopped, either playing or rendering. */
    void stopped();

    /** @brief The renderer started playing. */
    void playing(double);

    /** @brief The renderer started rendering. */
    void rendering(const GenTime &);

    /** @brief An error occurred within this renderer. */
    void error(const QString &, const QString &);
    void durationChanged(int);
    void rendererPosition(int);
    void rendererStopped(int);
    /** @brief The clip is not valid, should be removed from project. */
    void removeInvalidClip(const QString &, bool replaceProducer);
    /** @brief The proxy is not valid, should be deleted.
     *  @param id The original clip's id
     *  @param durationError Should be set to true if the proxy failed because it has not same length as original clip
     */
    void removeInvalidProxy(const QString &id, bool durationError);
    void refreshDocumentProducers(bool displayRatioChanged, bool fpsChanged);
    /** @brief A proxy clip is missing, ask for creation. */
    void requestProxy(QString);
    /** @brief A multiple stream clip was found. */
    void multiStreamFound(const QString &,QList<int>,QList<int>,stringMap data);


    /** @brief A frame's image has to be shown.
     *
     * Used in Mac OS X. */
    void showImageSignal(QImage);
    void showAudioSignal(const QByteArray &);
    void addClip(const KUrl &, stringMap);
    void checkSeeking();

public slots:

    /** @brief Starts the consumer. */
    void start();

    /** @brief Stops the consumer. */
    void stop();
    int getLength();

    /** @brief Checks if the file is readable by MLT. */
    bool isValid(KUrl url);

    void exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime);
    void mltSavePlaylist();
    void slotSplitView(bool doit);
    void slotSwitchFullscreen();
    void slotSetVolume(int volume);
    void seekToFrame(int pos);
    /** @brief Starts a timer to query for a refresh. */
    void doRefresh();

    /* TODO: @eddrog find solution - moc has a problem with #ifdef in slots */
    /** @brief */
    void slotOnSlavePlaybackStarted(int position);
    /** @brief */
    void slotOnSlavePlaybackSync(int position);
    /** @brief */
    void slotOnSlavePlaybackStopped(int position);
};

#endif
