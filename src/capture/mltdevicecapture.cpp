/*
   SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mltdevicecapture.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>

#include "kdenlive_debug.h"

#include <QString>
#include <QThread>

#include <cstdarg>
#include <cstdlib>

static void consumer_gl_frame_show(mlt_consumer /*unused*/, MltDeviceCapture *self, mlt_frame frame_ptr)
{
    // detect if the producer has finished playing. Is there a better way to do it?
    Mlt::Frame frame(frame_ptr);
    self->showFrame(frame);
}

MltDeviceCapture::MltDeviceCapture(const QString &profile, /*VideoSurface *surface, */ QWidget *parent)
    : AbstractRender(Kdenlive::RecordMonitor, parent)
    , doCapture(0)
    , processingImage(false)
    , m_mltConsumer(nullptr)
    , m_mltProducer(nullptr)
    , m_mltProfile(nullptr)
    , m_showFrameEvent(nullptr)
    , m_droppedFrames(0)
    , m_livePreview(KdenliveSettings::enable_recording_preview())
{
    analyseAudio = KdenliveSettings::monitor_audio();
    if (profile.isEmpty()) {
        // profile = KdenliveSettings::current_profile();
    }
    buildConsumer(profile);
    connect(this, &MltDeviceCapture::unblockPreview, this, &MltDeviceCapture::slotPreparePreview);
    m_droppedFramesTimer.setSingleShot(false);
    m_droppedFramesTimer.setInterval(1000);
    connect(&m_droppedFramesTimer, &QTimer::timeout, this, &MltDeviceCapture::slotCheckDroppedFrames);
}

MltDeviceCapture::~MltDeviceCapture()
{
    delete m_mltConsumer;
    delete m_mltProducer;
    delete m_mltProfile;
}

bool MltDeviceCapture::buildConsumer(const QString &profileName)
{
    if (!profileName.isEmpty()) {
        m_activeProfile = profileName;
    }

    delete m_mltProfile;

    char *tmp = qstrdup(m_activeProfile.toUtf8().constData());
    qputenv("MLT_PROFILE", tmp);
    m_mltProfile = new Mlt::Profile(tmp);
    m_mltProfile->set_explicit(1);
    delete[] tmp;

    QString videoDriver = KdenliveSettings::videodrivername();
    if (!videoDriver.isEmpty()) {
        if (videoDriver == QLatin1String("x11_noaccel")) {
            qputenv("SDL_VIDEO_YUV_HWACCEL", "0");
            videoDriver = QStringLiteral("x11");
        } else {
            qunsetenv("SDL_VIDEO_YUV_HWACCEL");
        }
    }
    qputenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1");
    // OpenGL monitor
    m_mltConsumer = new Mlt::Consumer(*m_mltProfile, KdenliveSettings::audiobackend().toUtf8().constData());
    m_mltConsumer->set("preview_off", 1);
    m_mltConsumer->set("preview_format", mlt_image_rgb);
    m_showFrameEvent = m_mltConsumer->listen("consumer-frame-show", this, mlt_listener(consumer_gl_frame_show));
    // m_mltConsumer->set("resize", 1);
    // m_mltConsumer->set("terminate_on_pause", 1);
    m_mltConsumer->set("window_background", KdenliveSettings::window_background().name().toUtf8().constData());
    // m_mltConsumer->set("rescale", "nearest");

    QString audioDevice = KdenliveSettings::audiodevicename();
    if (!audioDevice.isEmpty()) {
        m_mltConsumer->set("audio_device", audioDevice.toUtf8().constData());
    }

    if (!videoDriver.isEmpty()) {
        m_mltConsumer->set("video_driver", videoDriver.toUtf8().constData());
    }

    QString audioDriver = KdenliveSettings::audiodrivername();

    if (!audioDriver.isEmpty()) {
        m_mltConsumer->set("audio_driver", audioDriver.toUtf8().constData());
    }

    // m_mltConsumer->set("progressive", 0);
    // m_mltConsumer->set("buffer", 1);
    // m_mltConsumer->set("real_time", 0);
    if (!m_mltConsumer->is_valid()) {
        delete m_mltConsumer;
        m_mltConsumer = nullptr;
        return false;
    }
    return true;
}

void MltDeviceCapture::pause()
{
    if (m_mltConsumer) {
        m_mltConsumer->set("refresh", 0);
        // m_mltProducer->set_speed(0.0);
        m_mltConsumer->purge();
    }
}

void MltDeviceCapture::stop()
{
    m_droppedFramesTimer.stop();
    bool isPlaylist = false;
    // disconnect(this, SIGNAL(imageReady(QImage)), this, SIGNAL(frameUpdated(QImage)));
    // m_captureDisplayWidget->stop();

    delete m_showFrameEvent;
    m_showFrameEvent = nullptr;

    if (m_mltConsumer) {
        m_mltConsumer->set("refresh", 0);
        m_mltConsumer->purge();
        m_mltConsumer->stop();
        // if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    }
    if (m_mltProducer) {
        Mlt::Service service(m_mltProducer->parent().get_service());
        mlt_service_lock(service.get_service());
        if (service.type() == mlt_service_tractor_type) {
            isPlaylist = true;
            Mlt::Tractor tractor(service);
            mlt_tractor_close(tractor.get_tractor());
            Mlt::Field *field = tractor.field();
            mlt_service nextservice = mlt_service_get_producer(service.get_service());
            mlt_service nextservicetodisconnect;
            mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
            QString mlt_type = mlt_properties_get(properties, "mlt_type");
            QString resource = mlt_properties_get(properties, "mlt_service");
            // Delete all transitions
            while (mlt_type == QLatin1String("transition")) {
                nextservicetodisconnect = nextservice;
                nextservice = mlt_service_producer(nextservice);
                mlt_field_disconnect_service(field->get_field(), nextservicetodisconnect);
                if (nextservice == nullptr) {
                    break;
                }
                properties = MLT_SERVICE_PROPERTIES(nextservice);
                mlt_type = mlt_properties_get(properties, "mlt_type");
                resource = mlt_properties_get(properties, "mlt_service");
            }
            delete field;
            field = nullptr;
        }
        mlt_service_unlock(service.get_service());
        delete m_mltProducer;
        m_mltProducer = nullptr;
    }
    // For some reason, the consumer seems to be deleted by previous stuff when in playlist mode
    if (!isPlaylist && (m_mltConsumer != nullptr)) {
        delete m_mltConsumer;
    }
    m_mltConsumer = nullptr;
}

void MltDeviceCapture::emitFrameUpdated(Mlt::Frame &frame)
{
    /*
    //TEST: is it better to convert the frame in a thread outside of MLT??
    if (processingImage) return;
    mlt_image_format format = (mlt_image_format) frame.get_int("format"); //mlt_image_rgb;
    int width = frame.get_int("width");
    int height = frame.get_int("height");
    unsigned char *buffer = (unsigned char *) frame.get_data("image");
    if (format == mlt_image_yuv422) {
        QtConcurrent::run(this, &MltDeviceCapture::uyvy2rgb, (unsigned char *) buffer, width, height);
    }
    */

    mlt_image_format format = mlt_image_rgb;
    int width = 0;
    int height = 0;
    const uchar *image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_RGB888);
    // QImage qimage(width, height, QImage::Format_ARGB32_Premultiplied);
    memcpy(qimage.bits(), image, size_t(width * height * 3));
    Q_EMIT frameUpdated(qimage);
}

void MltDeviceCapture::showFrame(Mlt::Frame &frame)
{
    mlt_image_format format = mlt_image_rgb;
    int width = 0;
    int height = 0;
    const uchar *image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_RGB888);
    memcpy(qimage.scanLine(0), image, static_cast<size_t>(width * height * 3));
    Q_EMIT showImageSignal(qimage);

    if (sendFrameForAnalysis && (frame.get_frame()->convert_image != nullptr)) {
        Q_EMIT frameUpdated(qimage.rgbSwapped());
    }
}

void MltDeviceCapture::showAudio(Mlt::Frame &frame)
{
    if (!frame.is_valid() || frame.get_int("test_audio") != 0) {
        return;
    }
    mlt_audio_format audio_format = mlt_audio_s16;
    int freq = 0;
    int num_channels = 0;
    int samples = 0;
    auto *data = static_cast<qint16 *>(frame.get_audio(audio_format, freq, num_channels, samples));

    if (!data) {
        return;
    }

    // Data format: [ c00 c10 c01 c11 c02 c12 c03 c13 ... c0{samples-1} c1{samples-1} for 2 channels.
    // So the vector is of size samples*channels.
    audioShortVector sampleVector(samples * num_channels);
    memcpy(sampleVector.data(), data, size_t(samples * num_channels) * sizeof(qint16));
    if (samples > 0) {
        Q_EMIT audioSamplesSignal(sampleVector, freq, num_channels, samples);
    }
}

bool MltDeviceCapture::slotStartPreview(const QString &producer, bool xmlFormat)
{
    if (m_mltConsumer == nullptr) {
        if (!buildConsumer()) {
            return false;
        }
    }
    char *tmp = qstrdup(producer.toUtf8().constData());

    QMutexLocker lock(&pCore->xmlMutex);
    if (xmlFormat) {
        m_mltProducer = new Mlt::Producer(*m_mltProfile, "xml-string", tmp);
    } else {
        m_mltProducer = new Mlt::Producer(*m_mltProfile, tmp);
    }
    lock.unlock();
    delete[] tmp;

    if (m_mltProducer == nullptr || !m_mltProducer->is_valid()) {
        if (m_mltProducer) {
            delete m_mltProducer;
            m_mltProducer = nullptr;
        }
        // qCDebug(KDENLIVE_LOG)<<"//// ERROR CREATRING PROD";
        return false;
    }
    m_mltConsumer->connect(*m_mltProducer);
    if (m_mltConsumer->start() == -1) {
        delete m_mltConsumer;
        m_mltConsumer = nullptr;
        return false;
    }
    m_droppedFramesTimer.start();
    // connect(this, SIGNAL(imageReady(QImage)), this, SIGNAL(frameUpdated(QImage)));
    return true;
}

void MltDeviceCapture::slotCheckDroppedFrames()
{
    if (m_mltProducer) {
        int dropped = m_mltProducer->get_int("dropped");
        if (dropped > m_droppedFrames) {
            m_droppedFrames = dropped;
            Q_EMIT droppedFrames(m_droppedFrames);
        }
    }
}

void MltDeviceCapture::saveFrame(Mlt::Frame &frame)
{
    mlt_image_format format = mlt_image_rgb;
    int width = 0;
    int height = 0;
    const uchar *image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_RGB888);
    memcpy(qimage.bits(), image, static_cast<size_t>(width * height * 3));

    // Re-enable overlay
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(0));
    trackProducer.set("hide", 0);

    qimage.save(m_capturePath);
    Q_EMIT frameSaved(m_capturePath);
    m_capturePath.clear();
}

void MltDeviceCapture::captureFrame(const QString &path)
{
    if (m_mltProducer == nullptr || !m_mltProducer->is_valid()) {
        return;
    }

    // Hide overlay track before doing the capture
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(0));
    mlt_service_lock(service.get_service());
    trackProducer.set("hide", 1);
    m_mltConsumer->purge();
    mlt_service_unlock(service.get_service());
    m_capturePath = path;
    // Wait for 5 frames before capture to make sure overlay is gone
    doCapture = 5;
}

bool MltDeviceCapture::slotStartCapture(const QString &params, const QString &path, const QString &playlist, bool livePreview, bool xmlPlaylist)
{
    stop();
    m_livePreview = livePreview;
    m_frameCount = 0;
    m_droppedFrames = 0;
    delete m_mltProfile;
    char *tmp = qstrdup(m_activeProfile.toUtf8().constData());
    m_mltProfile = new Mlt::Profile(tmp);
    delete[] tmp;

    m_mltConsumer = new Mlt::Consumer(*m_mltProfile, "multi");
    if (m_mltConsumer == nullptr || !m_mltConsumer->is_valid()) {
        delete m_mltConsumer;
        m_mltConsumer = nullptr;
        return false;
    }

    // Create multi consumer setup
    auto *renderProps = new Mlt::Properties;
    renderProps->set("mlt_service", "avformat");
    renderProps->set("target", path.toUtf8().constData());
    renderProps->set("real_time", -1);
    renderProps->set("terminate_on_pause", 0); // was commented out. restoring it  fixes mantis#3415 - FFmpeg recording freezes
    // without this line a call to mlt_properties_get_int(terminate on pause) for in mlt/src/modules/core/consumer_multi.c is returning 1
    // and going into and endless loop.
    renderProps->set("mlt_profile", m_activeProfile.toUtf8().constData());
    const QStringList paramList = params.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < paramList.count(); ++i) {
        tmp = qstrdup(paramList.at(i).section(QLatin1Char('='), 0, 0).toUtf8().constData());
        QString value = paramList.at(i).section(QLatin1Char('='), 1, 1);
        if (value == QLatin1String("%threads")) {
            value = QString::number(QThread::idealThreadCount());
        }
        char *tmp2 = qstrdup(value.toUtf8().constData());
        renderProps->set(tmp, tmp2);
        delete[] tmp;
        delete[] tmp2;
    }
    mlt_properties consumerProperties = m_mltConsumer->get_properties();
    mlt_properties_set_data(consumerProperties, "0", renderProps->get_properties(), 0, mlt_destructor(mlt_properties_close), nullptr);

    if (m_livePreview) {
        // user wants live preview
        auto *previewProps = new Mlt::Properties;
        QString videoDriver = KdenliveSettings::videodrivername();
        if (!videoDriver.isEmpty()) {
            if (videoDriver == QLatin1String("x11_noaccel")) {
                qputenv("SDL_VIDEO_YUV_HWACCEL", "0");
                videoDriver = QStringLiteral("x11");
            } else {
                qunsetenv("SDL_VIDEO_YUV_HWACCEL");
            }
        }
        qputenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1");

        // OpenGL monitor
        previewProps->set("mlt_service", KdenliveSettings::audiobackend().toUtf8().constData());
        previewProps->set("preview_off", 1);
        previewProps->set("preview_format", mlt_image_rgb);
        previewProps->set("terminate_on_pause", 0);
        m_showFrameEvent = m_mltConsumer->listen("consumer-frame-show", this, mlt_listener(consumer_gl_frame_show));
        // m_mltConsumer->set("resize", 1);
        previewProps->set("window_background", KdenliveSettings::window_background().name().toUtf8().constData());
        QString audioDevice = KdenliveSettings::audiodevicename();
        if (!audioDevice.isEmpty()) {
            previewProps->set("audio_device", audioDevice.toUtf8().constData());
        }

        if (!videoDriver.isEmpty()) {
            previewProps->set("video_driver", videoDriver.toUtf8().constData());
        }

        QString audioDriver = KdenliveSettings::audiodrivername();

        if (!audioDriver.isEmpty()) {
            previewProps->set("audio_driver", audioDriver.toUtf8().constData());
        }

        previewProps->set("real_time", "0");
        previewProps->set("mlt_profile", m_activeProfile.toUtf8().constData());
        mlt_properties_set_data(consumerProperties, "1", previewProps->get_properties(), 0, mlt_destructor(mlt_properties_close), nullptr);
        // m_showFrameEvent = m_mltConsumer->listen("consumer-frame-render", this, (mlt_listener) rec_consumer_frame_show);
    } else {
    }

    QMutexLocker lock(&pCore->xmlMutex);
    if (xmlPlaylist) {
        // create an xml producer
        m_mltProducer = new Mlt::Producer(*m_mltProfile, "xml-string", playlist.toUtf8().constData());
    } else {
        // create a producer based on mltproducer parameter
        m_mltProducer = new Mlt::Producer(*m_mltProfile, playlist.toUtf8().constData());
    }
    lock.unlock();

    if (m_mltProducer == nullptr || !m_mltProducer->is_valid()) {
        // qCDebug(KDENLIVE_LOG)<<"//// ERROR CREATRING PROD";
        delete m_mltConsumer;
        m_mltConsumer = nullptr;
        delete m_mltProducer;
        m_mltProducer = nullptr;
        return false;
    }

    m_mltConsumer->connect(*m_mltProducer);
    if (m_mltConsumer->start() == -1) {
        delete m_showFrameEvent;
        m_showFrameEvent = nullptr;
        delete m_mltConsumer;
        m_mltConsumer = nullptr;
        return false;
    }
    m_droppedFramesTimer.start();
    return true;
}

void MltDeviceCapture::setOverlay(const QString &path)
{
    if (m_mltProducer == nullptr || !m_mltProducer->is_valid()) {
        return;
    }
    Mlt::Producer parentProd(m_mltProducer->parent());
    if (parentProd.get_producer() == nullptr) {
        // qCDebug(KDENLIVE_LOG) << "PLAYLIST BROKEN, CANNOT INSERT CLIP //////";
        return;
    }

    Mlt::Service service(parentProd.get_service());
    if (service.type() != mlt_service_tractor_type) {
        qCWarning(KDENLIVE_LOG) << "// TRACTOR PROBLEM";
        return;
    }
    Mlt::Tractor tractor(service);
    if (tractor.count() < 2) {
        qCWarning(KDENLIVE_LOG) << "// TRACTOR PROBLEM";
        return;
    }
    mlt_service_lock(service.get_service());
    Mlt::Producer trackProducer(tractor.track(0));
    Mlt::Playlist trackPlaylist(mlt_playlist(trackProducer.get_service()));

    trackPlaylist.remove(0);
    if (path.isEmpty()) {
        mlt_service_unlock(service.get_service());
        return;
    }

    // Add overlay clip
    char *tmp = qstrdup(path.toUtf8().constData());
    auto *clip = new Mlt::Producer(*m_mltProfile, "loader", tmp);
    delete[] tmp;
    clip->set_in_and_out(0, 99999);
    trackPlaylist.insert_at(0, clip, 1);

    // Add transition
    mlt_service serv = m_mltProducer->parent().get_service();
    mlt_service nextservice = mlt_service_get_producer(serv);
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString mlt_type = mlt_properties_get(properties, "mlt_type");
    if (mlt_type != QLatin1String("transition")) {
        // transition does not exist, add it
        Mlt::Field *field = tractor.field();
        auto *transition = new Mlt::Transition(*m_mltProfile, "composite");
        transition->set_in_and_out(0, 0);
        transition->set("geometry", "0/0:100%x100%:70");
        transition->set("fill", 1);
        transition->set("operator", "and");
        transition->set("a_track", 0);
        transition->set("b_track", 1);
        field->plant_transition(*transition, 0, 1);
    }
    mlt_service_unlock(service.get_service());
    // delete clip;
}

void MltDeviceCapture::setOverlayEffect(const QString &tag, const QStringList &parameters)
{
    if (m_mltProducer == nullptr || !m_mltProducer->is_valid()) {
        return;
    }
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(0));
    Mlt::Playlist trackPlaylist(mlt_playlist(trackProducer.get_service()));
    Mlt::Service trackService(trackProducer.get_service());

    mlt_service_lock(service.get_service());

    // delete previous effects
    Mlt::Filter *filter;
    filter = trackService.filter(0);
    if ((filter != nullptr) && !tag.isEmpty()) {
        QString currentService = filter->get("mlt_service");
        if (currentService == tag) {
            // Effect is already there
            mlt_service_unlock(service.get_service());
            return;
        }
    }
    while (filter != nullptr) {
        trackService.detach(*filter);
        delete filter;
        filter = trackService.filter(0);
    }

    if (tag.isEmpty()) {
        mlt_service_unlock(service.get_service());
        return;
    }

    char *tmp = qstrdup(tag.toUtf8().constData());
    filter = new Mlt::Filter(*m_mltProfile, tmp);
    delete[] tmp;
    if ((filter != nullptr) && filter->is_valid()) {
        for (int j = 0; j < parameters.count(); ++j) {
            filter->set(parameters.at(j).section(QLatin1Char('='), 0, 0).toUtf8().constData(),
                        parameters.at(j).section(QLatin1Char('='), 1, 1).toUtf8().constData());
        }
        trackService.attach(*filter);
    }
    mlt_service_unlock(service.get_service());
}

void MltDeviceCapture::mirror(bool activate)
{
    if (m_mltProducer == nullptr || !m_mltProducer->is_valid()) {
        return;
    }
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(1));
    Mlt::Playlist trackPlaylist(mlt_playlist(trackProducer.get_service()));
    Mlt::Service trackService(trackProducer.get_service());

    mlt_service_lock(service.get_service());

    // delete previous effects
    Mlt::Filter *filter;
    filter = trackService.filter(0);
    while (filter != nullptr) {
        trackService.detach(*filter);
        delete filter;
        filter = trackService.filter(0);
    }

    if (!activate) {
        mlt_service_unlock(service.get_service());
        return;
    }

    filter = new Mlt::Filter(*m_mltProfile, "mirror");
    if ((filter != nullptr) && filter->is_valid()) {
        filter->set("mirror", "flip");
        trackService.attach(*filter);
    }
    mlt_service_unlock(service.get_service());
}

void MltDeviceCapture::uyvy2rgb(const unsigned char *yuv_buffer, int width, int height)
{
    processingImage = true;
    QImage image(width, height, QImage::Format_RGB888);
    unsigned char *rgb_buffer = image.bits();

    int rgb_ptr = 0, y_ptr = 0;
    int len = width * height / 2;

    for (int t = 0; t < len; ++t) {
        int Y = yuv_buffer[y_ptr];
        int U = yuv_buffer[y_ptr + 1];
        int Y2 = yuv_buffer[y_ptr + 2];
        int V = yuv_buffer[y_ptr + 3];
        y_ptr += 4;

        int r = ((298 * (Y - 16) + 409 * (V - 128) + 128) >> 8);

        int g = ((298 * (Y - 16) - 100 * (U - 128) - 208 * (V - 128) + 128) >> 8);

        int b = ((298 * (Y - 16) + 516 * (U - 128) + 128) >> 8);

        if (r > 255) {
            r = 255;
        }
        if (g > 255) {
            g = 255;
        }
        if (b > 255) {
            b = 255;
        }

        if (r < 0) {
            r = 0;
        }
        if (g < 0) {
            g = 0;
        }
        if (b < 0) {
            b = 0;
        }

        rgb_buffer[rgb_ptr] = static_cast<uchar>(r);
        rgb_buffer[rgb_ptr + 1] = static_cast<uchar>(g);
        rgb_buffer[rgb_ptr + 2] = static_cast<uchar>(b);
        rgb_ptr += 3;

        r = ((298 * (Y2 - 16) + 409 * (V - 128) + 128) >> 8);
        g = ((298 * (Y2 - 16) - 100 * (U - 128) - 208 * (V - 128) + 128) >> 8);
        b = ((298 * (Y2 - 16) + 516 * (U - 128) + 128) >> 8);

        if (r > 255) {
            r = 255;
        }
        if (g > 255) {
            g = 255;
        }
        if (b > 255) {
            b = 255;
        }

        if (r < 0) {
            r = 0;
        }
        if (g < 0) {
            g = 0;
        }
        if (b < 0) {
            b = 0;
        }

        rgb_buffer[rgb_ptr] = static_cast<uchar>(r);
        rgb_buffer[rgb_ptr + 1] = static_cast<uchar>(g);
        rgb_buffer[rgb_ptr + 2] = static_cast<uchar>(b);
        rgb_ptr += 3;
    }
    // Q_EMIT imageReady(image);
    // m_captureDisplayWidget->setImage(image);
    Q_EMIT unblockPreview();
    // processingImage = false;
}

void MltDeviceCapture::slotPreparePreview()
{
    QTimer::singleShot(1000, this, &MltDeviceCapture::slotAllowPreview);
}

void MltDeviceCapture::slotAllowPreview()
{
    processingImage = false;
}
