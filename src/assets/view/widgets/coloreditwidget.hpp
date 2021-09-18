/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef COLOREDITWIDGET_H
#define COLOREDITWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>

class KColorButton;

/** @class ColorEditWidget
    @brief Provides options to choose a color.
    Two mechanisms are provided: color-picking directly on the screen and choosing from a list
    @author Till Theato
 */
class ColorEditWidget : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
     */
    explicit ColorEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

    /** @brief Gets the chosen color. */
    QString getColor() const;

private:
    KColorButton *m_button;

public slots:
    void slotColorModified(const QColor &color);

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

    /** @brief Updates the different color choosing options to have all selected @param color. */
    void setColor(const QColor &color);

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void modified(QColor = QColor());
};

#endif
