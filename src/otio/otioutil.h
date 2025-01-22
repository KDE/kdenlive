/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QColor>
#include <QList>

/** @brief Converts an OTIO marker color to a marker type */
int fromOtioMarkerColor(const std::string &);

/** @brief Convert a marker type to the closest OTIO color */
std::string toOtioMarkerColor(int);
