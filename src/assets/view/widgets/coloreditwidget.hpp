/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef COLOREDITWIDGET_H
#define COLOREDITWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>

class KColorButton;

/**
 * @class ColorEditWidget
 * @brief Provides options to choose a color.
 Two mechanisms are provided: color-picking directly on the screen and choosing from a list
 * @author Till Theato
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
