/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef TRANSITIONPARAMETERVIEW_H
#define TRANSITIONPARAMETERVIEW_H

#include "assets/model/assetparametermodel.hpp"
#include <QQuickWidget>

/** @class TransitionParameterView
    @brief This class is the view of the properties of a transition
 */
class TransitionParameterView : public QQuickWidget
{
    Q_OBJECT

public:
    TransitionParameterView(QWidget *parent = nullptr);

    /** @brief Set the current model to be displayed */
    void setModel(const std::shared_ptr<AssetParameterModel> &model);

protected:
    std::shared_ptr<AssetParameterModel> m_model;
};

#endif
