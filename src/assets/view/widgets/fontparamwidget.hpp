/*
 *   SPDX-FileCopyrightText: 2016 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef FONTPARAMWIDGET_H
#define FONTPARAMWIDGET_H

#include "abstractparamwidget.hpp"
#include "ui_fontval_ui.h"
#include <QWidget>

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class FontParamWidget : public AbstractParamWidget, public Ui::Fontval_UI
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating whether the checkbox should initially be checked
        @param parent Parent widget
    */
    FontParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;
};

#endif
