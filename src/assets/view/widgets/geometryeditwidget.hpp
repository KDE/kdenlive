/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef GEOMEDITWIDGET_H
#define GEOMEDITWIDGET_H

#include "utils/timecode.h"
#include <QWidget>

#include "abstractparamwidget.hpp"

class QSlider;
class GeometryWidget;

/** @brief This class is used to display a parameter with time value */
class GeometryEditWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.*/
    explicit GeometryEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent = nullptr);
    ~GeometryEditWidget() override;

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

    /** @brief initialize qml overlay
     */
    void slotInitMonitor(bool active) override;

private slots:
    /** @brief monitor seek pos changed. */
    void monitorSeek(int pos);

private:
    GeometryWidget *m_geom;
};

#endif
