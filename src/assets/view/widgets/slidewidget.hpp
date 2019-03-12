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

#ifndef SLIDEWIDGET_H
#define SLIDEWIDGET_H

#include "abstractparamwidget.hpp"
#include "ui_wipeval_ui.h"

#include <QWidget>

/**
 * @class SlideWidget
 * @brief Provides options to choose slide.
 * @author Jean-Baptiste Mardelle
 */

class SlideWidget : public AbstractParamWidget, public Ui::Wipeval_UI
{
    Q_OBJECT
public:
    enum WIPE_DIRECTON { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, CENTER = 4 };
    struct wipeInfo
    {
        WIPE_DIRECTON start;
        WIPE_DIRECTON end;
        int startTransparency;
        int endTransparency;
    };
    /** @brief Sets up the widget.
     */
    explicit SlideWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent);

    /** @brief Gets the chosen slide. */
    QString getSlide() const;

private:
    wipeInfo getWipeInfo(QString value);
    const QString getWipeString(wipeInfo info);

public slots:
    /** @brief Toggle the comments on or off
     */
    void slotShowComment(bool show) override;

    /** @brief refresh the properties to reflect changes in the model
     */
    void slotRefresh() override;

    /** @brief Updates the different color choosing options to have all selected @param color. */
    void updateValue();

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void modified(const QString &);
};

#endif
