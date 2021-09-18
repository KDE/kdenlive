/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef BUTTONPARAMWIDGET_H
#define BUTTONPARAMWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>

class QPushButton;
class QProgressBar;
class KMessageWidget;

/** @brief This class represents a parameter that requires
           the user to choose tick a checkbox
 */
class ButtonParamWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param checked Boolean indicating whether the checkbox should initially be checked
        @param parent Parent widget
    */
    ButtonParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

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
    QPushButton *m_button;
    QProgressBar *m_progress;
    KMessageWidget *m_label;
    QString m_keyParam;
    QString m_buttonName;
    QString m_alternatebuttonName;
    bool m_displayConditional;
};

#endif
