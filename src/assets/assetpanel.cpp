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

#include "assetpanel.hpp"
#include "core.h"
#include "definitions.h"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "effects/effectstack/view/effectstackview.hpp"
#include "kdenlivesettings.h"
#include "model/assetparametermodel.hpp"
#include "transitions/transitionsrepository.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/view/transitionstackview.hpp"

#include "view/assetparameterview.hpp"

#include <KColorScheme>
#include <KColorUtils>
#include <KDualAction>
#include <KSqueezedTextLabel>
#include <KMessageWidget>
#include <QApplication>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QComboBox>
#include <QFontDatabase>
#include <klocalizedstring.h>

AssetPanel::AssetPanel(QWidget *parent)
    : QWidget(parent)
    , m_lay(new QVBoxLayout(this))
    , m_assetTitle(new KSqueezedTextLabel(this))
    , m_container(new QWidget(this))
    , m_transitionWidget(new TransitionStackView(this))
    , m_effectStackWidget(new EffectStackView(this))
{
    auto *buttonToolbar = new QToolBar(this);
    m_titleAction = buttonToolbar->addWidget(m_assetTitle);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);
    buttonToolbar->setIconSize(iconSize);
    // Edit composition button
    m_switchCompoButton = new QComboBox(this);
    m_switchCompoButton->setFrame(false);
    auto allTransitions = TransitionsRepository::get()->getNames();
    for (const auto &transition : qAsConst(allTransitions)) {
        m_switchCompoButton->addItem(transition.second, transition.first);
    }
    connect(m_switchCompoButton, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&]() {
        if (m_transitionWidget->stackOwner().first == ObjectType::TimelineComposition) {
            emit switchCurrentComposition(m_transitionWidget->stackOwner().second, m_switchCompoButton->currentData().toString());
        }
    });
    m_switchCompoButton->setToolTip(i18n("Change composition type"));
    m_switchAction = buttonToolbar->addWidget(m_switchCompoButton);
    m_switchAction->setVisible(false);

    // spacer
    QWidget *empty = new QWidget();
    empty->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    buttonToolbar->addWidget(empty);

    m_switchBuiltStack = new QToolButton(this);
    m_switchBuiltStack->setIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
    m_switchBuiltStack->setToolTip(i18n("Adjust clip"));
    m_switchBuiltStack->setCheckable(true);
    m_switchBuiltStack->setChecked(KdenliveSettings::showbuiltstack());
    m_switchBuiltStack->setVisible(false);
    // connect(m_switchBuiltStack, &QToolButton::toggled, m_effectStackWidget, &EffectStackView::switchBuiltStack);
    buttonToolbar->addWidget(m_switchBuiltStack);

    m_splitButton = new KDualAction(i18n("Normal view"), i18n("Compare effect"), this);
    m_splitButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("view-right-close")));
    m_splitButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    m_splitButton->setToolTip(i18n("Compare effect"));
    m_splitButton->setVisible(false);
    connect(m_splitButton, &KDualAction::activeChangedByUser, this, &AssetPanel::processSplitEffect);
    buttonToolbar->addAction(m_splitButton);

    m_enableStackButton = new KDualAction(i18n("Effects disabled"), i18n("Effects enabled"), this);
    m_enableStackButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("hint")));
    m_enableStackButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("visibility")));
    connect(m_enableStackButton, &KDualAction::activeChangedByUser, this, &AssetPanel::enableStack);
    m_enableStackButton->setVisible(false);
    buttonToolbar->addAction(m_enableStackButton);

    m_timelineButton = new KDualAction(i18n("Hide keyframes"), i18n("Display keyframes in timeline"), this);
    m_timelineButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
    m_timelineButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
    m_timelineButton->setToolTip(i18n("Display keyframes in timeline"));
    m_timelineButton->setVisible(false);
    connect(m_timelineButton, &KDualAction::activeChangedByUser, this, &AssetPanel::showKeyframes);
    buttonToolbar->addAction(m_timelineButton);

    m_lay->addWidget(buttonToolbar);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);
    auto *lay = new QVBoxLayout(m_container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_transitionWidget);
    lay->addWidget(m_effectStackWidget);
    m_sc = new QScrollArea;
    m_sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_sc->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_sc->setFrameStyle(QFrame::NoFrame);
    m_sc->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
    m_container->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
    m_sc->setWidgetResizable(true);

    m_lay->addWidget(m_sc);
    m_infoMessage = new KMessageWidget(this);
    m_lay->addWidget(m_infoMessage);
    m_infoMessage->hide();
    m_sc->setWidget(m_container);
    m_transitionWidget->setVisible(false);
    m_effectStackWidget->setVisible(false);
    updatePalette();
    connect(m_effectStackWidget, &EffectStackView::checkScrollBar, this, &AssetPanel::slotCheckWheelEventFilter);
    connect(m_effectStackWidget, &EffectStackView::seekToPos, this, &AssetPanel::seekToPos);
    connect(m_effectStackWidget, &EffectStackView::reloadEffect, this, &AssetPanel::reloadEffect);
    connect(m_transitionWidget, &TransitionStackView::seekToTransPos, this, &AssetPanel::seekToPos);
    connect(m_effectStackWidget, &EffectStackView::updateEnabledState, this, [this]() { m_enableStackButton->setActive(m_effectStackWidget->isStackEnabled()); });
}

void AssetPanel::showTransition(int tid, const std::shared_ptr<AssetParameterModel> &transitionModel)
{
    Q_UNUSED(tid)
    ObjectId id = transitionModel->getOwnerId();
    if (m_transitionWidget->stackOwner() == id) {
        // already on this effect stack, do nothing
        return;
    }
    clear();
    QString transitionId = transitionModel->getAssetId();
    m_switchCompoButton->setCurrentIndex(m_switchCompoButton->findData(transitionId));
    m_switchAction->setVisible(true);
    m_titleAction->setVisible(false);
    m_assetTitle->clear();
    m_transitionWidget->setVisible(true);
    m_timelineButton->setVisible(true);
    m_enableStackButton->setVisible(false);
    m_transitionWidget->setModel(transitionModel, QSize(), true);
}

void AssetPanel::showEffectStack(const QString &itemName, const std::shared_ptr<EffectStackModel> &effectsModel, QSize frameSize, bool showKeyframes)
{
    if (effectsModel == nullptr) {
        // Item is not ready
        m_splitButton->setVisible(false);
        m_enableStackButton->setVisible(false);
        clear();
        return;
    }
    ObjectId id = effectsModel->getOwnerId();
    if (m_effectStackWidget->stackOwner() == id) {
        // already on this effect stack, do nothing
        // Disable split effect in case clip was moved
        if (id.first == ObjectType::TimelineClip && m_splitButton->isActive()) {
            m_splitButton->setActive(false);
            processSplitEffect(false);
        }
        return;
    }
    clear();
    QString title;
    bool showSplit = false;
    bool enableKeyframes = false;
    switch (id.first) {
    case ObjectType::TimelineClip:
        title = i18n("%1 effects", itemName);
        showSplit = true;
        enableKeyframes = true;
        break;
    case ObjectType::TimelineComposition:
        title = i18n("%1 parameters", itemName);
        enableKeyframes = true;
        break;
    case ObjectType::TimelineTrack:
        title = i18n("Track %1 effects", itemName);
        // TODO: track keyframes
        // enableKeyframes = true;
        break;
    case ObjectType::BinClip:
        title = i18n("Bin %1 effects", itemName);
        showSplit = true;
        break;
    default:
        title = itemName;
        break;
    }
    m_assetTitle->setText(title);
    m_titleAction->setVisible(true);
    m_splitButton->setVisible(showSplit);
    m_enableStackButton->setVisible(id.first != ObjectType::TimelineComposition);
    m_enableStackButton->setActive(effectsModel->isStackEnabled());
    if (showSplit) {
        m_splitButton->setEnabled(effectsModel->rowCount() > 0);
        QObject::connect(effectsModel.get(), &EffectStackModel::dataChanged, this, [&]() {
            if (m_effectStackWidget->isEmpty()) {
                m_splitButton->setActive(false);
            }
            m_splitButton->setEnabled(!m_effectStackWidget->isEmpty());
        });
    }
    m_timelineButton->setVisible(enableKeyframes);
    m_timelineButton->setActive(showKeyframes);
    // Disable built stack until properly implemented
    // m_switchBuiltStack->setVisible(true);
    m_effectStackWidget->setVisible(true);
    m_effectStackWidget->setModel(effectsModel, frameSize);
}

void AssetPanel::clearAssetPanel(int itemId)
{
    if (itemId == -1) {
        // closing project, reset panel
        clear();
        return;
    }
    ObjectId id = m_effectStackWidget->stackOwner();
    if (id.first == ObjectType::TimelineClip && id.second == itemId) {
        clear();
    } else {
        id = m_transitionWidget->stackOwner();
        if (id.first == ObjectType::TimelineComposition && id.second == itemId) {
            clear();
        }
    }
}

void AssetPanel::clear()
{
    if (m_splitButton->isActive()) {
        m_splitButton->setActive(false);
        processSplitEffect(false);
    }
    m_switchAction->setVisible(false);
    m_transitionWidget->setVisible(false);
    m_transitionWidget->unsetModel();
    m_effectStackWidget->setVisible(false);
    m_splitButton->setVisible(false);
    m_timelineButton->setVisible(false);
    m_switchBuiltStack->setVisible(false);
    m_effectStackWidget->unsetModel();
    m_assetTitle->setText(QString());
}

void AssetPanel::updatePalette()
{
    QString styleSheet = getStyleSheet();
    setStyleSheet(styleSheet);
    m_transitionWidget->setStyleSheet(styleSheet);
    m_effectStackWidget->setStyleSheet(styleSheet);
}

// static
const QString AssetPanel::getStyleSheet()
{
    KColorScheme scheme(QApplication::palette().currentColorGroup(), KColorScheme::View);
    QColor selected_bg = scheme.decoration(KColorScheme::FocusColor).color();
    QColor hgh = KColorUtils::mix(QApplication::palette().window().color(), selected_bg, 0.2);
    QColor hover_bg = scheme.decoration(KColorScheme::HoverColor).color();
    QColor light_bg = scheme.shade(KColorScheme::LightShade);
    QColor alt_bg = scheme.background(KColorScheme::NormalBackground).color();

    QString stylesheet;

    // effect background
    stylesheet.append(QStringLiteral("QFrame#decoframe {border-bottom:2px solid "
                                     "palette(mid);background: transparent} QFrame#decoframe[active=\"true\"] {background: %1;}")
                          .arg(hgh.name()));

    // effect in group background
    stylesheet.append(
        QStringLiteral("QFrame#decoframesub {border-top:1px solid palette(light);}  QFrame#decoframesub[active=\"true\"] {background: %1;}").arg(hgh.name()));

    // group background
    stylesheet.append(QStringLiteral("QFrame#decoframegroup {border:2px solid palette(dark);margin:0px;margin-top:2px;} "));

    // effect title bar
    stylesheet.append(QStringLiteral("QFrame#frame {margin-bottom:2px;}  QFrame#frame[target=\"true\"] "
                                     "{background: palette(highlight);}"));

    // group effect title bar
    stylesheet.append(QStringLiteral("QFrame#framegroup {background: palette(dark);}  "
                                     "QFrame#framegroup[target=\"true\"] {background: palette(highlight);} "));

    // draggable effect bar content
    stylesheet.append(QStringLiteral("QProgressBar::chunk:horizontal {background: palette(button);border-top-left-radius: 4px;border-bottom-left-radius: 4px;} "
                                     "QProgressBar::chunk:horizontal#dragOnly {background: %1;border-top-left-radius: 4px;border-bottom-left-radius: 4px;} "
                                     "QProgressBar::chunk:horizontal:hover {background: %2;}")
                          .arg(alt_bg.name(), selected_bg.name()));

    // draggable effect bar
    stylesheet.append(QStringLiteral("QProgressBar:horizontal {border: 1px solid palette(dark);border-top-left-radius: 4px;border-bottom-left-radius: "
                                     "4px;border-right:0px;background:%3;padding: 0px;text-align:left center} QProgressBar:horizontal:disabled {border: 1px "
                                     "solid palette(button)} QProgressBar:horizontal#dragOnly {background: %3} QProgressBar:horizontal[inTimeline=\"true\"] { "
                                     "border: 1px solid %1;border-right: 0px;background: %2;padding: 0px;text-align:left center } "
                                     "QProgressBar::chunk:horizontal[inTimeline=\"true\"] {background: %1;}")
                          .arg(hover_bg.name(), light_bg.name(), alt_bg.name()));

    // spin box for draggable widget
    stylesheet.append(
        QStringLiteral("QAbstractSpinBox#dragBox {border: 1px solid palette(dark);border-top-right-radius: 4px;border-bottom-right-radius: "
                       "4px;padding-right:0px;} QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox:disabled#dragBox {border: 1px "
                       "solid palette(button);} QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { "
                       "border: 1px solid %1;} QAbstractSpinBox:hover#dragBox {border: 1px solid %2;} ")
            .arg(hover_bg.name(), selected_bg.name()));

    // minimal double edit
    stylesheet.append(
        QStringLiteral("QAbstractSpinBox#dragMinimal {border: 0px "
                       ";padding-right:0px; background-color:transparent} QAbstractSpinBox::down-button#dragMinimal {width:0px;padding:0px;} QAbstractSpinBox:disabled#dragMinimal {border: 0px;; background-color:transparent "
                       ";} QAbstractSpinBox::up-button#dragMinimal {width:0px;padding:0px;}")
            );
    // group editable labels
    stylesheet.append(QStringLiteral("MyEditableLabel { background-color: transparent; color: palette(bright-text); border-radius: 2px;border: 1px solid "
                                     "transparent;} MyEditableLabel:hover {border: 1px solid palette(highlight);} "));

    // transparent qcombobox
    stylesheet.append(QStringLiteral("QComboBox { background-color: transparent;} "));

    return stylesheet;
}

void AssetPanel::processSplitEffect(bool enable)
{
    ObjectType id = m_effectStackWidget->stackOwner().first;
    if (id == ObjectType::TimelineClip) {
        emit doSplitEffect(enable);
    } else if (id == ObjectType::BinClip) {
        emit doSplitBinEffect(enable);
    }
}

void AssetPanel::showKeyframes(bool enable)
{
    if (m_transitionWidget->isVisible()) {
        pCore->showClipKeyframes(m_transitionWidget->stackOwner(), enable);
    } else {
        pCore->showClipKeyframes(m_effectStackWidget->stackOwner(), enable);
    }
}

ObjectId AssetPanel::effectStackOwner()
{
    if (m_transitionWidget->isVisible()) {
        return m_transitionWidget->stackOwner();
    }
    if (!m_effectStackWidget->isVisible()) {
        return ObjectId(ObjectType::NoItem, -1);
    }
    return m_effectStackWidget->stackOwner();
}

bool AssetPanel::addEffect(const QString &effectId)
{
    if (!m_effectStackWidget->isVisible()) {
        return false;
    }
    return m_effectStackWidget->addEffect(effectId);
}

void AssetPanel::enableStack(bool enable)
{
    if (!m_effectStackWidget->isVisible()) {
        return;
    }
    m_effectStackWidget->enableStack(enable);
}

void AssetPanel::deleteCurrentEffect()
{
    if (m_effectStackWidget->isVisible()) {
        emit m_effectStackWidget->removeCurrentEffect();
    }
}

void AssetPanel::collapseCurrentEffect()
{
    if (m_effectStackWidget->isVisible()) {
        m_effectStackWidget->switchCollapsed();
    }
}

void AssetPanel::slotCheckWheelEventFilter()
{
    // If the effect stack widget has no scrollbar, we will not filter the
    // mouse wheel events, so that user can easily adjust effect params
    bool blockWheel = false;
    if (m_sc->verticalScrollBar() && m_sc->verticalScrollBar()->isVisible()) {
        // widget has scroll bar,
        blockWheel = true;
    }
    emit m_effectStackWidget->blockWheenEvent(blockWheel);
}

void AssetPanel::assetPanelWarning(const QString service, const QString /*id*/, const QString message)
{
    QString finalMessage;
    if (!service.isEmpty() && EffectsRepository::get()->exists(service)) {
        QString effectName = EffectsRepository::get()->getName(service);
        if (!effectName.isEmpty()) {
            finalMessage = QStringLiteral("<b>") + effectName + QStringLiteral("</b><br />");
        }
    }
    finalMessage.append(message);
    m_infoMessage->setText(finalMessage);
    m_infoMessage->setWordWrap(message.length() > 35);
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->setMessageType(KMessageWidget::Warning);
    m_infoMessage->animatedShow();
}
