/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#ifndef ASSETPARAMETERVIEW_H
#define ASSETPARAMETERVIEW_H

#include <QModelIndex>
#include <QVector>
#include <QWidget>
#include <memory>

/* @brief This class is the view for a list of parameters.

 */

class QVBoxLayout;
class AbstractParamWidget;
class AssetParameterModel;

class AssetParameterView : public QWidget
{
    Q_OBJECT

public:
    AssetParameterView(QWidget *parent = nullptr);

    /** Sets the model to be displayed by current view */
    void setModel(const std::shared_ptr<AssetParameterModel> &model, bool addSpacer = false);

    /** Set the widget to display no model (this yield ownership on the smart-ptr)*/
    void unsetModel();

    /** Returns the preferred widget height */
    int contentHeight() const;

    /** Reset all parameter values to default */
    void resetValues();

protected:
    /** @brief This is a handler for the dataChanged slot of the model.
        It basically instructs the widgets in the given range to be refreshed */
    void refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

    QVBoxLayout *m_lay;
    std::shared_ptr<AssetParameterModel> m_model;
    std::vector<AbstractParamWidget *> m_widgets;

private slots:
    /** @brief Apply a change of parameter sent by the view
       @param index is the index corresponding to the modified param
       @param value is the new value of the parameter
    */
    void commitChanges(const QModelIndex &index, const QString &value);
};

#endif
