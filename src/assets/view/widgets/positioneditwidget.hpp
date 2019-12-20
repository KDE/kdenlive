/***************************************************************************
                          positionedit.h  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef POSITIONEDITWIDGET_H
#define POSITIONEDITWIDGET_H

#include "abstractparamwidget.hpp"
#include "timecode.h"
#include <QWidget>

class QSlider;
class TimecodeDisplay;

/*@brief This class is used to display a parameter with time value */
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
