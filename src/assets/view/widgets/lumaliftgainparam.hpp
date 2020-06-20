/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef LUMALIFTGAINPARAMWIDGET_H
#define LUMALIFTGAINPARAMWIDGET_H

#include "abstractparamwidget.hpp"
#include <QWidget>
#include <QDomElement>

class ColorWheel;
class FlowLayout;

/**
 * @class LumaLiftGainParam
 * @brief Provides options to choose 3 colors.
 * @author Jean-Baptiste Mardelle
 */

class LumaLiftGainParam : public AbstractParamWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
     * @param text (optional) What the color will be used for
     * @param color (optional) initial color
     * @param alphaEnabled (optional) Should transparent colors be enabled */
    explicit LumaLiftGainParam(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);
    void updateEffect(QDomElement &effect);

private:
    ColorWheel *m_lift;
    ColorWheel *m_gamma;
    ColorWheel *m_gain;
    FlowLayout *m_flowLayout;

protected:
    void resizeEvent(QResizeEvent *ev) override;

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void liftChanged();
    void gammaChanged();
    void gainChanged();

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;
};

#endif
