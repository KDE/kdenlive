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

#ifndef CLIPPROPERTIESCONTROLLER_H
#define CLIPPROPERTIESCONTROLLER_H

#include "definitions.h"
#include "timecode.h"

#include <mlt++/Mlt.h>
#include <QString>
#include <QObject>
#include <QUrl>

class QPixmap;
class BinController;

/**
 * @class ClipPropertiesController
 * @brief This class creates the widgets allowing to edit clip properties
 */


class ClipPropertiesController : public QWidget
{
  Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param id The clip's id
     * @param properties The clip's properties
     * @param parent The widget where our infos will be displayed
     */
    explicit ClipPropertiesController(Timecode tc, const QString &id, ClipType type, Mlt::Properties &properties, QWidget *parent);
    virtual ~ClipPropertiesController();

public slots:
    void slotReloadProperties();
    void slotRefreshTimeCode();

private slots:
    void slotColorModified(QColor newcolor);
    void slotDurationChanged(int duration);

private:
    QString m_id;
    ClipType m_type;
    Mlt::Properties m_properties;
    QMap <QString, QString> m_originalProperties;

signals:
    void updateClipProperties(const QString &,QMap <QString, QString>, QMap <QString, QString>);
    void modified(QColor);
    void modified(int);
    void updateTimeCodeFormat();
};

#endif
