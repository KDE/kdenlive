/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef LISTPARAMWIDGET_H
#define LISTPARAMWIDGET_H

#include "assets/view/widgets/abstractparamwidget.hpp"
#include "ui_listparamwidget_ui.h"
#include <QVariant>
#include <QWidget>

class AssetParameterModel;

/** @brief This class represents a parameter that requires
           the user to choose a value from a list
 */
class ListParamWidget : public AbstractParamWidget, public Ui::ListParamWidget_UI
{
    Q_OBJECT
public:
    /** @brief Constructor for the widgetComment
        @param name String containing the name of the parameter
        @param comment Optional string containing the comment associated to the parameter
        @param parent Parent widget
    */
    ListParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

    /** @brief Set the index of the current displayed element
        @param index Integer holding the index of the target element (0-indexed)
    */
    void setCurrentIndex(int index);

    /** @brief Set the text currently displayed on the list
        @param text String containing the text of the element to show
    */
    void setCurrentText(const QString &text);

    /** @brief Add an item to the list.
        @param text String to be displayed in the list
        @param value Underlying value corresponding to the text
    */
    void addItem(const QString &text, const QVariant &value = QVariant());

    /** @brief Set the icon of a given element
        @param index Integer holding the index of the target element (0-indexed)
        @param icon The corresponding icon
    */
    void setItemIcon(int index, const QIcon &icon);

    /** @brief Set the size of the icons shown in the list
        @param size Target size of the icon
    */
    void setIconSize(const QSize &size);

    /** @brief Returns the current value of the parameter
     */
    QString getValue();

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

};

#endif
