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
#include "effectstack/widgets/choosecolorwidget.h"

#include <KLocalizedString>

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <QVBoxLayout>


ClipPropertiesController::ClipPropertiesController(const QString &id, ClipType type, Mlt::Properties &properties, QWidget *parent) : QWidget(parent)
    , m_id(id)
    , m_type(type)
    , m_properties(properties)
{
    if (type == Color) {
        QVBoxLayout *vbox = new QVBoxLayout;
        m_originalProperties.insert("resource", m_properties.get("resource"));
        mlt_color color = m_properties.get_color("resource");
        ChooseColorWidget *choosecolor = new ChooseColorWidget(i18n("Color"), QColor::fromRgb(color.r, color.g, color.b).name(), false, this);
        vbox->addWidget(choosecolor);
        connect(choosecolor, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
        connect(choosecolor, SIGNAL(modified(QColor)), this, SLOT(slotColorModified(QColor)));
        connect(this, SIGNAL(modified(QColor)), choosecolor, SLOT(slotColorModified(QColor)));
        setLayout(vbox);
        vbox->addStretch(10);
        choosecolor->show();
    }
}

ClipPropertiesController::~ClipPropertiesController()
{
}

void ClipPropertiesController::slotReloadProperties()
{
    if (m_type == Color) {
        m_originalProperties.insert("resource", m_properties.get("resource"));
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

