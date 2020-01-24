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

#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "definitions.h"

class KSqueezedTextLabel;
class KDualAction;
class KMessageWidget;
class QToolButton;
class QComboBox;
class QScrollArea;

/** @brief This class is the widget that provides interaction with the asset currently selected.
    That is, it either displays an effectStack or the parameters of a transition
 */

class AssetParameterModel;
class AssetParameterView;
class EffectStackModel;
class EffectStackView;
class TransitionStackView;
class QLabel;

class AssetPanel : public QWidget
{
    Q_OBJECT

public:
    AssetPanel(QWidget *parent);

    /* @brief Shows the parameters of the given transition model */
    void showTransition(int tid, const std::shared_ptr<AssetParameterModel> &transition_model);

    /* @brief Shows the parameters of the given effect stack model */
    void showEffectStack(const QString &itemName, const std::shared_ptr<EffectStackModel> &effectsModel, QSize frameSize, bool showKeyframes);

    /* @brief Clear the panel so that it doesn't display anything */
    void clear();

    /* @brief This method should be called when the style changes */
    void updatePalette();
    /* @brief Returns the object type / id of effectstack owner */
    ObjectId effectStackOwner();
    /* @brief Add an effect to the current stack owner */
    bool addEffect(const QString &effectId);

public slots:
    /** @brief Clear panel if displaying itemId */
    void clearAssetPanel(int itemId);
    void assetPanelWarning(const QString service, const QString id, const QString message);
    void deleteCurrentEffect();
    /** @brief Collapse/expand current effect */
    void collapseCurrentEffect();
    void slotCheckWheelEventFilter();

protected:
    /** @brief Return the stylesheet used to display the panel (based on current palette). */
    static const QString getStyleSheet();
    QVBoxLayout *m_lay;
    KSqueezedTextLabel *m_assetTitle;
    QWidget *m_container;
    TransitionStackView *m_transitionWidget;
    EffectStackView *m_effectStackWidget;

private:
    QToolButton *m_switchBuiltStack;
    QComboBox *m_switchCompoButton;
    QAction *m_titleAction;
    QAction *m_switchAction;
    KDualAction *m_splitButton;
    KDualAction *m_enableStackButton;
    KDualAction *m_timelineButton;
    QScrollArea *m_sc;
    KMessageWidget *m_infoMessage;

private slots:
    void processSplitEffect(bool enable);
    /** Displays the owner clip keyframes in timeline */
    void showKeyframes(bool enable);
    /** Enable / disable effect stack */
    void enableStack(bool enable);

signals:
    void doSplitEffect(bool);
    void doSplitBinEffect(bool);
    void seekToPos(int);
    void reloadEffect(const QString &path);
    void switchCurrentComposition(int tid, const QString &compoId);
};

#endif
