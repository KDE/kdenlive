/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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

class AssetParameterModel;
class AssetParameterView;
class EffectStackModel;
class EffectStackView;
class TransitionStackView;
class MixStackView;
class QLabel;

/** @class AssetPanel
    @brief This class is the widget that provides interaction with the asset currently selected.
    That is, it either displays an effectStack or the parameters of a transition
 */
class AssetPanel : public QWidget
{
    Q_OBJECT

public:
    AssetPanel(QWidget *parent);

    /** @brief Shows the parameters of the given transition model */
    void showTransition(int tid, const std::shared_ptr<AssetParameterModel> &transition_model);
    /** @brief Shows the parameters of the given mix model */
    void showMix(int cid, const std::shared_ptr<AssetParameterModel> &transitionModel, bool refreshOnly);

    /** @brief Shows the parameters of the given effect stack model */
    void showEffectStack(const QString &itemName, const std::shared_ptr<EffectStackModel> &effectsModel, QSize frameSize, bool showKeyframes);

    /** @brief Clear the panel so that it doesn't display anything */
    void clear();

    /** @brief This method should be called when the style changes */
    void updatePalette();
    /** @brief Returns the object type / id of effectstack owner */
    ObjectId effectStackOwner();
    /** @brief Add an effect to the current stack owner */
    bool addEffect(const QString &effectId);
    /** @brief Used to pass a standard action like copy or paste to the effect stack widget */
    void sendStandardCommand(int command);

public Q_SLOTS:
    /** @brief Clear panel if displaying itemId */
    void clearAssetPanel(int itemId);
    void assetPanelWarning(const QString &service, const QString &id, const QString &message);
    void deleteCurrentEffect();
    /** @brief Collapse/expand current effect */
    void collapseCurrentEffect();
    void slotCheckWheelEventFilter();
    void slotAddRemoveKeyframe();
    void slotNextKeyframe();
    void slotPreviousKeyframe();
    /** @brief Update timelinbe position in keyframe views */
    void updateAssetPosition(int itemId, const QUuid uuid);

protected:
    /** @brief Return the stylesheet used to display the panel (based on current palette). */
    static const QString getStyleSheet();
    QVBoxLayout *m_lay;
    KSqueezedTextLabel *m_assetTitle;
    QWidget *m_container;
    TransitionStackView *m_transitionWidget;
    MixStackView *m_mixWidget;
    EffectStackView *m_effectStackWidget;

private:
    QToolButton *m_switchBuiltStack;
    QAction *m_applyEffectGroups;
    QToolButton *m_saveEffectStack;
    QComboBox *m_switchCompoButton;
    QAction *m_titleAction;
    QAction *m_switchAction;
    KDualAction *m_splitButton;
    KDualAction *m_enableStackButton;
    KDualAction *m_timelineButton;
    QScrollArea *m_sc;
    KMessageWidget *m_infoMessage;

private Q_SLOTS:
    void processSplitEffect(bool enable);
    /** Displays the owner clip keyframes in timeline */
    void showKeyframes(bool enable);
    /** Enable / disable effect stack */
    void enableStack(bool enable);
    /** Scroll effects view */
    void scrollTo(QRect rect);

Q_SIGNALS:
    void doSplitEffect(bool);
    void doSplitBinEffect(bool);
    void seekToPos(int);
    void reloadEffect(const QString &path);
    void switchCurrentComposition(int tid, const QString &compoId);
    void slotSaveStack();
};
