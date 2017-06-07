/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef ASSETPANEL_H
#define ASSETPANEL_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <memory>

/** @brief This class is the widget that provides interaction with the asset currently selected.
    That is, it either displays an effectStack or the parameters of a transition
 */

class AssetParameterModel;
class AssetParameterView;
class EffectStackModel;
class EffectStackView;
class QLabel;

class AssetPanel : public QScrollArea
{
    Q_OBJECT

public:
    AssetPanel(QWidget *parent);

    /* @brief Shows the parameters of the given transition model */
    void showTransition(int tid, std::shared_ptr<AssetParameterModel> transition_model);

    /* @brief Shows the parameters of the given effect stack model */
    void showEffectStack(int cid, std::shared_ptr<EffectStackModel> effectsModel);

    /* @brief Clear the panel so that it doesn't display anything */
    void clear();

    /* @brief This method should be called when the style changes */
    void updatePalette();

protected:
    /** @brief Return the stylesheet used to display the panel (based on current palette). */
    static const QString getStyleSheet();

    QVBoxLayout *m_lay;
    QLabel *m_assetTitle;
    AssetParameterView *m_transitionWidget;
    EffectStackView *m_effectStackWidget;
};

#endif
