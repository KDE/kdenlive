/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include <QWidget>

class PointWidget;

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class PointParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating whether the checkbox should initially be checked
        @param parent Parent widget
    */
    PointParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);
    void setValue(QPointF val);

public Q_SLOTS:
    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

private:
    PointWidget *m_pointWidget;
    QPointF dataToPoint(QVariant data, QPointF defaultVal = QPointF());
};
