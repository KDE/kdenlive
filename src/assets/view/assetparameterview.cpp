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

#include "assetparameterview.hpp"

#include "assets/model/assetcommand.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/abstractparamwidget.hpp"
#include "assets/view/widgets/keyframewidget.hpp"
#include "core.h"

#include <QActionGroup>
#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMenu>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <utility>

AssetParameterView::AssetParameterView(QWidget *parent)
    : QWidget(parent)

{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 2);
    m_lay->setSpacing(0);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
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
    const QString presetFile = dir.absoluteFilePath(QString("%1.json").arg(paramTag));
    connect(this, &AssetParameterView::updatePresets, this, [this, presetFile](const QString &presetName) {
        m_presetMenu->clear();
        m_presetGroup.reset(new QActionGroup(this));
        m_presetGroup->setExclusive(true);
        m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18n("Reset Effect"), this, SLOT(resetValues()));
        // Save preset
        m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save-as-template")), i18n("Save preset"), this, SLOT(slotSavePreset()));
        QAction *updatePreset = m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("document-save-as-template")), i18n("Update current preset"), this, SLOT(slotUpdatePreset()));
        QAction *deletePreset = m_presetMenu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete preset"), this, SLOT(slotDeletePreset()));
        m_presetMenu->addSeparator();
        QStringList presets = m_model->getPresetList(presetFile);
        if (presetName.isEmpty() || presets.isEmpty()) {
            updatePreset->setEnabled(false);
            deletePreset->setEnabled(false);
        }
        for (const QString &pName : qAsConst(presets)) {
            QAction *ac = m_presetMenu->addAction(pName, this, SLOT(slotLoadPreset()));
            m_presetGroup->addAction(ac);
            ac->setData(pName);
            ac->setCheckable(true);
            if (pName == presetName) {
                ac->setChecked(true);
            }
        }
    });
    emit updatePresets();
    connect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);
    if (paramTag.endsWith(QStringLiteral("lift_gamma_gain"))) {
        // Special case, the colorwheel widget manages several parameters
        QModelIndex index = model->index(0, 0);
        auto w = AbstractParamWidget::construct(model, index, frameSize, this);
        connect(w, &AbstractParamWidget::valuesChanged, this, &AssetParameterView::commitMultipleChanges);
        connect(w, &AbstractParamWidget::valueChanged, this, &AssetParameterView::commitChanges);
        m_lay->addWidget(w);
        connect(w, &AbstractParamWidget::updateHeight, this, [&](int h) {
            setFixedHeight(h + m_lay->contentsMargins().bottom());
            emit updateHeight();
        });
        m_widgets.push_back(w);
    } else {
        int minHeight = 0;
        for (int i = 0; i < model->rowCount(); ++i) {
            QModelIndex index = model->index(i, 0);
            auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
            if (m_mainKeyframeWidget &&
                (type == ParamType::Geometry || type == ParamType::Animated || type == ParamType::RestrictedAnim || type == ParamType::KeyframeParam)) {
                // Keyframe widget can have some extra params that shouldn't build a new widget
                qDebug() << "// FOUND ADDED PARAM";
                m_mainKeyframeWidget->addParameter(index);
            } else {
                auto w = AbstractParamWidget::construct(model, index, frameSize, this);
                connect(this, &AssetParameterView::initKeyframeView, w, &AbstractParamWidget::slotInitMonitor);
                connect(w, &AbstractParamWidget::valueChanged, this, &AssetParameterView::commitChanges);
                connect(w, &AbstractParamWidget::seekToPos, this, &AssetParameterView::seekToPos);
                connect(w, &AbstractParamWidget::activateEffect, this, &AssetParameterView::activateEffect);
                connect(w, &AbstractParamWidget::updateHeight, this, [&]() {
                    setFixedHeight(contentHeight());
                    emit updateHeight();
                });
                m_lay->addWidget(w);
                if (type == ParamType::KeyframeParam || type == ParamType::AnimatedRect || type == ParamType::Roto_spline) {
                    m_mainKeyframeWidget = static_cast<KeyframeWidget *>(w);
                } else {
                    minHeight += w->minimumHeight();
                }
                m_widgets.push_back(w);
            }
        }
        setMinimumHeight(m_mainKeyframeWidget ? m_mainKeyframeWidget->minimumHeight() + minHeight : minHeight);
    }
    if (addSpacer) {
        m_lay->addStretch();
    }
}

QVector<QPair<QString, QVariant>> AssetParameterView::getDefaultValues() const
{
    QVector<QPair<QString, QVariant>> values;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i, 0);
        QString name = m_model->data(index, AssetParameterModel::NameRole).toString();
        auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        QVariant defaultValue = m_model->data(index, AssetParameterModel::DefaultRole);
        if (type == ParamType::KeyframeParam || type == ParamType::AnimatedRect) {
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
    if (m_model->getOwnerId().second != -1) {
        pCore->pushUndo(command);
    } else {
        command->redo();
        delete command;
    }
    // Unselect preset if any
    QAction *ac = m_presetGroup->checkedAction();
    if (ac) {
        ac->setChecked(false);;
    }
}

void AssetParameterView::commitChanges(const QModelIndex &index, const QString &value, bool storeUndo)
{
    // Warning: please note that some widgets (for example keyframes) do NOT send the valueChanged signal and do modifications on their own
    auto *command = new AssetCommand(m_model, index, value);
    if (storeUndo && m_model->getOwnerId().second != -1) {
        pCore->pushUndo(command);
    } else {
        command->redo();
        delete command;
    }
}

void AssetParameterView::commitMultipleChanges(const QList <QModelIndex> indexes, const QStringList &values, bool storeUndo)
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

void AssetParameterView::unsetModel()
{
    QMutexLocker lock(&m_lock);
    if (m_model) {
        // if a model is already there, we have to disconnect signals first
        disconnect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);
    }
    m_mainKeyframeWidget = nullptr;

    // clear layout
    m_widgets.clear();
    QLayoutItem *child;
    while ((child = m_lay->takeAt(0)) != nullptr) {
        if (child->layout()) {
            QLayoutItem *subchild;
            while ((subchild = child->layout()->takeAt(0)) != nullptr) {
                delete subchild->widget();
                delete subchild->spacerItem();
            }
        }
        delete child->widget();
        delete child->spacerItem();
    }

    // Release ownership of smart pointer
    m_model.reset();
}

void AssetParameterView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    QMutexLocker lock(&m_lock);
    if (m_widgets.size() == 0) {
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
        m_widgets[0]->slotRefresh();
        return;
    }
    size_t max = m_widgets.size() - 1;
    if (bottomRight.isValid()) {
        max = qMin(max, (size_t)bottomRight.row());
    }
    Q_ASSERT(max < m_widgets.size());
    for (size_t i = (size_t)topLeft.row(); i <= max; ++i) {
        m_widgets[i]->slotRefresh();
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

/*void AssetParameterView::initKeyframeView()
{
    if (m_mainKeyframeWidget) {
        m_mainKeyframeWidget->initMonitor();
    } else {
        for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i, 0);
        auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::Geometry) {
            return MonitorSceneGeometry;
        }
    }
    }
}*/

void AssetParameterView::slotRefresh()
{
    refresh(m_model->index(0, 0), m_model->index(m_model->rowCount() - 1, 0), {});
}

bool AssetParameterView::keyframesAllowed() const
{
    return m_mainKeyframeWidget != nullptr;
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
        emit updateHeight();
    }
}

void AssetParameterView::slotDeletePreset()
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
        const QString presetFile = dir.absoluteFilePath(QString("%1.json").arg(m_model->getAssetId()));
        m_model->deletePreset(presetFile, presetName);
        emit updatePresets();
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
        presetName = QInputDialog::getText(this, i18n("Enter preset name"), i18n("Enter the name of this preset"), QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    const QString presetFile = dir.absoluteFilePath(QString("%1.json").arg(m_model->getAssetId()));
    m_model->savePreset(presetFile, presetName);
    emit updatePresets(presetName);
}

void AssetParameterView::slotLoadPreset()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action) {
        return;
    }
    const QString presetName = action->data().toString();
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    const QString presetFile = dir.absoluteFilePath(QString("%1.json").arg(m_model->getAssetId()));
    const QVector<QPair<QString, QVariant>> params = m_model->loadPreset(presetFile, presetName);
    auto *command = new AssetUpdateCommand(m_model, params);
    pCore->pushUndo(command);
    emit updatePresets(presetName);
}

QMenu *AssetParameterView::presetMenu()
{
    return m_presetMenu;
}

