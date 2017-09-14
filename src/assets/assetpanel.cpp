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
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "effects/effectstack/view/effectstackview.hpp"
#include "transitions/view/transitionstackview.hpp"
#include "kdenlivesettings.h"
#include "model/assetparametermodel.hpp"
#include "transitions/transitionsrepository.hpp"
#include "view/assetparameterview.hpp"
#include "utils/KoIconUtils.h"
#include "definitions.h"

#include <KColorScheme>
#include <KColorUtils>
#include <KSqueezedTextLabel>
#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <klocalizedstring.h>

AssetPanel::AssetPanel(QWidget *parent)
    : QScrollArea(parent)
    , m_lay(new QVBoxLayout(this))
    , m_assetTitle(new KSqueezedTextLabel(this))
    , m_transitionWidget(new TransitionStackView(this))
    , m_effectStackWidget(new EffectStackView(this))
{
    QHBoxLayout *tLayout = new QHBoxLayout;
    tLayout->addWidget(m_assetTitle);
    m_switchBuiltStack = new QToolButton(this);
    m_switchBuiltStack->setIcon(KoIconUtils::themedIcon(QStringLiteral("adjustlevels")));
    m_switchBuiltStack->setToolTip(i18n("Adjust clip"));
    m_switchBuiltStack->setCheckable(true);
    m_switchBuiltStack->setChecked(KdenliveSettings::showbuiltstack());
    m_switchBuiltStack->setVisible(false);
    connect(m_switchBuiltStack, &QToolButton::toggled, m_effectStackWidget, &EffectStackView::switchBuiltStack);
    tLayout->addWidget(m_switchBuiltStack);

    m_splitButton = new QToolButton(this);
    m_splitButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-split-left-right")));
    m_splitButton->setToolTip(i18n("Compare effect"));
    m_splitButton->setCheckable(true);
    m_splitButton->setVisible(false);
    connect(m_splitButton, &QToolButton::toggled, this, &AssetPanel::processSplitEffect);
    tLayout->addWidget(m_splitButton);
    m_lay->addLayout(tLayout);
    m_lay->addWidget(m_transitionWidget);
    m_lay->addWidget(m_effectStackWidget);
    m_transitionWidget->setVisible(false);
    m_effectStackWidget->setVisible(false);
    updatePalette();
    connect(m_effectStackWidget, &EffectStackView::seekToPos, this, &AssetPanel::seekToPos);
}

void AssetPanel::showTransition(int tid, std::shared_ptr<AssetParameterModel> transitionModel)
{
    clear();
    QString transitionId = transitionModel->getAssetId();
    m_transitionWidget->setProperty("compositionId", tid);
    QString transitionName = TransitionsRepository::get()->getName(transitionId);
    m_assetTitle->setText(i18n("Properties of transition %1", transitionName));
    m_transitionWidget->setVisible(true);
    m_transitionWidget->setModel(transitionModel, QPair<int, int>(-1, -1), true);
}

void AssetPanel::showEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel> effectsModel, QPair<int, int> range)
{
    clear();
    if (effectsModel == nullptr) {
        // Item is not ready
        return;
    }
    m_assetTitle->setText(i18n("%1 effects", clipName));
    m_splitButton->setVisible(true);
    m_switchBuiltStack->setVisible(true);
    m_effectStackWidget->setVisible(true);
    m_effectStackWidget->setModel(effectsModel, range);
}

void AssetPanel::clearAssetPanel(int itemId)
{
    if (itemId == -1 || m_transitionWidget->property("compositionId").toInt() == itemId || m_effectStackWidget->property("clipId").toInt() == itemId) {
        clear();
    }
}

void AssetPanel::adjustAssetPanelRange(int itemId, int in, int out)
{
    if (m_effectStackWidget->property("clipId").toInt() == itemId) {
        m_effectStackWidget->setRange(in, out);
    }
}

void AssetPanel::clear()
{
    m_transitionWidget->setVisible(false);
    m_transitionWidget->setProperty("compositionId", QVariant());
    m_transitionWidget->unsetModel();
    m_effectStackWidget->setVisible(false);
    m_splitButton->setVisible(false);
    m_switchBuiltStack->setVisible(false);
    m_effectStackWidget->setProperty("clipId", QVariant());
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
    KColorScheme scheme(QApplication::palette().currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
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

ObjectId AssetPanel::effectStackOwner()
{
    if (!m_effectStackWidget->isVisible()) {
        return ObjectId(ObjectType::NoItem, -1);
    }
    return m_effectStackWidget->stackOwner();
}

void AssetPanel::parameterChanged(QString name, int value)
{
    emit changeSpeed(value);
}
