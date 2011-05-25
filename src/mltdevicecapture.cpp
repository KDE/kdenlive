/***************************************************************************
                        mltdevicecapture.cpp  -  description
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


#include "mltdevicecapture.h"
#include "kdenlivesettings.h"
#include "definitions.h"
//#include "recmonitor.h"
//#include "renderer.h"
#include "blackmagic/devices.h"

#include <mlt++/Mlt.h>

#include <KDebug>
#include <KStandardDirs>
#include <KMessageBox>
#include <KLocale>
#include <KTemporaryFile>

#include <QTimer>
#include <QDir>
#include <QString>
#include <QApplication>

#include <cstdlib>
#include <cstdarg>

#include <QDebug>



static void consumer_gl_frame_show(mlt_consumer, MltDeviceCapture * self, mlt_frame frame_ptr)
{
    // detect if the producer has finished playing. Is there a better way to do it?
    Mlt::Frame frame(frame_ptr);
    self->showFrame(frame);
}

static void rec_consumer_frame_show(mlt_consumer, MltDeviceCapture * self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    if (!frame.is_valid()) return;
    self->gotCapturedFrame(frame);
}

static void rec_consumer_frame_preview(mlt_consumer, MltDeviceCapture * self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    if (!frame.is_valid()) return;
    if (self->sendFrameForAnalysis && frame_ptr->convert_image) {
        self->emitFrameUpdated(frame);
    }
    if (self->doCapture) {
        self->doCapture = false;
        self->saveFrame(frame);
    }

/*    if (self->analyseAudio) {
        self->showAudio(frame);
    }
    if (frame.get_double("_speed") == 0.0) {
        self->emitConsumerStopped();
    } else if (frame.get_double("_speed") < 0.0 && mlt_frame_get_position(frame_ptr) <= 0) {
        self->pause();
        self->emitConsumerStopped();
    }*/
}


MltDeviceCapture::MltDeviceCapture(QString profile, VideoPreviewContainer *surface, QWidget *parent) :
    AbstractRender(parent),
    doCapture(false),
    sendFrameForAnalysis(false),
    m_mltConsumer(NULL),
    m_mltProducer(NULL),
    m_mltProfile(NULL),
    m_captureDisplayWidget(surface),
    m_winid((int) surface->winId()),
    m_analyseAudio(KdenliveSettings::monitor_audio())
{
    if (profile.isEmpty()) profile = KdenliveSettings::current_profile();
    buildConsumer(profile);
}

MltDeviceCapture::~MltDeviceCapture()
{
    if (m_mltConsumer) delete m_mltConsumer;
    if (m_mltProducer) delete m_mltProducer;
    if (m_mltProfile) delete m_mltProfile;
}

void MltDeviceCapture::buildConsumer(const QString &profileName)
{
    if (!profileName.isEmpty()) m_activeProfile = profileName;

    if (m_mltProfile) delete m_mltProfile;

    char *tmp = qstrdup(m_activeProfile.toUtf8().constData());
    setenv("MLT_PROFILE", tmp, 1);
    m_mltProfile = new Mlt::Profile(tmp);
    m_mltProfile->get_profile()->is_explicit = 1;
    delete[] tmp;

    QString videoDriver = KdenliveSettings::videodrivername();
    if (!videoDriver.isEmpty()) {
        if (videoDriver == "x11_noaccel") {
            setenv("SDL_VIDEO_YUV_HWACCEL", "0", 1);
            videoDriver = "x11";
        } else {
            unsetenv("SDL_VIDEO_YUV_HWACCEL");
        }
    }
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 1);

    if (m_winid == 0) {
        // OpenGL monitor
        m_mltConsumer = new Mlt::Consumer(*m_mltProfile, "sdl_audio");
        m_mltConsumer->set("preview_off", 1);
        m_mltConsumer->set("preview_format", mlt_image_rgb24a);
        m_mltConsumer->listen("consumer-frame-show", this, (mlt_listener) consumer_gl_frame_show);
    } else {
        m_mltConsumer = new Mlt::Consumer(*m_mltProfile, "sdl_preview");
        m_mltConsumer->set("window_id", m_winid);
    }
    m_mltConsumer->set("resize", 1);
    //m_mltConsumer->set("terminate_on_pause", 1);
    m_mltConsumer->set("window_background", KdenliveSettings::window_background().name().toUtf8().constData());
    m_mltConsumer->set("rescale", "nearest");
    
    m_mltConsumer->listen("consumer-frame-show", this, (mlt_listener) rec_consumer_frame_preview);

    QString audioDevice = KdenliveSettings::audiodevicename();
    if (!audioDevice.isEmpty())
        m_mltConsumer->set("audio_device", audioDevice.toUtf8().constData());

    if (!videoDriver.isEmpty())
        m_mltConsumer->set("video_driver", videoDriver.toUtf8().constData());

    QString audioDriver = KdenliveSettings::audiodrivername();

    if (!audioDriver.isEmpty())
        m_mltConsumer->set("audio_driver", audioDriver.toUtf8().constData());

    //m_mltConsumer->set("progressive", 1);
    //m_mltConsumer->set("buffer", 1);
    //m_mltConsumer->set("real_time", 0);
}

void MltDeviceCapture::stop()
{
    if (m_mltConsumer) {
        m_mltConsumer->stop();
        //if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
        delete m_mltConsumer;
        m_mltConsumer = NULL;
    }
    kDebug()<<"STOPPING cap";
    if (m_mltProducer) {
        QList <Mlt::Producer *> prods;
        Mlt::Service service(m_mltProducer->parent().get_service());
        mlt_service_lock(service.get_service());
kDebug()<<"STOPPING cap 2";
        if (service.type() == tractor_type) {
            kDebug()<<"STOPPING cap 3";
            Mlt::Tractor tractor(service);
            mlt_tractor_close(tractor.get_tractor());
            Mlt::Field *field = tractor.field();
            mlt_service nextservice = mlt_service_get_producer(service.get_service());
            mlt_service nextservicetodisconnect;
            mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
            QString mlt_type = mlt_properties_get(properties, "mlt_type");
            QString resource = mlt_properties_get(properties, "mlt_service");
            // Delete all transitions
            while (mlt_type == "transition") {
                nextservicetodisconnect = nextservice;
                nextservice = mlt_service_producer(nextservice);
                mlt_field_disconnect_service(field->get_field(), nextservicetodisconnect);
                if (nextservice == NULL) break;
                properties = MLT_SERVICE_PROPERTIES(nextservice);
                mlt_type = mlt_properties_get(properties, "mlt_type");
                resource = mlt_properties_get(properties, "mlt_service");
            }
            for (int trackNb = tractor.count() - 1; trackNb >= 0; --trackNb) {
                Mlt::Producer trackProducer(tractor.track(trackNb));
                Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
                if (trackPlaylist.type() == playlist_type) {
                    for (int i = 0; i < trackPlaylist.count();i++) {
                        // We need to manually decrease the ref count and close the producer, otherwise
                        // the video4linux device stays open, seems like a bug in MLT that is not cleaning properly
                        mlt_properties props = MLT_PRODUCER_PROPERTIES(trackPlaylist.get_clip(i)->get_parent());
                        while (mlt_properties_ref_count(props) > 0) mlt_properties_dec_ref(props);
                        mlt_producer_close(trackPlaylist.get_clip(i)->get_parent());
                    }
                    mlt_playlist_close(trackPlaylist.get_playlist());
                    //trackPlaylist.clear();
                }
            }
            delete field;
            field = NULL;
        }
        mlt_service_unlock(service.get_service());
        delete m_mltProducer;
        kDebug()<<"/// STOP REC PROD";
        m_mltProducer = NULL;
    }
}


void MltDeviceCapture::doRefresh()
{
    if (m_mltConsumer) m_mltConsumer->set("refresh", 1);
}


void MltDeviceCapture::emitFrameUpdated(Mlt::Frame& frame)
{
    mlt_image_format format = mlt_image_rgb24a;
    int width = 0;
    int height = 0;
    const uchar* image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_ARGB32);
    memcpy(qimage.bits(), image, width * height * 4);
    emit frameUpdated(qimage.rgbSwapped());
}

void MltDeviceCapture::showFrame(Mlt::Frame& frame)
{
    mlt_image_format format = mlt_image_rgb24a;
    int width = 0;
    int height = 0;
    const uchar* image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_ARGB32_Premultiplied);
    memcpy(qimage.scanLine(0), image, width * height * 4);
    emit showImageSignal(qimage);
    if (m_analyseAudio) showAudio(frame);
    if (sendFrameForAnalysis && frame.get_frame()->convert_image) {
        emit frameUpdated(qimage.rgbSwapped());
    }
}

void MltDeviceCapture::showAudio(Mlt::Frame& frame)
{
    if (!frame.is_valid() || frame.get_int("test_audio") != 0) {
        return;
    }
    mlt_audio_format audio_format = mlt_audio_s16;
    int freq = 0;
    int num_channels = 0;
    int samples = 0;
    int16_t* data = (int16_t*)frame.get_audio(audio_format, freq, num_channels, samples);

    if (!data) {
        return;
    }

    // Data format: [ c00 c10 c01 c11 c02 c12 c03 c13 ... c0{samples-1} c1{samples-1} for 2 channels.
    // So the vector is of size samples*channels.
    QVector<int16_t> sampleVector(samples*num_channels);
    memcpy(sampleVector.data(), data, samples*num_channels*sizeof(int16_t));

    if (samples > 0) {
        emit audioSamplesSignal(sampleVector, freq, num_channels, samples);
    }
}

bool MltDeviceCapture::slotStartPreview(const QString &producer)
{
    //stop();
    if (m_mltConsumer == NULL) buildConsumer();
    /*if (m_mltConsumer) delete m_mltConsumer;
    if (m_mltProducer) delete m_mltProducer;
    if (m_mltProfile) delete m_mltProfile;
    
    char *tmp = qstrdup(m_activeProfile.toUtf8().constData());
    setenv("MLT_PROFILE", tmp, 1);
    m_mltProfile = new Mlt::Profile(tmp);
    delete[] tmp;
    m_mltProfile->get_profile()->is_explicit = 1;

    m_mltConsumer = new Mlt::Consumer(*m_mltProfile, "sdl_preview");

    m_mltConsumer->set("window_id", m_winid);
    m_mltConsumer->set("real_time", 0);
//    m_mltConsumer->set("buffer", 1);
    m_mltConsumer->set("resize", 1);
    m_mltConsumer->set("progressive", 1);
    m_mltConsumer->set("rescale", "nearest");*/
    
    //char *tmp = qstrdup(QString("avformat-novalidate:video4linux2:%1?frame_rate:%2&width:%3&height:%4").arg(KdenliveSettings::video4vdevice()).arg(m_mltProfile->fps()).arg(m_mltProfile->width()).arg(m_mltProfile->height()).toUtf8().constData());

    char *tmp = qstrdup(producer.toUtf8().constData());
    
    m_mltProducer = new Mlt::Producer(*m_mltProfile, tmp);
    delete[] tmp;
    if (m_mltProducer == NULL || !m_mltProducer->is_valid()) {
        kDebug()<<"//// ERROR CREATRING PROD";
        return false;
    }
    m_mltConsumer->connect(*m_mltProducer);
    if (m_mltConsumer->start() == -1) {
        delete m_mltConsumer;
        m_mltConsumer = NULL;
        return 0;
    }
    return 1;
}

void MltDeviceCapture::gotCapturedFrame(Mlt::Frame& frame)
{
    mlt_image_format format = mlt_image_rgb24a;
    int width = 0;
    int height = 0;
    const uchar* image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_ARGB32);
    memcpy(qimage.bits(), image, width * height * 4);
    m_captureDisplayWidget->setImage(qimage.rgbSwapped());
}

void MltDeviceCapture::saveFrame(Mlt::Frame& frame)
{
    mlt_image_format format = mlt_image_rgb24a;
    int width = 0;
    int height = 0;
    const uchar* image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_ARGB32);
    memcpy(qimage.bits(), image, width * height * 4);
    qimage.rgbSwapped().save(m_capturePath);
    emit frameSaved(m_capturePath);
    m_capturePath.clear();
}

void MltDeviceCapture::captureFrame(const QString &path)
{
    m_capturePath = path;
    doCapture = true;
}

bool MltDeviceCapture::slotStartCapture(const QString &params, const QString &path, const QString &playlist)
{
    stop();
    if (m_mltProfile) delete m_mltProfile;
    char *tmp = qstrdup(m_activeProfile.toUtf8().constData());
    m_mltProfile = new Mlt::Profile(tmp);
    delete[] tmp;
    m_mltProfile->get_profile()->is_explicit = 1;
    kDebug()<<"-- CREATING CAP: "<<params<<", PATH: "<<path;
    tmp = qstrdup(QString("avformat:" + path).toUtf8().constData());
    m_mltConsumer = new Mlt::Consumer(*m_mltProfile, tmp);
    m_mltConsumer->set("terminate_on_pause", 1);
    delete[] tmp;

    QStringList paramList = params.split(" ", QString::SkipEmptyParts);
    char *tmp2;
    for (int i = 0; i < paramList.count(); i++) {
        tmp = qstrdup(paramList.at(i).section("=", 0, 0).toUtf8().constData());
        tmp2 = qstrdup(paramList.at(i).section("=", 1, 1).toUtf8().constData());
        m_mltConsumer->set(tmp, tmp2);
        delete[] tmp;
        delete[] tmp2;
    }
    
    if (m_mltConsumer == NULL || !m_mltConsumer->is_valid()) {
        if (m_mltConsumer) {
            delete m_mltConsumer;
            m_mltConsumer = NULL;
        }
        return false;
    }
    
    // FIXME: the event object returned by the listen gets leaked...
    m_mltConsumer->listen("consumer-frame-show", this, (mlt_listener) rec_consumer_frame_show);

    //tmp = qstrdup(QString("avformat-novalidate:video4linux2:%1?frame_rate:%2&width:%3&height:%4").arg(KdenliveSettings::video4vdevice()).arg(m_mltProfile->fps()).arg(m_mltProfile->width()).arg(m_mltProfile->height()).toUtf8().constData());
    tmp = qstrdup(playlist.toUtf8().constData());
    m_mltProducer = new Mlt::Producer(*m_mltProfile, "xml-string", tmp);
    delete[] tmp;

    if (m_mltProducer == NULL || !m_mltProducer->is_valid()) {
        kDebug()<<"//// ERROR CREATRING PROD";
        return false;
    }

    m_mltConsumer->connect(*m_mltProducer);
    if (m_mltConsumer->start() == -1) {
        delete m_mltConsumer;
        m_mltConsumer = NULL;
        return 0;
    }
    return 1;
}


