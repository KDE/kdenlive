/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef CLICKABLEPARAMWIDGET_H
#define CLICKABLEPARAMWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>

class QPushButton;
class QToolButton;
class QLabel;

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class ClickableLabelParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating whether the checkbox should initially be checked
        @param parent Parent widget
    */
    ClickableLabelParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

    /** @brief Returns the current value of the parameter
     */
    bool getValue();

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

private:
    QLabel *m_label;
    QToolButton *m_tb;
    QString m_displayName;
};

#endif
