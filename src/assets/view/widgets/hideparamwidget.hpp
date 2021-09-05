/*
 *   SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef HIDEPARAMWIDGET_H
#define HIDEPARAMWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>

/** @brief This class represents a parameter that requires
           no display
 */
class HideParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param model The asset model
        @param index The parameter model index
        @param parent Parent widget
    */
    HideParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;
};

#endif
