/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include <QWidget>

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class EffectButtonsParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating whether the checkbox should initially be checked
        @param parent Parent widget
    */
    EffectButtonsParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

public Q_SLOTS:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

private Q_SLOTS:
    void buttonClicked(bool checked);
    void paramChanged(int state);
};
