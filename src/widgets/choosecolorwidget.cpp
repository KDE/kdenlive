/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "choosecolorwidget.h"
#include "colorpickerwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextStream>

#include <KColorButton>

static QColor stringToColor(QString strColor)
{
    bool ok = false;
    QColor color("black");
    uint intval = 0;
    if (strColor.startsWith(QLatin1String("0x"))) {
        if (strColor.length() == 10) {
            // 0xRRGGBBAA
            intval = strColor.toUInt(&ok, 16);
            color.setRgb((int)((intval >> 24) & 0xff), // r
                         (intval >> 16) & 0xff,        // g
                         (intval >> 8) & 0xff,         // b
                         intval & 0xff);               // a
        } else {
            // 0xRRGGBB, 0xRGB
            color.setNamedColor(strColor.replace(0, 2, QLatin1Char('#')));
        }
    } else {
        if (strColor.length() == 9) {
            // #AARRGGBB
            strColor = strColor.replace('#', QLatin1String("0x"));
            intval = strColor.toUInt(&ok, 16);
            color.setRgb((intval >> 16) & 0xff,         // r
                         (intval >> 8) & 0xff,          // g
                         intval & 0xff,                 // b
                         (int)((intval >> 24) & 0xff)); // a
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

static QString colorToString(const QColor &color, bool alpha)
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

ChooseColorWidget::ChooseColorWidget(const QString &text, const QString &color, const QString &comment, bool alphaEnabled, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(text, this);

    QWidget *rightSide = new QWidget(this);
    auto *rightSideLayout = new QHBoxLayout(rightSide);
    rightSideLayout->setContentsMargins(0, 0, 0, 0);
    rightSideLayout->setSpacing(0);

    m_button = new KColorButton(stringToColor(color), rightSide);
    if (alphaEnabled) {
        m_button->setAlphaChannelEnabled(alphaEnabled);
    }
    //     m_button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *picker = new ColorPickerWidget(rightSide);

    layout->addWidget(label, 1);
    layout->addWidget(rightSide, 1);
    rightSideLayout->addStretch();
    rightSideLayout->addWidget(m_button, 2);
    rightSideLayout->addWidget(picker);

    connect(picker, &ColorPickerWidget::colorPicked, this, &ChooseColorWidget::setColor);
    connect(picker, &ColorPickerWidget::disableCurrentFilter, this, &ChooseColorWidget::disableCurrentFilter);
    connect(m_button, &KColorButton::changed, this, &ChooseColorWidget::modified);

    // connect the signal of the derived class to the signal of the base class
    connect(this, &ChooseColorWidget::modified, [this](const QColor &) { emit valueChanged(); });

    // setup comment
    setToolTip(comment);
}

QString ChooseColorWidget::getColor() const
{
    bool alphaChannel = m_button->isAlphaChannelEnabled();
    return colorToString(m_button->color(), alphaChannel);
}

void ChooseColorWidget::setColor(const QColor &color)
{
    m_button->setColor(color);
}

void ChooseColorWidget::slotColorModified(const QColor &color)
{
    blockSignals(true);
    m_button->setColor(color);
    blockSignals(false);
}
