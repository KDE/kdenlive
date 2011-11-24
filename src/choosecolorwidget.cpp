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

#include <QLabel>
#include <QHBoxLayout>

#include <KColorButton>
#include <KLocalizedString>
#include <kdeversion.h>

static QColor stringToColor(QString strColor)
{
    bool ok = false;
    QColor color("black");
    int intval = 0;

    if (strColor.startsWith("0x")) {
        // Format must be 0xRRGGBBAA
        intval = strColor.toUInt(&ok, 16);
        color.setRgb( ( intval >> 24 ) & 0xff,   // r
                      ( intval >> 16 ) & 0xff,   // g
                      ( intval >>  8 ) & 0xff,   // b
                      ( intval )       & 0xff ); // a
    } else if (strColor.startsWith("#") && strColor.length() == 9) {
        // Format must be #AARRGGBB
        strColor = strColor.replace('#', "0x");
        intval = strColor.toUInt(&ok, 16);
        color.setRgb( ( intval >> 16 ) & 0xff,   // r
                      ( intval >>  8 ) & 0xff,   // g
                      ( intval       ) & 0xff,   // b
                      ( intval >> 24 ) & 0xff ); // a
    } else if (strColor.startsWith("#") && strColor.length() == 7) {
        // Format must be #RRGGBB
        strColor = strColor.replace('#', "0x");
        intval = strColor.toUInt(&ok, 16);
        color.setRgb( ( intval >> 16 ) & 0xff,   // r
                      ( intval >>  8 ) & 0xff,   // g
                      ( intval       ) & 0xff,   // b
                      0xff );                    // a
    }

    return color;
}

static QString colorToString(QColor color, bool alpha)
{
    QString colorStr;
    QTextStream stream(&colorStr);
    stream << "#";
    stream.setIntegerBase(16);
    stream.setFieldWidth(2);
    stream.setFieldAlignment(QTextStream::AlignRight);
    stream.setPadChar('0');
    if(alpha)
    {
        stream << color.alpha();
    }
    stream <<  color.red() << color.green() << color.blue();

    return colorStr;
}

ChooseColorWidget::ChooseColorWidget(QString text, QString color, QWidget *parent) :
        QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(text, this);

    QWidget *rightSide = new QWidget(this);
    QHBoxLayout *rightSideLayout = new QHBoxLayout(rightSide);
    rightSideLayout->setContentsMargins(0, 0, 0, 0);
    rightSideLayout->setSpacing(0);

    m_button = new KColorButton(stringToColor(color), rightSide);
//     m_button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    ColorPickerWidget *picker = new ColorPickerWidget(rightSide);

    layout->addWidget(label);
    layout->addWidget(rightSide);
    rightSideLayout->addWidget(m_button);
    rightSideLayout->addWidget(picker, 0, Qt::AlignRight);

    connect(picker, SIGNAL(colorPicked(QColor)), this, SLOT(setColor(QColor)));
    connect(picker, SIGNAL(displayMessage(const QString&, int)), this, SIGNAL(displayMessage(const QString&, int)));
    connect(m_button, SIGNAL(changed(QColor)), this, SIGNAL(modified()));
}

QString ChooseColorWidget::getColor()
{
    bool alphaChannel = false;
#if KDE_IS_VERSION(4,5,0)
    alphaChannel = m_button->isAlphaChannelEnabled();
#endif
    return colorToString(m_button->color(), alphaChannel);
}

void ChooseColorWidget::setAlphaChannelEnabled(bool enabled)
{
#if KDE_IS_VERSION(4,5,0)
    m_button->setAlphaChannelEnabled(enabled);
#endif
}

void ChooseColorWidget::setColor(QColor color)
{
    m_button->setColor(color);
}

#include "choosecolorwidget.moc"
