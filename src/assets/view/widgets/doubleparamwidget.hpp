/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include <QModelIndex>
#include <memory>

class AssetParameterModel;
class DoubleWidget;
class QVBoxLayout;

/** @class DoubleParamWidget
    @brief This represent a double parameter
 */
class DoubleParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    DoubleParamWidget() = delete;
    DoubleParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

protected:
    DoubleWidget *m_doubleWidget;
    QVBoxLayout *m_lay;
};
