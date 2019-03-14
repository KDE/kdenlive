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

/*!
 * @class MltDeviceCapture
 * @brief Interface for MLT capture.
 * Capturing started by   MltDeviceCapture::slotStartCapture ()
 *
 * Capturing is stopped  by  RecMonitor::slotStopCapture()
 */

#ifndef MLTDEVICECAPTURE_H
#define MLTDEVICECAPTURE_H

#include "definitions.h"
#include "gentime.h"
#include "monitor/abstractmonitor.h"

#include <QMutex>
#include <QTimer>

// include after QTimer to have C++ phtreads defined
#include <mlt/framework/mlt_types.h>

namespace Mlt {
class Consumer;
class Frame;
class Event;
class Producer;
class Profile;
} // namespace Mlt

class MltDeviceCapture : public AbstractRender
{
    Q_OBJECT public :

        enum FailStates {
            OK = 0,
            APP_NOEXIST
        };
    /** @brief Build a MLT Renderer
     *  @param winid The parent widget identifier (required for SDL display). Set to 0 for OpenGL rendering
     *  @param profile The MLT profile used for the capture (default one will be used if empty). */
    explicit MltDeviceCapture(const QString &profile, /*VideoSurface *surface,*/ QWidget *parent = nullptr);

    /** @brief Destroy the MLT Renderer. */
    ~MltDeviceCapture() override;

    int doCapture;

    /** @brief Someone needs us to send again a frame. */
    void sendFrameUpdate() override {}

    void emitFrameUpdated(Mlt::Frame & /*frame*/);
    void emitFrameNumber(double position);
    void emitConsumerStopped();
    void showFrame(Mlt::Frame & /*frame*/);
    void showAudio(Mlt::Frame & /*frame*/);

    void saveFrame(Mlt::Frame &frame);

    /** @brief Starts the MLT Video4Linux process.
     * @param surface The widget onto which the frame should be painted
     * Called by  RecMonitor::slotRecord ()
     */
    bool slotStartCapture(const QString &params, const QString &path, const QString &playlist, bool livePreview, bool xmlPlaylist = true);
    bool slotStartPreview(const QString &producer, bool xmlFormat = false);
    /** @brief Save current frame to file. */
    void captureFrame(const QString &path);

    /** @brief This will add the video clip from path and add it in the overlay track. */
    void setOverlay(const QString &path);

    /** @brief This will add an MLT video effect to the overlay track. */
    void setOverlayEffect(const QString &tag, const QStringList &parameters);

    /** @brief This will add a horizontal flip effect, easier to work when filming yourself. */
    void mirror(bool activate);

    /** @brief True if we are processing an image (yuv > rgb) when recording. */
    bool processingImage;

    void pause();

private:
    Mlt::Consumer *m_mltConsumer;
    Mlt::Producer *m_mltProducer;
    Mlt::Profile *m_mltProfile;
    Mlt::Event *m_showFrameEvent;
    QString m_activeProfile;
    int m_droppedFrames;
    /** @brief When true, images will be displayed on monitor while capturing. */
    bool m_livePreview;
    /** @brief Count captured frames, used to display only one in ten images while capturing. */
    int m_frameCount{};

    void uyvy2rgb(const unsigned char *yuv_buffer, int width, int height);

    QString m_capturePath;

    QTimer m_droppedFramesTimer;

    QMutex m_mutex;

    /** @brief Build the MLT Consumer object with initial settings.
     *  @param profileName The MLT profile to use for the consumer
     *  @returns true if consumer is valid */
    bool buildConsumer(const QString &profileName = QString());

private slots:
    void slotPreparePreview();
    void slotAllowPreview();
    /** @brief When capturing, check every second for dropped frames. */
    void slotCheckDroppedFrames();

signals:
    /** @brief A frame's image has to be shown.
     *
     * Used in Mac OS X. */
    void showImageSignal(const QImage &);

    void frameSaved(const QString &);

    void droppedFrames(int);

    void unblockPreview();
    void imageReady(const QImage &);

public slots:
    /** @brief Stops the consumer. */
    void stop();
};

#endif
