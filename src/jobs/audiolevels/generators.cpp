/*
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "generators.h"

#include "audiolevelstask.h"
#include "core.h"
#include "definitions.h"
#include <KLocalizedString>
#include <KMessageWidget>
#include <QDebug>
#include <QVector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

// The av_err2str macro in libavutil/error.h does not play nice with C++
#ifdef av_err2str
#undef av_err2str
#include <string>
av_always_inline QString av_err2string(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#define av_err2str(err) av_err2string(err).toLatin1().constData();
#endif // av_err2str

void computePeaks(const int16_t *in, int16_t *out, const size_t nChannels, const size_t nSamplesIn, const size_t nSamplesOut)
{
    Q_ASSERT(in != nullptr);
    Q_ASSERT(out != nullptr);
    Q_ASSERT(nSamplesOut > 0);
    Q_ASSERT(nSamplesIn > 0);
    Q_ASSERT(nChannels > 0);

    const float scale = static_cast<float>(nSamplesIn) / nSamplesOut;

    for (size_t ch = 0; ch < nChannels; ++ch) {
        const auto *pIn = in + ch; // Pointer to the start of the current channel in the input
        auto *pOut = out + ch;     // Pointer to the start of the current channel in the output

        for (size_t outIdx = 0; outIdx < nSamplesOut; ++outIdx) {
            // [start, end] is a sliding window over the input samples
            const size_t start = outIdx * scale;
            size_t end = (outIdx + 1) * scale;
            if (end > nSamplesIn) end = nSamplesIn;

            Q_ASSERT(start < nSamplesIn);
            Q_ASSERT(outIdx < nSamplesOut);

            int16_t maxValue = std::abs(pIn[start * nChannels]);
            for (size_t inIdx = start; inIdx < end; ++inIdx) {
                maxValue = std::max(maxValue, static_cast<int16_t>(std::abs(pIn[inIdx * nChannels])));
            }
            pOut[outIdx * nChannels] = maxValue;
        }
    }
}

QVector<int16_t> generateMLT(const size_t streamIdx, const QString &service, const QString &resource, int channels,
                             const std::function<void(int progress, const QVector<int16_t> &levels)> &progressCallback, const QAtomicInt &isCanceled)
{
    qDebug() << "Generating audio levels for stream" << streamIdx << "of" << resource << "using MLT";
    QElapsedTimer timer;
    timer.start();

    // Create a new, separate producer for this clip. It will have the same fps as the current project profile
    const auto aProd = std::make_unique<Mlt::Producer>(pCore->getProjectProfile(), service.toUtf8().constData(), resource.toUtf8().constData());
    if (!aProd->is_valid()) {
        qWarning() << "Could not create producer for" << service << ":" << resource;
        return {};
    }
    aProd->set("video_index", -1); // disable video
    aProd->set("audio_index", static_cast<int>(streamIdx));
    aProd->set("cache", 0); // disable caching, should help with performance

    int sampleRate = 44100;                                                // Request this sample rate (MLT will resample under the hood)
    Mlt::Filter convertFilter(pCore->getProjectProfile(), "audioconvert"); // add a filter to convert the sample format
    aProd->attach(convertFilter);

    const double framesPerSecond = aProd->get_fps();
    const int lengthInFrames = aProd->get_length();
    mlt_audio_format audioFormat = mlt_audio_s16; // target sample format == interleaved uint16_t

    QVector<int16_t> levels(lengthInFrames * AUDIOLEVELS_POINTS_PER_FRAME * channels);
    for (int f = 0; f < lengthInFrames; ++f) {
        if (isCanceled) {
            levels.clear();
            break;
        }

        auto mltFrame = std::unique_ptr<Mlt::Frame>(aProd->get_frame());
        if (mltFrame && mltFrame->is_valid()) {
            int samples = mlt_audio_calculate_frame_samples(static_cast<float>(framesPerSecond), sampleRate, f);
            const auto buf = static_cast<int16_t *>(mltFrame->get_audio(audioFormat, sampleRate, channels, samples));
            computePeaks(buf, &levels[f * AUDIOLEVELS_POINTS_PER_FRAME * channels], channels, samples, AUDIOLEVELS_POINTS_PER_FRAME);
        } else {
            qWarning() << "invalid frame" << f;
            levels.clear();
            break;
        }

        progressCallback(100.0 * f / lengthInFrames, levels);
    }

    qDebug() << "Audio levels generation took" << timer.elapsed() / 1000.0 << "s (" << lengthInFrames / (timer.elapsed() / 1000.0) << "frames/s)";
    return levels;
}

QVector<int16_t> generateLibav(const size_t streamIdx, const QString &uri, const size_t MLTlengthInFrames, const double MLTfps,
                               const std::function<void(int progress, const QVector<int16_t> &levels)> &progressCallback, const QAtomicInt &isCanceled)
{
    qDebug() << "Generating audio levels for stream" << streamIdx << "of" << uri << "using libav";
    QElapsedTimer timer;
    timer.start();

    int ret = 0;
    size_t MLTFrameCount = 0;

    AVFormatContext *fmt_ctx = nullptr;
    const AVCodec *codec = nullptr;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    const AVStream *stream = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    QVector<int16_t> levels;
    SwrContext *swr_ctx = nullptr;

    uint8_t **buf = nullptr;
    int buf_nbsamples = 0, max_buf_nbsamples = 0;
    int src_rate, dst_rate;
    int src_nb_channels, dst_nb_channels;
    int dst_linesize;
    int dst_nb_samples = 0;
    int samplesPerMLTFrame = 0;
    AVSampleFormat src_sample_fmt, dst_sample_fmt;
    AVChannelLayout *src_ch_layout, *dst_ch_layout;
    AVAudioFifo *fifo = nullptr;

    // Open file
    ret = avformat_open_input(&fmt_ctx, uri.toLocal8Bit().data(), nullptr, nullptr);
    if (ret < 0) {
        qWarning() << "Could not open input file" << uri << ":" << av_err2string(ret);
        goto cleanup;
    }
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        qWarning() << "Could not find stream information:" << av_err2string(ret);
        goto cleanup;
    }

    if (streamIdx >= fmt_ctx->nb_streams) {
        qWarning() << "Invalid stream index" << streamIdx;
        goto cleanup;
    }

    // Find and open codec for requested stream
    stream = fmt_ctx->streams[streamIdx];

    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        qWarning() << "No suitable decoder found for" << avcodec_get_name(stream->codecpar->codec_id);
        goto cleanup;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        qWarning() << "Failed to allocate codec context";
        goto cleanup;
    }

    ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (ret < 0) {
        qWarning() << "Failed to copy codec parameters to codec:" << av_err2string(ret);
        goto cleanup;
    }

    // Request s16 to codec, if possible
    codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_S16; // == interleaved uint16_t

    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        qWarning() << "Failed to open codec:" << av_err2string(ret);
        goto cleanup;
    }

    // Add a sample format converter (will no-op if the codec is able to directly output s16)
    src_ch_layout = &codec_ctx->ch_layout;
    dst_ch_layout = src_ch_layout;
    src_nb_channels = codec_ctx->ch_layout.nb_channels;
    dst_nb_channels = src_nb_channels;
    src_sample_fmt = codec_ctx->sample_fmt;
    dst_sample_fmt = AV_SAMPLE_FMT_S16;
    src_rate = codec_ctx->sample_rate;
    dst_rate = src_rate;

    ret = swr_alloc_set_opts2(&swr_ctx, dst_ch_layout, dst_sample_fmt, dst_rate, src_ch_layout, src_sample_fmt, src_rate, 0, nullptr);
    if (ret < 0) {
        qWarning() << "Failed to set SwrContext options:" << av_err2string(ret);
        goto cleanup;
    }

    if ((ret = swr_init(swr_ctx)) < 0) {
        qWarning() << "Failed to initialize SwrContext:" << av_err2string(ret);
        goto cleanup;
    }

    // Allocate fifo with a bit of space (will be grown automatically)
    samplesPerMLTFrame = mlt_audio_calculate_frame_samples(MLTfps, dst_rate, 0);
    fifo = av_audio_fifo_alloc(dst_sample_fmt, dst_nb_channels, 2 * samplesPerMLTFrame);

    // Allocate levels
    levels.resize(MLTlengthInFrames * AUDIOLEVELS_POINTS_PER_FRAME * dst_nb_channels);

    // /!\ libav frames != MLT frames !
    // Read each packet in the stream
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (isCanceled) {
            levels.clear();
            break;
        }

        if (packet->stream_index != streamIdx) {
            av_packet_unref(packet);
            continue;
        }

        // Send encoded packet to the decoder .....
        if ((ret = avcodec_send_packet(codec_ctx, packet)) < 0) {
            qWarning() << "Error sending packet for decoding: " << av_err2string(ret);
            levels.clear();
            break;
        }

        // .... which can output more than 1 audio frame per packet
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN)) {
                break; // we're done with this packet
            }
            if (ret < 0) {
                qWarning() << "Error during decoding: " << av_err2string(ret);
                levels.clear();
                goto cleanup;
            }

            // Grow the output buffer (only if needed) to be able to store either the output from swr, or a full MLT frame's worth of data.
            dst_nb_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
            buf_nbsamples = std::max(dst_nb_samples, samplesPerMLTFrame);
            if (buf_nbsamples > max_buf_nbsamples) {
                if (buf) {
                    av_freep(&buf[0]);
                }
                av_freep(&buf);
                ret = av_samples_alloc_array_and_samples(&buf, &dst_linesize, dst_nb_channels, buf_nbsamples, dst_sample_fmt, 0);
                if (ret < 0) {
                    qWarning() << "Failed to allocate output buffer:" << av_err2string(ret);
                    levels.clear();
                    goto cleanup;
                }
                max_buf_nbsamples = buf_nbsamples;
            }

            // Convert sample format, put data into buffer
            ret = swr_convert(swr_ctx, buf, dst_nb_samples, const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples);
            if (ret <= 0) {
                qWarning() << "Failed to convert samples:" << av_err2string(ret);
                levels.clear();
                goto cleanup;
            }

            // Write the buffer into the fifo (grows automatically if needed)
            ret = av_audio_fifo_write(fifo, reinterpret_cast<void **>(buf), dst_nb_samples);
            if (ret < 0) {
                qWarning() << "Failed to write samples to audio fifo:" << av_err2string(ret);
                levels.clear();
                goto cleanup;
            }

            // If there is enough samples for one MLT frame in the fifo, compute the peaks and advance one MLT frame !
            while (av_audio_fifo_size(fifo) >= samplesPerMLTFrame) {
                av_audio_fifo_read(fifo, reinterpret_cast<void **>(buf), samplesPerMLTFrame);
                const size_t requiredSize = (MLTFrameCount + 1) * AUDIOLEVELS_POINTS_PER_FRAME * dst_nb_channels;
                if (requiredSize > levels.size()) {
                    levels.resize(requiredSize);
                }
                computePeaks(reinterpret_cast<const int16_t *>(buf[0]), levels.data() + MLTFrameCount * AUDIOLEVELS_POINTS_PER_FRAME * dst_nb_channels,
                             dst_nb_channels, samplesPerMLTFrame, AUDIOLEVELS_POINTS_PER_FRAME);

                progressCallback(100.0 * MLTFrameCount / MLTlengthInFrames, levels);

                MLTFrameCount++;
                if (MLTFrameCount > MLTlengthInFrames) {
                    qWarning() << "MLT frame" << MLTFrameCount << "of" << MLTlengthInFrames << "is beyond the MLT length !!!";
                    levels.clear();
                    goto cleanup;
                }
                samplesPerMLTFrame = mlt_audio_calculate_frame_samples(MLTfps, dst_rate, samplesPerMLTFrame);
            }
        }

        av_packet_unref(packet);
    }
cleanup:
    if (buf) {
        av_freep(&buf[0]);
    }
    av_freep(&buf);
    av_audio_fifo_free(fifo);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    swr_free(&swr_ctx);
    avformat_close_input(&fmt_ctx);

    qDebug() << "Audio levels generation took" << timer.elapsed() / 1000.0 << "s (" << MLTlengthInFrames / (timer.elapsed() / 1000.0) << "frames/s)";
    return levels;
}