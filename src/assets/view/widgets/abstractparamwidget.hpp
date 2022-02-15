/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef ABSTRACTPARAMWIDGET_H
#define ABSTRACTPARAMWIDGET_H

#include <QDebug>
#include <QPersistentModelIndex>
#include <QWidget>
#include <memory>

class AssetParameterModel;

/** @class AbstractParamWidget
    @brief Base class of all the widgets representing a parameter of an asset (effect or transition)
 */
class AbstractParamWidget : public QWidget
{
    Q_OBJECT
public:
    AbstractParamWidget() = delete;
    AbstractParamWidget(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, QWidget *parent);
    ~AbstractParamWidget() override = default;

    /** @brief Factory method to construct a parameter widget.
        @param model Parameter model this parameter belongs to
        @param index Index of the parameter in the given model
        @param parent parent widget
    */
    static AbstractParamWidget *construct(const std::shared_ptr<AssetParameterModel> &model, const QModelIndex &index, QSize frameSize, QWidget *parent);

signals:
    /** @brief Signal sent when the parameters hold by the widgets are modified
        The index corresponds which parameter is changed
        The string is the new value
        The bool allows to decide whether an undo object should be created
     */
    void valueChanged(QModelIndex, QString, bool);

    /** @brief Signal sent when the filter needs to be deactivated or reactivated.
       This happens for example when the user has to pick a color.
     */
    void disableCurrentFilter(bool);

    void seekToPos(int);
    void updateHeight();
    void activateEffect();

public slots:
    /** @brief Toggle the comments on or off
     */
    virtual void slotShowComment(bool /*show*/) { qDebug() << "DEBUG: show_comment not correctly overridden"; }

    /** @brief refresh the properties to reflect changes in the model
     */
    virtual void slotRefresh() = 0;

    /** @brief initialize qml keyframe view after creating it
     */
    virtual void slotInitMonitor(bool /*active*/) {}

protected:
    std::shared_ptr<AssetParameterModel> m_model;
    QPersistentModelIndex m_index;
};

#endif
