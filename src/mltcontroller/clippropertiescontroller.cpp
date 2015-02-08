/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clippropertiescontroller.h"
#include "bincontroller.h"
#include "timecodedisplay.h"
#include "effectstack/widgets/choosecolorwidget.h"

#include <KLocalizedString>

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <QVBoxLayout>
#include <QLabel>

ClipPropertiesController::ClipPropertiesController(Timecode tc, const QString &id, ClipType type, Mlt::Properties &properties, QWidget *parent) : QWidget(parent)
    , m_id(id)
    , m_type(type)
    , m_properties(properties)
{
    if (type == Color || type == Image) {
        QVBoxLayout *vbox = new QVBoxLayout;
        // Edit duration widget
        m_originalProperties.insert("out", m_properties.get("out"));
        m_originalProperties.insert("length", m_properties.get("length"));
        QLabel *lab = new QLabel(i18n("Duration"), this);
        vbox->addWidget(lab);
        TimecodeDisplay *timePos = new TimecodeDisplay(tc, this);
        timePos->setValue(m_properties.get_int("out") + 1);
        vbox->addWidget(timePos);
        connect(timePos, SIGNAL(timeCodeEditingFinished(int)), this, SLOT(slotDurationChanged(int)));
        connect(this, SIGNAL(updateTimeCodeFormat()), timePos, SLOT(slotUpdateTimeCodeFormat()));
        connect(this, SIGNAL(modified(int)), timePos, SLOT(setValue(int)));
        if (type == Color) {
            // Edit color widget
            m_originalProperties.insert("resource", m_properties.get("resource"));
            mlt_color color = m_properties.get_color("resource");
            ChooseColorWidget *choosecolor = new ChooseColorWidget(i18n("Color"), QColor::fromRgb(color.r, color.g, color.b).name(), false, this);
            vbox->addWidget(choosecolor);
            //connect(choosecolor, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
            connect(choosecolor, SIGNAL(modified(QColor)), this, SLOT(slotColorModified(QColor)));
            connect(this, SIGNAL(modified(QColor)), choosecolor, SLOT(slotColorModified(QColor)));
        }
        setLayout(vbox);
        vbox->addStretch(10);
    }
}

ClipPropertiesController::~ClipPropertiesController()
{
}

void ClipPropertiesController::slotRefreshTimeCode()
{
    emit updateTimeCodeFormat();
}

void ClipPropertiesController::slotReloadProperties()
{
    if (m_type == Color) {
        m_originalProperties.insert("resource", m_properties.get("resource"));
        m_originalProperties.insert("out", m_properties.get("out"));
        m_originalProperties.insert("length", m_properties.get("length"));
        emit modified(m_properties.get_int("length"));
        mlt_color color = m_properties.get_color("resource");
        emit modified(QColor::fromRgb(color.r, color.g, color.b));
    }
}

void ClipPropertiesController::slotColorModified(QColor newcolor)
{
    QMap <QString, QString> properties;
    properties.insert("resource", newcolor.name(QColor::HexArgb));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotDurationChanged(int duration)
{
    QMap <QString, QString> properties;
    properties.insert("length", QString::number(duration));
    properties.insert("out", QString::number(duration - 1));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}


