/***************************************************************************
 *   Copyright (C) 2012 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "audioEnvelope.h"

#include "audioStreamInfo.h"
#include <QImage>
#include <QTime>
#include <cmath>
#include <iostream>

AudioEnvelope::AudioEnvelope(Mlt::Producer *producer) :
    m_envelope(NULL),
    m_producer(producer),
    m_envelopeStdDevCalculated(false)
{
    m_info = new AudioInfo(m_producer);
}

AudioEnvelope::~AudioEnvelope()
{
    if (m_envelope != NULL) {
        delete m_envelope;
    }
    delete m_info;
}


void AudioEnvelope::loadEnvelope()
{
    Q_ASSERT(m_envelope == NULL);

    std::cout << "Loading envelope ..." << std::endl;

    m_envelopeSize = m_producer->get_length();

    int samplingRate = m_info->info(0)->samplingRate();
    mlt_audio_format format_s16 = mlt_audio_s16;
    int channels = 1;

    Mlt::Frame *frame;
    int64_t position;
    int samples;

    m_envelope = new uint64_t[m_envelopeSize];
    m_envelopeMax = 0;
    m_envelopeMean = 0;

    QTime t;
    t.start();
    for (int i = 0; i < m_envelopeSize; i++) {

        frame = m_producer->get_frame(i);
        position = mlt_frame_get_position(frame->get_frame());
        samples = mlt_sample_calculator(m_producer->get_fps(), samplingRate, position);

        int16_t *data = static_cast<int16_t*>(frame->get_audio(format_s16, samplingRate, channels, samples));

        uint64_t sum = 0;
        for (int k = 0; k < samples; k++) {
            sum += fabs(data[k]);
        }
        m_envelope[i] = sum;

        m_envelopeMean += sum;
        if (sum > m_envelopeMax) {
            m_envelopeMax = sum;
        }
    }
    m_envelopeMean /= m_envelopeSize;
    std::cout << "Calculating the envelope (" << m_envelopeSize << " frames) took "
              << t.elapsed() << " ms." << std::endl;
}

int64_t AudioEnvelope::loadStdDev()
{
    if (m_envelopeStdDevCalculated) {
        std::cout << "Standard deviation already calculated, not re-calculating." << std::endl;
    } else {

        if (m_envelope == NULL) {
            loadEnvelope();
        }

        m_envelopeStdDev = 0;
        for (int i = 0; i < m_envelopeSize; i++) {
            m_envelopeStdDev += sqrt((m_envelope[i]-m_envelopeMean)*(m_envelope[i]-m_envelopeMean)/m_envelopeSize);
        }
        m_envelopeStdDevCalculated = true;

    }
    return m_envelopeStdDev;
}

QImage AudioEnvelope::drawEnvelope()
{
    if (m_envelope == NULL) {
        loadEnvelope();
    }

    QImage img(m_envelopeSize, 400, QImage::Format_ARGB32);
    img.fill(qRgb(255,255,255));
    double fy;
    for (int x = 0; x < img.width(); x++) {
        fy = m_envelope[x]/double(m_envelopeMax) * img.height();
        for (int y = img.height()-1; y > img.height()-1-fy; y--) {
            img.setPixel(x,y, qRgb(50, 50, 50));
        }
    }
    return img;
}

void AudioEnvelope::dumpInfo() const
{
    if (m_envelope == NULL) {
        std::cout << "Envelope not generated, no information available." << std::endl;
    } else {
        std::cout << "Envelope info" << std::endl
                  << "* size = " << m_envelopeSize << std::endl
                  << "* max = " << m_envelopeMax << std::endl
                  << "* µ = " << m_envelopeMean << std::endl
                     ;
        if (m_envelopeStdDevCalculated) {
            std::cout << "* s = " << m_envelopeStdDev << std::endl;
        }
    }
}
