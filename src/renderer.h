/***************************************************************************
                         krender.h  -  description
                            -------------------
   begin                : Fri Nov 22 2002
   copyright            : (C) 2002 by Jason Wood
   email                : jasonwood@blueyonder.co.uk
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KRENDER_H
#define KRENDER_H

#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <QList>

#include <kurl.h>

#include "gentime.h"
#include "definitions.h"

/**Render encapsulates the client side of the interface to a renderer.
From Kdenlive's point of view, you treat the Render object as the
renderer, and simply use it as if it was local. Calls are asyncrhonous -
you send a call out, and then recieve the return value through the
relevant signal that get's emitted once the call completes.
  *@author Jason Wood
  */

class Render;
//class EffectParamDesc;
class QPixmap;

namespace Mlt {
class Consumer;
class Playlist;
class Tractor;
class Frame;
class Producer;
class Filter;
class Profile;
class Multitrack;
};



class Render: public QObject {
Q_OBJECT public:

    enum FailStates { OK = 0,
                      APP_NOEXIST
                    };

    Render(const QString & rendererName, int winid, int extid, QWidget *parent = 0);
    ~Render();

    /** Seeks the renderer clip to the given time. */
    void seek(GenTime time);
    void seekToFrame(int pos);
    bool m_isBlocked;

    //static QPixmap getVideoThumbnail(char *profile, QString file, int frame, int width, int height);
    QPixmap getImageThumbnail(KUrl url, int width, int height);

    /** Return thumbnail for color clip */
    //void getImage(int id, QString color, QPoint size);

    // static QPixmap frameThumbnail(Mlt::Frame *frame, int width, int height, bool border = false);

    /** Return thumbnail for image clip */
    //void getImage(KUrl url, QPoint size);

    /** Requests a particular frame from the given file.
     *
     * The pixmap will be returned by emitting the replyGetImage() signal.
     * */
    //void getImage(KUrl url, int frame, QPoint size);


    /** Wraps the VEML command of the same name. Sets the current scene list to
    be list. */
    void setSceneList(QDomDocument list, int position = 0);
    void setSceneList(QString playlist, int position = 0);
    void setProducer(Mlt::Producer *producer, int position);
    QString sceneList();
    void saveSceneList(QString path, QDomElement kdenliveData = QDomElement());

    /** Wraps the VEML command of the same name. Tells the renderer to
    play the current scene at the speed specified, relative to normal
    playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
    backwards. Does not specify start/stop times for playback.*/
    void play(double speed);
    void switchPlay();
    void pause();
    /** stop playing */
    void stop(const GenTime & startTime);
    void setVolume(double volume);

    QPixmap extractFrame(int frame_position, int width = -1, int height = -1);
    /** Wraps the VEML command of the same name. Tells the renderer to
    play the current scene at the speed specified, relative to normal
    playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
    backwards. Specifes the start/stop times for playback.*/
    void play(const GenTime & startTime);
    void playZone(const GenTime & startTime, const GenTime & stopTime);
    void loopZone(const GenTime & startTime, const GenTime & stopTime);

    void saveZone(KUrl url, QString desc, QPoint zone);

    /** Returns the name of the renderer. */
    const QString & rendererName() const;

    /** Returns the speed at which the renderer is currently playing, 0.0 if the renderer is
    not playing anything. */
    double playSpeed();
    /** Returns the current seek position of the renderer. */
    GenTime seekPosition() const;

    void emitFrameNumber(double position);
    void emitConsumerStopped();

    /** Gives the aspect ratio of the consumer */
    double consumerRatio() const;

    void askForRefresh();
    void doRefresh();

    /** Save current producer frame as image */
    void exportCurrentFrame(KUrl url, bool notify);

    /** Turn on or off on screen display */
    void refreshDisplay();
    int resetProfile();
    const double fps() const;
    const int renderWidth() const;
    const int renderHeight() const;
    /** get display aspect ratio */
    const double dar() const;

    /** Playlist manipulation */
    void mltInsertClip(ItemInfo info, QDomElement element, Mlt::Producer *prod);
    void mltUpdateClip(ItemInfo info, QDomElement element, Mlt::Producer *prod);
    void mltCutClip(int track, GenTime position);
    void mltInsertSpace(QMap <int, int> trackClipStartList, QMap <int, int> trackTransitionStartList, int track, const GenTime duration, const GenTime timeOffset);
    int mltGetSpaceLength(const GenTime pos, int track, bool fromBlankStart);
    bool mltResizeClipEnd(ItemInfo info, GenTime clipDuration);
    bool mltResizeClipStart(ItemInfo info, GenTime diff);
    bool mltMoveClip(int startTrack, int endTrack, GenTime pos, GenTime moveStart, Mlt::Producer *prod);
    bool mltMoveClip(int startTrack, int endTrack, int pos, int moveStart, Mlt::Producer *prod);
    bool mltRemoveClip(int track, GenTime position);
    bool mltRemoveEffect(int track, GenTime position, QString index, bool updateIndex, bool doRefresh = true);
    bool mltAddEffect(int track, GenTime position, EffectsParameterList params, bool doRefresh = true);
    bool mltEditEffect(int track, GenTime position, EffectsParameterList params);
    void mltMoveEffect(int track, GenTime position, int oldPos, int newPos);
    void mltChangeTrackState(int track, bool mute, bool blind);
    bool mltMoveTransition(QString type, int startTrack,  int newTrack, int newTransitionTrack, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut);
    bool mltAddTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool refresh = true);
    void mltDeleteTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml, bool refresh = true);
    void mltUpdateTransition(QString oldTag, QString tag, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml);
    void mltUpdateTransitionParams(QString type, int a_track, int b_track, GenTime in, GenTime out, QDomElement xml);
    void mltAddClipTransparency(ItemInfo info, int transitiontrack, int id);
    void mltMoveTransparency(int startTime, int endTime, int startTrack, int endTrack, int id);
    void mltDeleteTransparency(int pos, int track, int id);
    void mltResizeTransparency(int oldStart, int newStart, int newEnd, int track, int id);
    void mltInsertTrack(int ix, bool videoTrack);
    void mltDeleteTrack(int ix);
    void mltUpdateClipProducer(int track, int pos, Mlt::Producer *prod);

    /** Change speed of a clip in playlist. To do this, we create a new "framebuffer" producer.
    This new producer must have its "resource" param set to: video.mpg?0.6 where video.mpg is the path
    to the clip and 0.6 is the speed in percents. The newly created producer will have it's
    "id" parameter set to: "slowmotion:parentid:speed", where parentid is the id of the original clip
    in the ClipManager list and speed is the current speed */
    int mltChangeClipSpeed(ItemInfo info, double speed, double oldspeed, Mlt::Producer *prod);

    QList <Mlt::Producer *> producersList();
    void updatePreviewSettings();

private:   // Private attributes & methods
    /** The name of this renderer - useful to identify the renderes by what they do - e.g. background rendering, workspace monitor, etc... */
    QString m_name;
    Mlt::Consumer * m_mltConsumer;
    Mlt::Producer * m_mltProducer;
    Mlt::Producer *m_mltTextProducer;
    Mlt::Filter *m_osdInfo;
    Mlt::Profile *m_mltProfile;
    double m_framePosition;
    double m_fps;

    /** true if we are playing a zone (ie the in and out properties have been temporarily changed) */
    bool m_isZoneMode;
    bool m_isLoopMode;
    GenTime m_loopStart;
    int m_originalOut;

    /** true when monitor is in split view (several tracks at the same time) */
    bool m_isSplitView;

    Mlt::Producer *m_blackClip;
    /** Holds the path to on screen display profile */
    QString m_osdProfile;
    QString m_activeProfile;

    QTimer *refreshTimer;
    QTimer *osdTimer;
    KUrl m_exportedFile;

    /** A human-readable description of this renderer. */
    int m_winid;
    int m_externalwinid;

    /** Sets the description of this renderer to desc. */
    void closeMlt();
    void mltCheckLength();
    QMap<QString, QString> mltGetTransitionParamsFromXml(QDomElement xml);
    QMap<QString, Mlt::Producer *> m_slowmotionProducers;
    void buildConsumer();
    void resetZoneMode();
    void fillSlowMotionProducers();

private slots:  // Private slots
    /** refresh monitor display */
    void refresh();
    void slotOsdTimeout();
    void connectPlaylist();
    //void initSceneList();

signals:   // Signals
    /** emitted when the renderer recieves a reply to a getFileProperties request. */
    void replyGetFileProperties(const QString &clipId, Mlt::Producer*, const QMap < QString, QString > &, const QMap < QString, QString > &);

    /** emitted when the renderer recieves a reply to a getImage request. */
    void replyGetImage(const QString &, const QPixmap &);

    /** Emitted when the renderer stops, either playing or rendering. */
    void stopped();
    /** Emitted when the renderer starts playing. */
    void playing(double);
    /** Emitted when the renderer is rendering. */
    void rendering(const GenTime &);
    /** Emitted when rendering has finished */
    void renderFinished();
    /** Emitted when the current seek position has been changed by the renderer. */
//    void positionChanged(const GenTime &);
    /** Emitted when an error occurs within this renderer. */
    void error(const QString &, const QString &);
    void durationChanged(int);
    void rendererPosition(int);
    void rendererStopped(int);
    void removeInvalidClip(const QString &);

public slots:  // Public slots
    /** Start Consumer */
    void start();
    /** Stop Consumer */
    void stop();
    void clear();
    int getLength();
    /** If the file is readable by mlt, return true, otherwise false */
    bool isValid(KUrl url);

    /** Wraps the VEML command of the same name. Requests the file properties
    for the specified url from the renderer. Upon return, the result will be emitted
    via replyGetFileProperties(). */
    void getFileProperties(const QDomElement &xml, const QString &clipId);

    void exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime);
    static char *decodedString(QString str);
    void mltSavePlaylist();
    void slotSplitView(bool doit);
};

#endif
