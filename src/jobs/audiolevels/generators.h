/*
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include <QString>
#include <QVector>

/**
 * @brief Computes peaks on interleaved multichannel audio data.
 *
 * This function downsamples an interleaved input buffer by selecting the maximum value
 * within a sliding window for each output sample, preserving the number of channels.
 *
 * @param in Pointer to the input buffer of size nChannels * nIn (interleaved format).
 * @param out Pointer to the output buffer of size nChannels * nOut (interleaved format).
 * @param nChannels Number of audio channels in the input and output buffers.
 * @param nSamplesIn Number of input samples.
 * @param nSamplesOut Number of output samples.
 *
 */
void computePeaks(const int16_t *in, int16_t *out, size_t nChannels, size_t nSamplesIn, size_t nSamplesOut);

/** @brief Computes the audio levels using MLT to access the resource.
 *
 * This function computes the audio levels using MLT to access the resource. As such, it works with media files and all other MLT sources.
 *
 * @param streamIdx audio stream index
 * @param service MLT service name
 * @param resource MLT resource
 * @param channels number of channels in this stream
 * @param progressCallback process callback function
 * @param isCanceled task cancelled semaphor, 0 = not cancelled, 1 = cancelled
 * @return the computed audio levels
 */
QVector<int16_t> generateMLT(size_t streamIdx, const QString &service, const QString &resource, int channels,
                             const std::function<void(int progress, const QVector<int16_t> &levels)> &progressCallback, const QAtomicInt &isCanceled);

/** @brief Computes the audio levels using libav.
 *
 * This function computes the audio levels of a media file using libav directly, to avoid the speed penalty of MLT.
 *
 * @param streamIdx audio stream index
 * @param uri URI of the media file to process
 * @param MLTlengthInFrames duration of the file in MLT frames
 * @param MLTfps frames per second
 * @param progressCallback process callback function
 * @param isCanceled task cancelled semaphor, 0 = not cancelled, 1 = cancelled
 * @return the computed audio levels
 */
QVector<int16_t> generateLibav(size_t streamIdx, const QString &uri, size_t MLTlengthInFrames, double MLTfps,
                               const std::function<void(int progress, const QVector<int16_t> &levels)> &progressCallback, const QAtomicInt &isCanceled);