/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assetparameterview.hpp"

#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetcommand.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/abstractparamwidget.hpp"
#include "assets/view/widgets/keyframecontainer.hpp"
#include "core.h"
#include "monitor/monitor.h"

#include <QActionGroup>
#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <utility>

AssetParameterView::AssetParameterView(QWidget *parent)
    : QWidget(parent)

{
    m_lay = new QFormLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 2);
    m_lay->setVerticalSpacing(2);
    m_lay->setHorizontalSpacing(m_lay->horizontalSpacing() * 3);
    // Presets Combo
    m_presetMenu = new QMenu(this);
}

void AssetParameterView::setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer)
{
    unsetModel();
    QMutexLocker lock(&m_lock);
    m_model = model;
    setSizePolicy(QSizePolicy::Preferred, addSpacer ? QSizePolicy::Preferred : QSizePolicy::Fixed);
    const QString paramTag = model->getAssetId();
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    const QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(paramTag));
    connect(this, &AssetParameterView::updatePresets, this, [this, presetFile](const QString &presetName) {
        m_presetMenu->clear();
        m_presetGroup.reset(new QActionGroup(this));
        m_presetGroup->setExclusive(true);
        m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Reset Effect"), this, &AssetParameterView::resetValues);
        // Save preset
        m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save-as-template")), i18n("Save preset…"), this, [&]() { slotSavePreset(); });
        QAction *updatePreset = m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save-as-template")), i18n("Update current preset"), this,
                                                        &AssetParameterView::slotUpdatePreset);
        QAction *deletePreset =
            m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete preset"), this, &AssetParameterView::slotDeleteCurrentPreset);
        m_presetMenu->setWhatsThis(xi18nc("@info:whatsthis", "Deletes the currently selected preset."));
        m_presetMenu->addSeparator();
        QStringList presets = m_model->getPresetList(presetFile);
        if (presetName.isEmpty() || presets.isEmpty()) {
            updatePreset->setEnabled(false);
            deletePreset->setEnabled(false);
        }
        for (const QString &pName : std::as_const(presets)) {
            QAction *ac = m_presetMenu->addAction(pName, this, &AssetParameterView::slotLoadPreset);
            m_presetGroup->addAction(ac);
            ac->setData(pName);
            ac->setCheckable(true);
            if (pName == presetName) {
                ac->setChecked(true);
            }
        }
        m_presetMenu->addSeparator();
        m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), i18n("Save effect"), this, &AssetParameterView::saveEffect);
    });
    Q_EMIT updatePresets();
    connect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);
    // First pass: find and create the keyframe widget for the first animated parameter
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex index = model->index(i, 0);
        auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        if (!m_mainKeyframeWidget && AssetParameterModel::isAnimated(type)) {
            auto paramWidgets = AbstractParamWidget::construct(model, index, frameSize, this, m_lay);
            if (paramWidgets.second) {
                m_mainKeyframeWidget = paramWidgets.second;
                connect(this, &AssetParameterView::initKeyframeView, m_mainKeyframeWidget, &KeyframeContainer::slotInitMonitor);
                connect(m_mainKeyframeWidget, &KeyframeContainer::seekToPos, this, &AssetParameterView::seekToPos);
                connect(m_mainKeyframeWidget, &KeyframeContainer::activateEffect, this, &AssetParameterView::activateEffect);
                connect(m_mainKeyframeWidget, &KeyframeContainer::updateHeight, this, [&]() {
                    setFixedHeight(contentHeight());
                    Q_EMIT updateHeight();
                });
                connect(this, &AssetParameterView::nextKeyframe, m_mainKeyframeWidget, &KeyframeContainer::goToNext);
                connect(this, &AssetParameterView::previousKeyframe, m_mainKeyframeWidget, &KeyframeContainer::goToPrevious);
                connect(this, &AssetParameterView::addRemoveKeyframe, m_mainKeyframeWidget, &KeyframeContainer::addRemove);
                connect(this, &AssetParameterView::sendStandardCommand, m_mainKeyframeWidget, &KeyframeContainer::sendStandardCommand);
                m_mainKeyframeWidget->initNeededSceneAndHelper();
            }
            break;
        }
    }
    // Second pass: add all animated params to the keyframe widget, and all others to the layout
    int nonKeyframeRow = 0;
    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex index = model->index(i, 0);
        auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        if (m_mainKeyframeWidget && (AssetParameterModel::isAnimated(type))) {
            if (type != ParamType::ColorWheel) {
                m_mainKeyframeWidget->addParameter(index);
            }
        } else {
            auto paramWidgets = AbstractParamWidget::construct(model, index, frameSize, this, m_lay);
            AbstractParamWidget *w = paramWidgets.first;
            if (w) {
                connect(this, &AssetParameterView::initKeyframeView, w, &AbstractParamWidget::slotInitMonitor);
                connect(w, &AbstractParamWidget::valueChanged, this, &AssetParameterView::commitChanges);
                connect(w, &AbstractParamWidget::disableCurrentFilter, this, &AssetParameterView::disableCurrentFilter);
                connect(w, &AbstractParamWidget::seekToPos, this, &AssetParameterView::seekToPos);
                connect(w, &AbstractParamWidget::activateEffect, this, &AssetParameterView::activateEffect);
                connect(w, &AbstractParamWidget::updateHeight, this, [&]() {
                    setMinimumHeight(contentHeight());
                    Q_EMIT updateHeight();
                });
                if (type != ParamType::Hidden) {
                    m_lay->insertRow(nonKeyframeRow, w->createLabel(), w);
                    nonKeyframeRow++;
                }
            }
            m_widgets.push_back(w);
        }
    }
    setMinimumHeight(contentHeight());
    // Ensure effect parameters are adjusted to current position
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    Q_EMIT monitor->seekPosition(monitor->position());
}

QVector<QPair<QString, QVariant>> AssetParameterView::getDefaultValues() const
{
    QVector<QPair<QString, QVariant>> values;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i, 0);
        QString name = m_model->data(index, AssetParameterModel::NameRole).toString();
        auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        QVariant defaultValue = m_model->data(index, AssetParameterModel::DefaultRole);
        if (AssetParameterModel::isAnimated(type) && type != ParamType::Roto_spline) {
            // Roto_spline keyframes are stored as JSON so do not apply this to roto
            QString val = defaultValue.toString();
            if (!val.contains(QLatin1Char('='))) {
                val.prepend(QStringLiteral("%1=").arg(m_model->data(index, AssetParameterModel::ParentInRole).toInt()));
                defaultValue = QVariant(val);
            }
        }
        values.append({name, defaultValue});
    }
    return values;
}

void AssetParameterView::resetValues()
{
    const QVector<QPair<QString, QVariant>> values = getDefaultValues();
    auto *command = new AssetUpdateCommand(m_model, values);
    if (m_model->getOwnerId().itemId != -1) {
        pCore->pushUndo(command);
    } else {
        command->redo();
        delete command;
    }
    // Unselect preset if any
    QAction *ac = m_presetGroup->checkedAction();
    if (ac) {
        ac->setChecked(false);
    }
}

void AssetParameterView::disableCurrentFilter(bool disable)
{
    m_model->setParameter(QStringLiteral("disable"), disable ? 1 : 0, true);
}

void AssetParameterView::commitChanges(const QModelIndex &index, const QString &value, bool storeUndo)
{
    // Warning: please note that some widgets (for example keyframes) do NOT send the valueChanged signal and do modifications on their own
    const QString previousValue = m_model->data(index, AssetParameterModel::ValueRole).toString();
    auto *command = new AssetCommand(m_model, index, value);
    if (storeUndo && m_model->getOwnerId().itemId != -1) {
        if (m_model->getOwnerId().type == KdenliveObjectType::TimelineClip || m_model->getOwnerId().type == KdenliveObjectType::BinClip) {
            pCore->groupAssetCommand(m_model->getOwnerId(), m_model->getAssetId(), index, previousValue, value, command);
        }
        pCore->pushUndo(command);
    } else {
        command->redo();
        delete command;
    }
}

void AssetParameterView::commitMultipleChanges(const QList<QModelIndex> &indexes, const QStringList &values, bool storeUndo)
{
    // Warning: please note that some widgets (for example keyframes) do NOT send the valueChanged signal and do modifications on their own
    auto *command = new AssetMultiCommand(m_model, indexes, values);
    if (storeUndo) {
        pCore->pushUndo(command);
    } else {
        command->redo();
        delete command;
    }
}

void AssetParameterView::disconnectKeyframeWidget()
{
    if (m_mainKeyframeWidget) {
        disconnect(this, &AssetParameterView::initKeyframeView, m_mainKeyframeWidget, &KeyframeContainer::slotInitMonitor);
        disconnect(m_mainKeyframeWidget, &KeyframeContainer::seekToPos, this, &AssetParameterView::seekToPos);
        disconnect(m_mainKeyframeWidget, &KeyframeContainer::activateEffect, this, &AssetParameterView::activateEffect);

        disconnect(this, &AssetParameterView::nextKeyframe, m_mainKeyframeWidget, &KeyframeContainer::goToNext);
        disconnect(this, &AssetParameterView::previousKeyframe, m_mainKeyframeWidget, &KeyframeContainer::goToPrevious);
        disconnect(this, &AssetParameterView::addRemoveKeyframe, m_mainKeyframeWidget, &KeyframeContainer::addRemove);
        disconnect(this, &AssetParameterView::sendStandardCommand, m_mainKeyframeWidget, &KeyframeContainer::sendStandardCommand);
    }
}

void AssetParameterView::unsetModel()
{
    QMutexLocker lock(&m_lock);
    if (m_model) {
        // if a model is already there, we have to disconnect signals first
        disconnect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);
    }
    delete m_mainKeyframeWidget;
    m_mainKeyframeWidget = nullptr;
    // Delete widgets
    while (m_lay->rowCount() > 0) {
        m_lay->removeRow(0);
    }
    // clear layout
    m_widgets.clear();

    // Release ownership of smart pointer
    m_model.reset();
}

void AssetParameterView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    QMutexLocker lock(&m_lock);
    if (m_widgets.size() == 0 && !m_mainKeyframeWidget) {
        // no visible param for this asset, abort
        return;
    }
    Q_UNUSED(roles);
    // We are expecting indexes that are children of the root index, which is "invalid"
    Q_ASSERT(!topLeft.parent().isValid());
    // We make sure the range is valid
    auto type = m_model->data(m_model->index(topLeft.row(), 0), AssetParameterModel::TypeRole).value<ParamType>();
    if (type == ParamType::ColorWheel) {
        // Some special widgets, like colorwheel handle multiple params so we can have cases where param index row is greater than the number of widgets.
        // Should be better managed
        if (!m_widgets.empty() && m_widgets.at(0)) {
            m_widgets.at(0)->slotRefresh();
        } else if (m_mainKeyframeWidget) {
            m_mainKeyframeWidget->slotRefresh();
        }
        return;
    }
    if (!m_widgets.empty()) {
        size_t max = m_widgets.size() - 1;
        if (bottomRight.isValid()) {
            max = qMin(max, size_t(bottomRight.row()));
        }
        for (auto i = size_t(topLeft.row()); i <= max; ++i) {
            if (m_widgets.at(i)) {
                m_widgets.at(i)->slotRefresh();
            }
        }
    }
    if (m_mainKeyframeWidget) {
        m_mainKeyframeWidget->slotRefresh();
    }
}

int AssetParameterView::contentHeight() const
{
    return m_lay->minimumSize().height();
}

MonitorSceneType AssetParameterView::needsMonitorEffectScene() const
{
    if (m_mainKeyframeWidget) {
        return m_mainKeyframeWidget->requiredScene();
    }
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i, 0);
        auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::Geometry) {
            return MonitorSceneGeometry;
        }
    }
    return MonitorSceneDefault;
}

void AssetParameterView::slotRefresh()
{
    refresh(m_model->index(0, 0), m_model->index(m_model->rowCount() - 1, 0), {});
}

bool AssetParameterView::keyframesAllowed() const
{
    return m_mainKeyframeWidget != nullptr;
}

bool AssetParameterView::hasMultipleKeyframes() const
{
    if (m_model->getKeyframeModel()) {
        return !m_model->getKeyframeModel()->singleKeyframe();
    }
    return false;
}

bool AssetParameterView::modelHideKeyframes() const
{
    return m_mainKeyframeWidget != nullptr && !m_mainKeyframeWidget->keyframesVisible();
}

void AssetParameterView::toggleKeyframes(bool enable)
{
    if (m_mainKeyframeWidget) {
        m_mainKeyframeWidget->showKeyframes(enable);
        setFixedHeight(contentHeight());
        Q_EMIT updateHeight();
    }
}

void AssetParameterView::slotDeleteCurrentPreset()
{
    QAction *ac = m_presetGroup->checkedAction();
    if (!ac) {
        return;
    }
    slotDeletePreset(ac->data().toString());
}

void AssetParameterView::slotDeletePreset(const QString &presetName)
{
    if (presetName.isEmpty()) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    if (dir.exists()) {
        const QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(m_model->getAssetId()));
        m_model->deletePreset(presetFile, presetName);
        Q_EMIT updatePresets();
    }
}

void AssetParameterView::slotUpdatePreset()
{
    QAction *ac = m_presetGroup->checkedAction();
    if (!ac) {
        return;
    }
    slotSavePreset(ac->data().toString());
}

void AssetParameterView::slotSavePreset(QString presetName)
{
    if (presetName.isEmpty()) {
        bool ok;
        presetName =
            QInputDialog::getText(this, i18nc("@title:window", "Enter Preset Name"), i18n("Enter the name of this preset:"), QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    const QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(m_model->getAssetId()));
    m_model->savePreset(presetFile, presetName);
    Q_EMIT updatePresets(presetName);
}

void AssetParameterView::slotLoadPreset()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action) {
        return;
    }
    const QString presetName = action->data().toString();
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    const QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(m_model->getAssetId()));
    const QVector<QPair<QString, QVariant>> params = m_model->loadPreset(presetFile, presetName);
    auto *command = new AssetUpdateCommand(m_model, params);
    pCore->pushUndo(command);
    Q_EMIT updatePresets(presetName);
}

QMenu *AssetParameterView::presetMenu()
{
    return m_presetMenu;
}
