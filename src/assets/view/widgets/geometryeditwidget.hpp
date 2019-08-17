/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef GEOMEDITWIDGET_H
#define GEOMEDITWIDGET_H

#include "timecode.h"
#include <QWidget>

#include "abstractparamwidget.hpp"

class QSlider;
class GeometryWidget;

/*@brief This class is used to display a parameter with time value */
class GeometryEditWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.*/
    explicit GeometryEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent = nullptr);
    ~GeometryEditWidget() override;

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

    /** @brief initialize qml overlay
     */
    void slotInitMonitor(bool active) override;

private slots:
    /** @brief monitor seek pos changed. */
    void monitorSeek(int pos);

private:
    GeometryWidget *m_geom;
};

#endif
