/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioutil.h"

#include "core.h"

#include <QMap>

namespace {

const QMap<std::string, QColor> otioMarkerColors = {{"PINK", QColor(255, 192, 203)}, {"RED", QColor(255, 0, 0)},       {"ORANGE", QColor(255, 120, 0)},
                                                    {"YELLOW", QColor(255, 255, 0)}, {"GREEN", QColor(0, 255, 0)},     {"CYAN", QColor(0, 255, 255)},
                                                    {"BLUE", QColor(0, 0, 255)},     {"PURPLE", QColor(160, 32, 240)}, {"MAGENTA", QColor(255, 0, 255)},
                                                    {"BLACK", QColor(0, 0, 0)},      {"WHITE", QColor(255, 255, 255)}};

int closestColor(const QColor &color, const QList<QColor> &colors)
{
    int out = -1;
    const float colorHue = color.hueF();
    float closestHue = 1.F;
    for (auto i = colors.begin(); i != colors.end(); ++i) {
        const float otioHue = i->hueF();
        const float diff = qAbs(colorHue - otioHue);
        if (diff < closestHue) {
            out = i - colors.begin();
            closestHue = diff;
        }
    }
    return out;
}

} // namespace

int fromOtioMarkerColor(const std::string &otioColor)
{
    int markerType = -1;
    QList<int> ids;
    QList<QColor> colors;
    for (auto i = pCore->markerTypes.begin(); i != pCore->markerTypes.end(); ++i) {
        ids.push_back(i.key());
        colors.push_back(i->color);
    }
    auto j = otioMarkerColors.find(otioColor);
    if (j != otioMarkerColors.end()) {
        const int index = closestColor(*j, colors);
        if (index != -1) {
            markerType = ids[index];
        }
    }
    return markerType;
}

std::string toOtioMarkerColor(int markerType)
{
    std::string out = "RED";
    const auto i = pCore->markerTypes.find(markerType);
    if (i != pCore->markerTypes.end()) {
        QColor color = i->color;
        const int index = closestColor(color, otioMarkerColors.values());
        if (index != -1) {
            out = otioMarkerColors.key(otioMarkerColors.values()[index]);
        }
    }
    return out;
}