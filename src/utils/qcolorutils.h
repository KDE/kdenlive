/*
    SPDX-FileCopyrightText: 2017 Dušan Hanuš <hanus@pixelhouse.cz>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef QCOLORUTILS_H
#define QCOLORUTILS_H

#include <QtGlobal>
#include <QColor>

class QColor;

class QColorUtils
{
public:
    static QColor stringToColor(QString strColor);
    static QString colorToString(const QColor &color, bool alpha);
};

class NegQColor
{
public:
    NegQColor();
    int8_t sign_r = 1;
    int8_t sign_g = 1;
    int8_t sign_b = 1;
    QColor qcolor;
    static NegQColor fromHsvF(qreal h, qreal s, qreal l, qreal a = 1.0);
    static NegQColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);
    qreal redF() const;
    void setRedF(qreal val);
    qreal greenF() const;
    void setGreenF(qreal val);
    qreal blueF() const;
    void setBlueF(qreal val);
    qreal valueF() const;
    void setValueF(qreal val);
    int hue() const;
    qreal hueF() const;
    qreal saturationF() const;
};

#endif // QCOLORUTILS_H
