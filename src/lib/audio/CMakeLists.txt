# SPDX-FileCopyrightText: 2012 Simon A. Eugster (Granjow) <simon.eu@gmail.com>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

set(kdenlive_SRCS
    ${kdenlive_SRCS}
    lib/audio/audioCorrelation.cpp
    lib/audio/audioCorrelationInfo.cpp
    lib/audio/audioEnvelope.cpp
    lib/audio/audioInfo.cpp
    lib/audio/audioStreamInfo.cpp
    lib/audio/fftCorrelation.cpp
    lib/audio/fftTools.cpp
    PARENT_SCOPE
)
