/***************************************************************************
                         mltdevicecapture.h  -  description
                            -------------------
   begin                : Sun May 21 2011
   copyright            : (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)

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
 * @class MltDeviceCapture
 * @brief Interface for MLT capture.
 */

#ifndef MLTDEVICECAPTURE_H
#define MLTDEVICECAPTURE_H

#include "gentime.h"
#include "definitions.h"
#include "abstractmonitor.h"
#include "mlt/framework/mlt_types.h"

namespace Mlt
{
class Consumer;
class Playlist;
class Tractor;
class Transition;
class Frame;
class Field;
class Producer;
class Filter;
class Profile;
class Service;
};

class MltDeviceCapture: public AbstractRender
{
Q_OBJECT public:

    enum FailStates { OK = 0,
                      APP_NOEXIST
                    };
    /** @brief Build a MLT Renderer
     *  @param winid The parent widget identifier (required for SDL display). Set to 0 for OpenGL rendering
     *  @param profile The MLT profile used for the capture (default one will be used if empty). */
    MltDeviceCapture(QString profile, VideoPreviewContainer *surface, QWidget *parent = 0);

    /** @brief Destroy the MLT Renderer. */
    ~MltDeviceCapture();

    int doCapture;

    /** @brief This property is used to decide if the renderer should convert it's frames to QImage for use in other Kdenlive widgets. */
    bool sendFrameForAnalysis;

    /** @brief Someone needs us to send again a frame. */
    void sendFrameUpdate() {};
    
    void emitFrameUpdated(Mlt::Frame&);
    void emitFrameNumber(double position);
    void emitConsumerStopped();
    void showFrame(Mlt::Frame&);
    void showAudio(Mlt::Frame&);

    void saveFrame(Mlt::Frame& frame);

    /** @brief Starts the MLT Video4Linux process.
     * @param surface The widget onto which the frame should be painted
     */
    bool slotStartCapture(const QString &params, const QString &path, const QString &playlist, bool livePreview, bool xmlPlaylist = true);
    bool slotStartPreview(const QString &producer, bool xmlFormat = false);
    /** @brief A frame arrived from the MLT Video4Linux process. */
    void gotCapturedFrame(Mlt::Frame& frame);
    /** @brief Save current frame to file. */
    void captureFrame(const QString &path);
    void doRefresh();
    /** @brief This will add the video clip from path and add it in the overlay track. */
    void setOverlay(const QString &path);

    /** @brief This will add an MLT video effect to the overlay track. */
    void setOverlayEffect(const QString tag, QStringList parameters);

    /** @brief This will add a horizontal flip effect, easier to work when filming yourself. */
    void mirror(bool activate);

private:
    Mlt::Consumer * m_mltConsumer;
    Mlt::Producer * m_mltProducer;
    Mlt::Profile *m_mltProfile;
    QString m_activeProfile;
    int m_droppedFrames;
    bool m_livePreview;

    /** @brief The surface onto which the captured frames should be painted. */
    VideoPreviewContainer *m_captureDisplayWidget;

    /** @brief A human-readable description of this renderer. */
    int m_winid;

    /** @brief This property is used to decide if the renderer should send audio data for monitoring. */
    bool m_analyseAudio;

    QString m_capturePath;

    /** @brief Build the MLT Consumer object with initial settings.
     *  @param profileName The MLT profile to use for the consumer */
    void buildConsumer(const QString &profileName = QString());

private slots:

signals:
    /** @brief A frame's image has to be shown.
     *
     * Used in Mac OS X. */
    void showImageSignal(QImage);
    
    /** @brief This signal contains the audio of the current frame. */
    void audioSamplesSignal(const QVector<int16_t>&, int freq, int num_channels, int num_samples);

    void frameSaved(const QString);
    
    void droppedFrames(int);

public slots:

    /** @brief Stops the consumer. */
    void stop();
};

#endif
