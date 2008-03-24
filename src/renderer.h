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
#include <QWidget>

#include <kurl.h>

#include "gentime.h"
/*#include "docclipref.h"
#include "effectdesc.h"
#include "effectparamdescfactory.h"*/

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
    QString sceneList();
    void saveSceneList(QString path, QDomElement addedXml = QDomElement());

    /** Wraps the VEML command of the same name. Tells the renderer to
    play the current scene at the speed specified, relative to normal
    playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
    backwards. Does not specify start/stop times for playback.*/
    void play(double speed);
    void switchPlay();
    /** stop playing */
    void stop(const GenTime & startTime);
    void setVolume(double volume);

    QPixmap extractFrame(int frame_position, int width, int height);
    /** Wraps the VEML command of the same name. Tells the renderer to
    play the current scene at the speed specified, relative to normal
    playback. e.g. 1.0 is normal speed, 0.0 is paused, -1.0 means play
    backwards. Specifes the start/stop times for playback.*/
    void play(double speed, const GenTime & startTime);
    void play(double speed, const GenTime & startTime,
              const GenTime & stopTime);

    /** Returns the description of this renderer */
    QString description();

    /** Returns the name of the renderer. */
    const QString & rendererName() const;

    /** Returns the speed at which the renderer is currently playing, 0.0 if the renderer is
    not playing anything. */
    double playSpeed();
    /** Returns the current seek position of the renderer. */
    const GenTime & seekPosition() const;

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
    int resetProfile(QString profile);
    const double fps() const;

    /** Playlist manipulation */
    void mltInsertClip(int track, GenTime position, QDomElement element);
    void mltCutClip(int track, GenTime position);
    void mltResizeClipEnd(int track, GenTime pos, GenTime in, GenTime out);
    void mltResizeClipStart(int track, GenTime pos, GenTime moveEnd, GenTime moveStart, GenTime in, GenTime out);
    void mltMoveClip(int startTrack, int endTrack, GenTime pos, GenTime moveStart);
    void mltMoveClip(int startTrack, int endTrack, int pos, int moveStart);
    void mltRemoveClip(int track, GenTime position);
    void mltRemoveEffect(int track, GenTime position, QString index, bool doRefresh = true);
    void mltAddEffect(int track, GenTime position, QMap <QString, QString> args, bool doRefresh = true);
    void mltEditEffect(int track, GenTime position, QMap <QString, QString> args);
    void mltChangeTrackState(int track, bool mute, bool blind);
    void mltMoveTransition(QString type, int startTrack, int trackOffset, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut);
    void mltAddTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QMap <QString, QString> args, bool refresh = true);
    void mltDeleteTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QMap <QString, QString> args, bool refresh = true);
    void mltUpdateTransition(QString oldTag, QString tag, int a_track, int b_track, GenTime in, GenTime out, QMap <QString, QString> args);


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
    uint m_monitorId;
    bool m_generateScenelist;
    bool m_isBlocked;

    /** Holds the path to on screen display profile */
    QString m_osdProfile;

    QTimer *refreshTimer;
    QTimer *osdTimer;
    QTimer *m_connectTimer;
    KUrl m_exportedFile;
    int exportDuration, firstExportFrame, lastExportFrame;

    /** A human-readable description of this renderer. */
    QString m_description;
    int m_winid;
    int m_externalwinid;
    /** The actually seek command, private so people can't avoid the buffering of multiple seek commands. */
    void sendSeekCommand(GenTime time);

    /** Sets the description of this renderer to desc. */
    void setDescription(const QString & description);
    void closeMlt();
    void mltCheckLength();
    Mlt::Tractor* getTractor();
    Mlt::Playlist* getPlaylist(int track);
    void replaceTimelineTractor(Mlt::Tractor t);
private slots:  // Private slots
    /** refresh monitor display */
    void refresh();
    void slotOsdTimeout();
    void connectPlaylist();
    //void initSceneList();

signals:   // Signals
    /** emitted when the renderer recieves a reply to a getFileProperties request. */
    void replyGetFileProperties(int clipId, const QMap < QString, QString > &, const QMap < QString, QString > &);

    /** emitted when the renderer recieves a reply to a getImage request. */
    void replyGetImage(int , int, const QPixmap &, int, int);
    void replyGetImage(int, const QPixmap &, int, int);

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
    void getFileProperties(const QDomElement &xml, int clipId);

    void exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime);
    static char *decodedString(QString str);
    void mltSavePlaylist();
};

#endif
