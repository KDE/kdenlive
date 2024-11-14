/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assetpanel.hpp"
#include "core.h"
#include "definitions.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "effects/effectstack/view/effectstackview.hpp"
#include "kdenlivesettings.h"
#include "model/assetparametermodel.hpp"
#include "monitor/monitor.h"
#include "transitions/transitionsrepository.hpp"
#include "transitions/view/mixstackview.hpp"
#include "transitions/view/transitionstackview.hpp"

#include "view/assetparameterview.hpp"

#include <KColorScheme>
#include <KColorUtils>
#include <KDualAction>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KSqueezedTextLabel>
#include <QApplication>
#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

AssetPanel::AssetPanel(QWidget *parent)
    : QWidget(parent)
    , m_lay(new QVBoxLayout(this))
    , m_assetTitle(new KSqueezedTextLabel(this))
    , m_container(new QWidget(this))
    , m_transitionWidget(new TransitionStackView(this))
    , m_mixWidget(new MixStackView(this))
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
    for (const auto &transition : std::as_const(allTransitions)) {
        if (transition.first != QLatin1String("mix")) {
            m_switchCompoButton->addItem(transition.second, transition.first);
        }
    }
    connect(m_switchCompoButton, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [&]() {
        if (m_transitionWidget->stackOwner().type == KdenliveObjectType::TimelineComposition) {
            Q_EMIT switchCurrentComposition(m_transitionWidget->stackOwner().itemId, m_switchCompoButton->currentData().toString());
        } else if (m_mixWidget->isVisible()) {
            Q_EMIT switchCurrentComposition(m_mixWidget->stackOwner().itemId, m_switchCompoButton->currentData().toString());
        }
    });
    m_switchCompoButton->setToolTip(i18n("Change composition type"));
    m_switchAction = buttonToolbar->addWidget(m_switchCompoButton);
    m_switchAction->setVisible(false);

    // spacer
    QWidget *empty = new QWidget();
    empty->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    buttonToolbar->addWidget(empty);

    m_switchBuiltStack = new QAction(QIcon::fromTheme(QStringLiteral("adjustlevels")), QString(), this);
    m_switchBuiltStack->setToolTip(i18n("Enable Builtin Effects"));
    m_switchBuiltStack->setWhatsThis(xi18nc(
        "@info:whatsthis", "When enabled, this will add basic effects (Flip, Transform, Volume) to all clips for convenience. These effects are disabled "
                           "and will only be applied if you change a parameter."));
    m_switchBuiltStack->setCheckable(true);
    m_switchBuiltStack->setChecked(KdenliveSettings::enableBuiltInEffects());
    connect(m_switchBuiltStack, &QAction::triggered, this, [this](bool enable) {
        KdenliveSettings::setEnableBuiltInEffects(enable);
        if (m_effectStackWidget) {
            ObjectId owner = m_effectStackWidget->stackOwner();
            pCore->clearAssetPanel(-1);
            m_effectStackWidget->unsetModel();
            pCore->showEffectStackFromId(owner);
        } else {
            pCore->clearAssetPanel(-1);
        }
    });
    buttonToolbar->addAction(m_switchBuiltStack);
    m_switchBuiltStack->setVisible(false);

    m_applyEffectGroups = new QMenu(this);
    m_applyEffectGroups->setIcon(QIcon::fromTheme(QStringLiteral("link")));
    QAction *applyToSameOnly = new QAction(i18n("Apply only to effects with same value"), this);
    applyToSameOnly->setCheckable(true);
    applyToSameOnly->setChecked(KdenliveSettings::applyEffectParamsToGroupWithSameValue());
    connect(applyToSameOnly, &QAction::toggled, this, [](bool enabled) { KdenliveSettings::setApplyEffectParamsToGroupWithSameValue(enabled); });
    m_applyEffectGroups->addAction(applyToSameOnly);
    m_applyEffectGroups->menuAction()->setCheckable(true);
    m_applyEffectGroups->menuAction()->setToolTip(i18n("Apply effect change to all clips in the group…"));
    m_applyEffectGroups->menuAction()->setWhatsThis(
        xi18nc("@info:whatsthis", "When enabled and the clip is in a group, all clips in this group that have the same effect will see the parameters adjusted "
                                  "as well. Deleting an effect will delete it in all clips in the group."));
    buttonToolbar->addAction(m_applyEffectGroups->menuAction());
    m_applyEffectGroups->menuAction()->setChecked(KdenliveSettings::applyEffectParamsToGroup());
    connect(m_applyEffectGroups->menuAction(), &QAction::triggered, this, [this]() {
        KdenliveSettings::setApplyEffectParamsToGroup(m_applyEffectGroups->menuAction()->isChecked());
        Q_EMIT m_effectStackWidget->updateEffectsGroupesInstances();
    });
    m_applyEffectGroups->menuAction()->setVisible(false);
    m_saveEffectStack = new QAction(QIcon::fromTheme(QStringLiteral("document-save-all")), QString(), this);
    m_saveEffectStack->setToolTip(i18n("Save Effect Stack…"));
    m_saveEffectStack->setWhatsThis(xi18nc("@info:whatsthis", "Saves the entire effect stack as an XML file for use in other projects."));
    connect(m_saveEffectStack, &QAction::triggered, this, &AssetPanel::slotSaveStack);
    buttonToolbar->addAction(m_saveEffectStack);
    m_saveEffectStack->setVisible(false);

    m_splitButton = new KDualAction(i18n("Normal view"), i18n("Compare effect"), this);
    m_splitButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("view-right-close")));
    m_splitButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    m_splitButton->setCheckable(true);
    m_splitButton->setToolTip(i18n("Compare effect"));
    m_splitButton->setWhatsThis(xi18nc(
        "@info:whatsthis",
        "Turns on or off the split view in the project and/or clip monitor: on the left the clip with effect is shown, on the right the clip without effect."));
    m_splitButton->setVisible(false);
    connect(m_splitButton, &KDualAction::activeChangedByUser, this, &AssetPanel::processSplitEffect);
    buttonToolbar->addAction(m_splitButton);

    m_enableStackButton = new KDualAction(i18n("Effects disabled"), i18n("Effects enabled"), this);
    m_enableStackButton->setWhatsThis(
        xi18nc("@info:whatsthis",
               "Toggles the effect stack to be enabled or disabled. Useful to see the difference between original and edited or to speed up scrubbing."));
    m_enableStackButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("hint")));
    m_enableStackButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("visibility")));
    connect(m_enableStackButton, &KDualAction::activeChangedByUser, this, &AssetPanel::enableStack);
    m_enableStackButton->setVisible(false);
    buttonToolbar->addAction(m_enableStackButton);
    m_compositionHelpLink = new QAction(QIcon::fromTheme(QStringLiteral("help-about")), QString(), this);
    m_compositionHelpLink->setToolTip(i18n("Open composition documentation in browser"));
    buttonToolbar->addAction(m_compositionHelpLink);
    connect(m_compositionHelpLink, &QAction::triggered, this, [this]() { m_transitionWidget->openCompositionHelp(); });
    m_compositionHelpLink->setVisible(false);
    m_timelineButton = new KDualAction(i18n("Display keyframes in timeline"), i18n("Hide keyframes in timeline"), this);
    m_timelineButton->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of keyframes in the clip on the timeline"));
    m_timelineButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("keyframe-disable")));
    m_timelineButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("keyframe")));
    m_timelineButton->setVisible(false);
    connect(m_timelineButton, &KDualAction::activeChangedByUser, this, &AssetPanel::showKeyframes);
    buttonToolbar->addAction(m_timelineButton);

    m_lay->addWidget(buttonToolbar);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);
    auto *lay = new QVBoxLayout(m_container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_transitionWidget);
    lay->addWidget(m_mixWidget);
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
    m_mixWidget->setVisible(false);
    m_effectStackWidget->setVisible(false);
    updatePalette();
    connect(m_effectStackWidget, &EffectStackView::checkScrollBar, this, &AssetPanel::slotCheckWheelEventFilter);
    connect(m_effectStackWidget, &EffectStackView::scrollView, this, &AssetPanel::scrollTo);
    connect(m_effectStackWidget, &EffectStackView::checkDragScrolling, this, &AssetPanel::checkDragScroll);
    connect(m_effectStackWidget, &EffectStackView::seekToPos, this, &AssetPanel::seekToPos);
    connect(m_effectStackWidget, &EffectStackView::reloadEffect, this, &AssetPanel::reloadEffect);
    connect(m_transitionWidget, &TransitionStackView::seekToTransPos, this, &AssetPanel::seekToPos);
    connect(m_mixWidget, &MixStackView::seekToTransPos, this, &AssetPanel::seekToPos);
    connect(m_effectStackWidget, &EffectStackView::updateEnabledState, this,
            [this]() { m_enableStackButton->setActive(m_effectStackWidget->isStackEnabled()); });

    connect(this, &AssetPanel::slotSaveStack, m_effectStackWidget, &EffectStackView::slotSaveStack);
    m_dragScrollTimer.setSingleShot(true);
    m_dragScrollTimer.setInterval(200);
    connect(&m_dragScrollTimer, &QTimer::timeout, this, &AssetPanel::checkDragScroll);
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
    m_switchCompoButton->setCurrentIndex(m_switchCompoButton->findData(transitionModel->getAssetId()));
    m_switchAction->setVisible(true);
    m_titleAction->setVisible(false);
    m_applyEffectGroups->menuAction()->setVisible(false);
    m_assetTitle->clear();
    m_transitionWidget->setVisible(true);
    m_compositionHelpLink->setVisible(true);
    m_timelineButton->setVisible(true);
    m_enableStackButton->setVisible(false);
    QSize s = pCore->getCompositionSizeOnTrack(id);
    m_transitionWidget->setModel(transitionModel, s, true);
}

void AssetPanel::showMix(int cid, const std::shared_ptr<AssetParameterModel> &transitionModel, bool refreshOnly)
{
    if (cid == -1) {
        clear();
        return;
    }
    ObjectId id(KdenliveObjectType::TimelineMix, cid, transitionModel->getOwnerId().uuid);
    if (refreshOnly) {
        if (m_mixWidget->stackOwner() != id) {
            // item not currently displayed, ignore
            return;
        }
    } else {
        if (m_mixWidget->stackOwner() == id) {
            // already on this effect stack, do nothing
            return;
        }
    }
    clear();
    // There is only 1 audio composition, so hide switch combobox
    m_switchAction->setVisible(transitionModel->getAssetId() != QLatin1String("mix"));
    m_titleAction->setVisible(false);
    m_assetTitle->clear();
    m_mixWidget->setVisible(true);
    m_compositionHelpLink->setVisible(false);
    m_timelineButton->setVisible(false);
    m_applyEffectGroups->menuAction()->setVisible(false);
    m_enableStackButton->setVisible(false);
    m_switchCompoButton->setCurrentIndex(m_switchCompoButton->findData(transitionModel->getAssetId()));
    m_mixWidget->setModel(transitionModel, QSize(), true);
}

void AssetPanel::showEffectStack(const QString &itemName, const std::shared_ptr<EffectStackModel> &effectsModel, QSize frameSize, bool showKeyframes)
{
    if (effectsModel == nullptr) {
        // Item is not ready
        m_splitButton->setVisible(false);
        m_enableStackButton->setVisible(false);
        m_saveEffectStack->setVisible(false);
        m_switchBuiltStack->setVisible(false);
        m_applyEffectGroups->menuAction()->setVisible(false);
        clear();
        return;
    }
    ObjectId id = effectsModel->getOwnerId();
    if (m_effectStackWidget->stackOwner() == id) {
        // already on this effect stack, do nothing
        // Disable split effect in case clip was moved
        if (id.type == KdenliveObjectType::TimelineClip && m_splitButton->isActive()) {
            m_splitButton->setActive(false);
            processSplitEffect(false);
        }
        return;
    }
    clear();
    QString title;
    bool showSplit = false;
    bool enableKeyframes = false;
    switch (id.type) {
    case KdenliveObjectType::TimelineClip:
        title = i18n("%1 effects", itemName);
        showSplit = true;
        enableKeyframes = true;
        break;
    case KdenliveObjectType::TimelineComposition:
        title = i18n("%1 parameters", itemName);
        enableKeyframes = true;
        break;
    case KdenliveObjectType::TimelineTrack:
        title = i18n("Track %1 effects", itemName);
        // TODO: track keyframes
        // enableKeyframes = true;
        break;
    case KdenliveObjectType::BinClip:
        title = i18n("Bin %1 effects", itemName);
        showSplit = true;
        break;
    default:
        title = itemName;
        break;
    }
    m_assetTitle->setText(title);
    m_titleAction->setVisible(true);
    m_applyEffectGroups->menuAction()->setVisible(true);
    m_splitButton->setVisible(showSplit);
    m_saveEffectStack->setVisible(true);
    m_switchBuiltStack->setVisible(true);
    m_enableStackButton->setVisible(id.type != KdenliveObjectType::TimelineComposition);
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
    m_compositionHelpLink->setVisible(false);
    m_timelineButton->setVisible(enableKeyframes);
    m_timelineButton->setActive(showKeyframes);
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
    if (id.type == KdenliveObjectType::TimelineClip && id.itemId == itemId) {
        clear();
        return;
    }
    id = m_transitionWidget->stackOwner();
    if (id.type == KdenliveObjectType::TimelineComposition && id.itemId == itemId) {
        clear();
        return;
    }
    id = m_mixWidget->stackOwner();
    if (id.type == KdenliveObjectType::TimelineMix && id.itemId == itemId) {
        clear();
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
    m_mixWidget->setVisible(false);
    m_mixWidget->unsetModel();
    m_effectStackWidget->setVisible(false);
    m_splitButton->setVisible(false);
    m_saveEffectStack->setVisible(false);
    m_switchBuiltStack->setVisible(false);
    m_compositionHelpLink->setVisible(false);
    m_timelineButton->setVisible(false);
    m_switchBuiltStack->setVisible(false);
    m_effectStackWidget->unsetModel();
    m_assetTitle->clear();
}

void AssetPanel::updatePalette()
{
    QString styleSheet = getStyleSheet();
    setStyleSheet(styleSheet);
    m_transitionWidget->setStyleSheet(styleSheet);
    m_effectStackWidget->setStyleSheet(styleSheet);
    m_mixWidget->setStyleSheet(styleSheet);
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
    // QColor normal_bg = QApplication::palette().window().color();

    QString stylesheet;

    // effect background
    stylesheet.append(QStringLiteral("QFrame#decoframe {border-top:2px solid "
                                     "palette(shadow);background: transparent} QFrame#decoframe.builtin {border-top:0px solid "
                                     "palette(shadow);border-bottom:2px solid "
                                     "#33FF0000 ;background: transparent} QFrame#decoframe[target=\"true\"] {border-top:2px solid %2;"
                                     "background: transparent} QFrame#decoframe[active=\"true\"] {background: %1;} "
                                     "QFrame#decoframe.builtin[active=\"true\"] {background: %1;}")
                          .arg(hgh.name(), hover_bg.name()));

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
        QStringLiteral("QAbstractSpinBox#dragBox {border-width: 2px; border-style: solid; border-radius: 4px; border-color: transparent }"
                       "QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox:disabled#dragBox {border: 1px} "
                       "solid palette(button);} QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { "
                       "border: 1px solid %1;} QAbstractSpinBox:hover#dragBox {border: 1px solid %2;border-radius: 4px} ")
            .arg(hover_bg.name(), selected_bg.name()));

    // minimal double edit
    stylesheet.append(QStringLiteral("QAbstractSpinBox#dragMinimal {border: 0px "
                                     ";padding-right:0px; background-color:transparent} QAbstractSpinBox::down-button#dragMinimal {width:0px;padding:0px;} "
                                     "QAbstractSpinBox:disabled#dragMinimal {border: 0px;; background-color:transparent "
                                     ";} QAbstractSpinBox::up-button#dragMinimal {width:0px;padding:0px;}"));
    // group editable labels
    stylesheet.append(QStringLiteral("MyEditableLabel { background-color: transparent; color: palette(bright-text); border-radius: 2px;border: 1px solid "
                                     "transparent;} MyEditableLabel:hover {border: 1px solid palette(highlight);} "));
    return stylesheet;
}

void AssetPanel::processSplitEffect(bool enable)
{
    KdenliveObjectType id = m_effectStackWidget->stackOwner().type;
    if (id == KdenliveObjectType::TimelineClip) {
        Q_EMIT doSplitEffect(enable);
    } else if (id == KdenliveObjectType::BinClip) {
        Q_EMIT doSplitBinEffect(enable);
    }
}

void AssetPanel::showKeyframes(bool enable)
{
    if (m_effectStackWidget->isVisible()) {
        pCore->showClipKeyframes(m_effectStackWidget->stackOwner(), enable);
    } else if (m_transitionWidget->isVisible()) {
        pCore->showClipKeyframes(m_transitionWidget->stackOwner(), enable);
    }
}

ObjectId AssetPanel::effectStackOwner()
{
    if (m_effectStackWidget->isVisible()) {
        return m_effectStackWidget->stackOwner();
    }
    if (m_transitionWidget->isVisible()) {
        return m_transitionWidget->stackOwner();
    }
    if (m_mixWidget->isVisible()) {
        return m_mixWidget->stackOwner();
    }
    return ObjectId();
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
        Q_EMIT m_effectStackWidget->removeCurrentEffect();
    }
}

void AssetPanel::collapseCurrentEffect()
{
    if (m_effectStackWidget->isVisible()) {
        m_effectStackWidget->switchCollapsed();
    }
}

void AssetPanel::scrollTo(QRect rect)
{
    // Ensure the scrollview widget adapted its height to the effectstackview height change
    m_sc->widget()->adjustSize();
    if (rect.height() < m_sc->height()) {
        m_sc->ensureVisible(0, rect.y() + rect.height(), 0, 0);
    } else {
        m_sc->ensureVisible(0, rect.y() + m_sc->height(), 0, 0);
    }
}

void AssetPanel::checkDragScroll()
{
    if (m_effectStackWidget->dragPos.isNull()) {
        return;
    }
    int dragYPos = m_effectStackWidget->dragPos.y();
    int mousePos = m_effectStackWidget->mapTo(m_sc, m_effectStackWidget->dragPos).y();
    int viewPos = m_sc->verticalScrollBar()->value();
    if (viewPos > 0 && mousePos < 15) {
        m_sc->verticalScrollBar()->setValue(qMax(0, viewPos - m_sc->verticalScrollBar()->singleStep()));
        viewPos -= m_sc->verticalScrollBar()->value();
        m_effectStackWidget->dragPos.setY(dragYPos - viewPos);
        m_dragScrollTimer.start();
    } else if (m_sc->height() - mousePos < 15) {
        m_sc->verticalScrollBar()->setValue(viewPos + m_sc->verticalScrollBar()->singleStep());
        viewPos -= m_sc->verticalScrollBar()->value();
        m_effectStackWidget->dragPos.setY(dragYPos - viewPos);
        m_dragScrollTimer.start();
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
    Q_EMIT m_effectStackWidget->blockWheelEvent(blockWheel);
}

void AssetPanel::assetPanelWarning(const QString &service, const QString & /*id*/, const QString &message)
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

void AssetPanel::slotAddRemoveKeyframe()
{
    if (m_effectStackWidget->isVisible()) {
        m_effectStackWidget->addRemoveKeyframe();
    } else if (m_transitionWidget->isVisible()) {
        Q_EMIT m_transitionWidget->addRemoveKeyframe();
    } else if (m_mixWidget->isVisible()) {
        Q_EMIT m_mixWidget->addRemoveKeyframe();
    }
}

void AssetPanel::slotNextKeyframe()
{
    if (m_effectStackWidget->isVisible()) {
        m_effectStackWidget->slotGoToKeyframe(true);
    } else if (m_transitionWidget->isVisible()) {
        Q_EMIT m_transitionWidget->nextKeyframe();
    } else if (m_mixWidget->isVisible()) {
        Q_EMIT m_mixWidget->nextKeyframe();
    }
}

void AssetPanel::slotPreviousKeyframe()
{
    if (m_effectStackWidget->isVisible()) {
        m_effectStackWidget->slotGoToKeyframe(false);
    } else if (m_transitionWidget->isVisible()) {
        Q_EMIT m_transitionWidget->previousKeyframe();
    } else if (m_mixWidget->isVisible()) {
        Q_EMIT m_mixWidget->previousKeyframe();
    }
}

void AssetPanel::updateAssetPosition(int itemId, const QUuid uuid)
{
    if (m_effectStackWidget->isVisible()) {
        ObjectId id(KdenliveObjectType::TimelineClip, itemId, uuid);
        if (m_effectStackWidget->stackOwner() == id) {
            Q_EMIT pCore->getMonitor(Kdenlive::ProjectMonitor)->seekPosition(pCore->getMonitorPosition());
        }
    } else if (m_transitionWidget->isVisible()) {
        ObjectId id(KdenliveObjectType::TimelineComposition, itemId, uuid);
        if (m_transitionWidget->stackOwner() == id) {
            Q_EMIT pCore->getMonitor(Kdenlive::ProjectMonitor)->seekPosition(pCore->getMonitorPosition());
        }
    }
}

void AssetPanel::sendStandardCommand(int command)
{
    if (m_effectStackWidget->isVisible()) {
        m_effectStackWidget->sendStandardCommand(command);
    } else if (m_transitionWidget->isVisible()) {
        Q_EMIT m_transitionWidget->sendStandardCommand(command);
    }
}
