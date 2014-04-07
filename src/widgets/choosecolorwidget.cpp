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


#include "widgets/choosecolorwidget.h"
#include "widgets/colorpickerwidget.h"
#include "widgets/colorwheel.h"

#include <QLabel>
#include <QHBoxLayout>

#include <KColorButton>
#include <KLocalizedString>
#include <kdeversion.h>
#include <KDebug>

static QColor stringToColor(QString strColor)
{
    bool ok = false;
    QColor color("black");
    int intval = 0;
    if (strColor.startsWith("0x")) {
        if (strColor.length() == 10) {
            // 0xRRGGBBAA
            intval = strColor.toUInt(&ok, 16);
            color.setRgb( ( intval >> 24 ) & 0xff,   // r
                          ( intval >> 16 ) & 0xff,   // g
                          ( intval >>  8 ) & 0xff,   // b
                          ( intval       ) & 0xff ); // a
        } else {
            // 0xRRGGBB, 0xRGB
            color.setNamedColor(strColor.replace(0, 2, "#"));
        }
    } else {
        if (strColor.length() == 9) {
            // #AARRGGBB
            strColor = strColor.replace('#', "0x");
            intval = strColor.toUInt(&ok, 16);
            color.setRgb( ( intval >> 16 ) & 0xff,   // r
                          ( intval >>  8 ) & 0xff,   // g
                          ( intval       ) & 0xff,   // b
                          ( intval >> 24 ) & 0xff ); // a
	} else if (strColor.length() == 8) {
	    // 0xRRGGBB
	    strColor = strColor.replace('#', "0x");
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
    stream <<  color.red() << color.green() << color.blue();
    if (alpha) {
        stream << color.alpha();
    } else {
	// MLT always wants 0xRRGGBBAA format
	stream << "ff";
    }
    return colorStr;
}

ChooseColorWidget::ChooseColorWidget(const QString &text, const QString &color, bool alphaEnabled, QWidget *parent) :
        QWidget(parent)
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(text, this);
    QColor col = stringToColor(color);
    m_button = new KColorButton(col, this);
#if KDE_IS_VERSION(4,5,0)
    if (alphaEnabled) m_button->setAlphaChannelEnabled(alphaEnabled);
#endif
//     m_button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_picker = new ColorPickerWidget(this);
    ColorWheel *wheel = new ColorWheel(col, this);
    connect(wheel, SIGNAL(colorChange(const QColor &)), this, SIGNAL(modified()));

    layout->addWidget(label, 0, 0, 1, 1, Qt::AlignTop);
    layout->addWidget(m_button, 1, 0);
    layout->addWidget(m_picker, 2, 0);
    layout->addWidget(wheel, 0, 1, -1, 1, Qt::AlignRight);
    setLayout(layout);
    //rightSideLayout->addWidget(m_button);
    //rightSideLayout->addWidget(picker, 0, Qt::AlignRight);

    connect(wheel, SIGNAL(colorChange(QColor)), this, SLOT(setColor(QColor)));
    connect(m_picker, SIGNAL(colorPicked(QColor)), this, SLOT(setColor(QColor)));
    connect(m_picker, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
    connect(m_picker, SIGNAL(disableCurrentFilter(bool)), this, SIGNAL(disableCurrentFilter(bool)));
    connect(m_button, SIGNAL(changed(QColor)), wheel, SLOT(setColor(QColor)));
    connect(m_button, SIGNAL(changed(QColor)), this, SIGNAL(modified()));
}

QString ChooseColorWidget::getColor() const
{
    bool alphaChannel = false;
#if KDE_IS_VERSION(4,5,0)
    alphaChannel = m_button->isAlphaChannelEnabled();
#endif
    return colorToString(m_button->color(), alphaChannel);
}

void ChooseColorWidget::setColor(const QColor& color)
{
    m_button->setColor(color);
}

#include "choosecolorwidget.moc"
