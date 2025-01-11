#!/bin/bash

# SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

# This file creates a few audio test files.
# All channels are identical and consist of 1 second of a 1000Hz full-volume sine wave amplitude modulated at 5 Hz.

# lots_of_audio_streams.mkv: multiple audio streams in a matroska container, with varying sample rates and bitrates
# stream 0: mono (1 channel), 16 kHz
# stream 1: stereo (2 channels), 24 kHz, 128k
# stream 2: surround (6 channels), 48 kHz, 192k
ffmpeg -y \
-f lavfi -i "sine=frequency=1000:duration=1,tremolo=f=5:d=1,volume=8" -t 1 \
-filter_complex "[0:a]pan=mono|c0=c0[a_mono]; \
[0:a]aformat=channel_layouts=stereo[a_stereo]; \
[0:a]pan=5.1|c0=c0|c1=c0|c2=c0|c3=c0|c4=c0|c5=c0[a_surround]" \
-map "[a_mono]" -c:a:0 pcm_s32le -ar:0 16000 \
-map "[a_stereo]" -c:a:1 pcm_s32le -ar:1 24000 \
-map "[a_surround]" -c:a:2 pcm_s32le -ar:2 48000 \
lots_of_audio_streams.mkv

# mono.flac: mono (1 channel), default flac options
ffmpeg -y \
-f lavfi -i "sine=frequency=1000:duration=1,tremolo=f=5:d=1,volume=8" -t 1 \
mono.flac

# stereo.flac: stereo (2 channels), default flac options
ffmpeg -y \
-f lavfi -i "sine=frequency=1000:duration=1,tremolo=f=5:d=1,volume=8" -t 1 \
-f lavfi -i "sine=frequency=1000:duration=1,tremolo=f=5:d=1,volume=8" \
-filter_complex amerge \
stereo.flac