/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
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


#ifndef BOOLPARAMWIDGET_H
#define BOOLPARAMWIDGET_H

#include <QWidget>
#include "ui_boolparamwidget_ui.h"
#include "abstractparamwidget.h"

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class BoolParamWidget : public AbstractParamWidget, public Ui::BoolParamWidget_UI
{
    Q_OBJECT
public:

    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating wether the checkbox should initially be checked
        @param parent Parent widget
    */
    BoolParamWidget(const QString& name, const QString& comment = "", bool checked = false, QWidget *parent = Q_NULLPTR);


    /** @brief Returns the current value of the parameter
     */
    bool getValue();

public slots:
    /** @brief Toggle the comments on or off    */
    void slotShowComment(bool);
};


#endif
