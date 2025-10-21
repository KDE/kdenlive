/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


function colorWithApha(color1, alpha) {
    return Qt.rgba(color1.r, color1.g, color1.b, alpha)
}

function desaturateColor(color1, factor, alpha) {
    return Qt.hsva(color1.hsvHue, color1.hsvSaturation * factor, color1.hsvValue, alpha)
}

function softenColor(color1) {
    if (activePalette.base.hslLightness > activePalette.text.hslLightness) {
        // Light theme, brighten color
        return color1.lighter()
    } else {
        // Dark theme, brighten color
        return color1.darker()
    }
}

function mixColors(color1, color2, factor) {
    return Qt.rgba(color1.r*(1-factor) + color2.r*factor, color1.g*(1-factor) + color2.g*factor, color1.b*(1-factor) + color2.b*factor, 1)
}
