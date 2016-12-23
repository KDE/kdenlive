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


#ifndef ABSTRACTPARAMWIDGET_H
#define ABSTRACTPARAMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPoint>
#include <QDebug>

/** @brief Base class of all the widgets representing a parameter of an effect

 */
class AbstractParamWidget : public QWidget
{
    Q_OBJECT
public:
    AbstractParamWidget() = delete;
    AbstractParamWidget(QWidget* w):QWidget(w){}
    virtual ~AbstractParamWidget(){};

    /** @brief Factory method to construct a parameter widget given its xml representation
        @param content XML representation of the widget
        @param parent Optional parent of the widget
        TODO
    */
    //static AbstractParamWidget* construct(const QDomElement& content, QWidget* parent = nullptr);

signals:
    /** @brief Signal sent when the parameters hold by the widgets are modified
     */
    void valueChanged();

    /* @brief Signal sent when the filter needs to be deactivated or reactivated.
       This happens for example when the user has to pick a color.
     */
    void disableCurrentFilter(bool);
public slots:
    /** @brief Toggle the comments on or off
     */
    virtual void slotShowComment(bool){
        qDebug()<<"DEBUG: show_comment not correctly overriden";
    };

};


#endif
