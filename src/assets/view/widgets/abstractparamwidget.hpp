/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
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

#ifndef ABSTRACTPARAMWIDGET_H
#define ABSTRACTPARAMWIDGET_H

#include <QDebug>
#include <QPersistentModelIndex>
#include <QWidget>
#include <memory>

/** @brief Base class of all the widgets representing a parameter of an asset (effect or transition)

 */

class AssetParameterModel;
class AbstractParamWidget : public QWidget
{
    Q_OBJECT
public:
    AbstractParamWidget() = delete;
    AbstractParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);
    ~AbstractParamWidget() override = default;

    /** @brief Factory method to construct a parameter widget.
        @param model Parameter model this parameter belongs to
        @param index Index of the parameter in the given model
        @param parent parent widget
    */
    static AbstractParamWidget *construct(const std::shared_ptr<AssetParameterModel> &model, QModelIndex index, QSize frameSize, QWidget *parent);

signals:
    /** @brief Signal sent when the parameters hold by the widgets are modified
        The index corresponds which parameter is changed
        The string is the new value
        The bool allows to decide whether an undo object should be created
     */
    void valueChanged(QModelIndex, QString, bool);
    void valuesChanged(const QList <QModelIndex>, const QStringList&, bool);

    /* @brief Signal sent when the filter needs to be deactivated or reactivated.
       This happens for example when the user has to pick a color.
     */
    void disableCurrentFilter(bool);

    void seekToPos(int);
    void updateHeight(int height = -1);
    void activateEffect();

public slots:
    /** @brief Toggle the comments on or off
     */
    virtual void slotShowComment(bool) { qDebug() << "DEBUG: show_comment not correctly overridden"; }

    /** @brief refresh the properties to reflect changes in the model
     */
    virtual void slotRefresh() = 0;

    /** @brief initialize qml keyframe view after creating it
     */
    virtual void slotInitMonitor(bool /*active*/) {}

protected:
    std::shared_ptr<AssetParameterModel> m_model;
    QPersistentModelIndex m_index;
};

#endif
