/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QSize>
#include <QString>

#include <string>

/** @brief Converts an OTIO marker color to a marker type */
int fromOtioMarkerColor(const std::string &);

/** @brief Convert a marker type to the closest OTIO color */
std::string toOtioMarkerColor(int);

/** @brief Get the video size from the media. */
QSize getVideoSize(const QString &fileName);

/** @brief Get the start timecode from the media.
 *
 * Note that MLT does not provide the start timecode:
 * https://github.com/mltframework/mlt/pull/1011
 */
QString getTimecode(const QString &fileName);