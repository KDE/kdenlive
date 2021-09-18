/*
    SPDX-FileCopyrightText: 2008 Marco Gittler <g.marco@freenet.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef POSITIONEDITWIDGET_H
#define POSITIONEDITWIDGET_H

#include "abstractparamwidget.hpp"
#include "timecode.h"
#include <QWidget>

class QSlider;
class TimecodeDisplay;

/** @brief This class is used to display a parameter with time value */
class PositionEditWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.*/
    explicit PositionEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent = nullptr);
    ~PositionEditWidget() override;
    /** @brief get current position
     */
    int getPosition() const;
    /** @brief set position
     */
    void setPosition(int pos);
    /** @brief Call this when the timecode has been changed project-wise
     */
    void updateTimecodeFormat();
    /** @brief checks that the allowed time interval is valid
     */
    bool isValid() const;

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

private:
    TimecodeDisplay *m_display;
    QSlider *m_slider;
    bool m_inverted;

private slots:
    void slotUpdatePosition();

signals:
    void valueChanged();
};

#endif
