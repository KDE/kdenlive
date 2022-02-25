/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Dušan Hanuš <hanus@pixelhouse.cz>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "qcolorutils.h"

#include <QTextStream>

QColor QColorUtils::stringToColor(QString strColor)
{
    bool ok = false;
    QColor color("black");
    if (strColor.startsWith(QLatin1String("0x"))) {
        if (strColor.length() == 10) {
            // 0xRRGGBBAA
            uint intval = strColor.toUInt(&ok, 16);
            color.setRgb((intval >> 24) & 0xff, // r
                         (intval >> 16) & 0xff, // g
                         (intval >>  8) & 0xff, // b
                          intval        & 0xff); // a
        } else {
            // 0xRRGGBB, 0xRGB
            color.setNamedColor(strColor.replace(0, 2, QLatin1Char('#')));
        }
    } else {
        if (strColor.length() == 9) {
            // #AARRGGBB
            strColor = strColor.replace('#', QLatin1String("0x"));
            uint intval = strColor.toUInt(&ok, 16);
            color.setRgb((intval >> 16) & 0xff, // r
                         (intval >>  8) & 0xff, // g
                          intval        & 0xff, // b
                         (intval >> 24) & 0xff); // a
        } else if (strColor.length() == 8) {
            // 0xRRGGBB
            strColor = strColor.replace('#', QLatin1String("0x"));
            color.setNamedColor(strColor);
        } else {
            // #RRGGBB, #RGB
            color.setNamedColor(strColor);
        }
    }

    return color;
}

QString QColorUtils::colorToString(const QColor &color, bool alpha)
{
    QString colorStr;
    QTextStream stream(&colorStr);
    stream << "0x";
    stream.setIntegerBase(16);
    stream.setFieldWidth(2);
    stream.setFieldAlignment(QTextStream::AlignRight);
    stream.setPadChar('0');
    stream << color.red() << color.green() << color.blue();
    if (alpha) {
        stream << color.alpha();
    } else {
        // MLT always wants 0xRRGGBBAA format
        stream << "ff";
    }
    return colorStr;
}

NegQColor::NegQColor()
{

}

NegQColor NegQColor::fromHsvF(qreal h, qreal s, qreal l, qreal a)
{
    NegQColor color;
    color.qcolor = QColor::fromHsvF(h, s, l < 0 ? -l : l, a);
    color.sign_r = l < 0 ? -1 : 1;
    color.sign_g = l < 0 ? -1 : 1;
    color.sign_b = l < 0 ? -1 : 1;
    return color;
}

QDebug operator<<(QDebug qd, const NegQColor &color)
{
    qd << color.qcolor;
    return qd.maybeSpace();
}

NegQColor NegQColor::fromRgbF(qreal r, qreal g, qreal b, qreal a)
{
    NegQColor color;
    color.qcolor = QColor::fromRgbF(r < 0 ? -r : r, g < 0 ? -g : g, b < 0 ? -b : b, a);
    color.sign_r = r < 0 ? -1 : 1;
    color.sign_g = g < 0 ? -1 : 1;
    color.sign_b = b < 0 ? -1 : 1;
    return color;
}

qreal NegQColor::redF() const
{
    return qcolor.redF() * sign_r;
}

void NegQColor::setRedF(qreal val)
{
    sign_r = val < 0 ? -1 : 1;
    qcolor.setRedF(val * sign_r);
}

qreal NegQColor::greenF() const
{
    return qcolor.greenF() * sign_g;
}

void NegQColor::setGreenF(qreal val)
{
    sign_g = val < 0 ? -1 : 1;
    qcolor.setGreenF(val * sign_g);
}

qreal NegQColor::blueF() const
{
    return qcolor.blueF() * sign_b;
}

void NegQColor::setBlueF(qreal val)
{
    sign_b = val < 0 ? -1 : 1;
    qcolor.setBlueF(val * sign_b);
}

void NegQColor::setValueF(qreal val)
{
    qcolor = QColor::fromHsvF(hueF(), saturationF(), val < 0 ? -val : val, 1.);
    sign_r = val < 0 ? -1 : 1;
    sign_g = val < 0 ? -1 : 1;
    sign_b = val < 0 ? -1 : 1;
}

qreal NegQColor::valueF() const
{
    return qcolor.valueF() * sign_g;
}

int NegQColor::hue() const
{
    return qcolor.hue();
}

qreal NegQColor::hueF() const
{
    return qcolor.hueF();
}

qreal NegQColor::saturationF() const
{
    return qcolor.saturationF();
}
