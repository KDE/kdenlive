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
#include "widgets/animationwidget.h"

#include <QDebug>
#include <QFontDatabase>
#include <QLabel>
#include <QVBoxLayout>
#include <utility>

AssetParameterView::AssetParameterView(QWidget *parent)
    : QWidget(parent)
    , m_mainKeyframeWidget(nullptr)
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(2, 2, 2, 2);
    m_lay->setSpacing(2);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
}

void AssetParameterView::setModel(const std::shared_ptr<AssetParameterModel> &model, QPair<int, int> range, QSize frameSize, bool addSpacer)
{
    unsetModel();
    QMutexLocker lock(&m_lock);
    m_model = model;
    m_model->prepareKeyframes();
    const QString paramTag = model->getAssetId();
    connect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);
    if (paramTag == QStringLiteral("lift_gamma_gain")) {
        // Special case, the colorwheel widget manages several parameters
        QModelIndex index = model->index(0, 0);
        auto w = AbstractParamWidget::construct(model, index, range, frameSize, this);
        connect(w, &AbstractParamWidget::valueChanged, this, &AssetParameterView::commitChanges);
        m_lay->addWidget(w);
        m_widgets.push_back(w);
    } else {
        for (int i = 0; i < model->rowCount(); ++i) {
            QModelIndex index = model->index(i, 0);
            auto type = model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
            if (m_mainKeyframeWidget &&
            (type == ParamType::Geometry || type == ParamType::Animated || type == ParamType::RestrictedAnim || type == ParamType::KeyframeParam)) {
                // Keyframe widget can have some extra params that should'nt build a new widget
                qDebug() << "// FOUND ADDED PARAM";
                m_mainKeyframeWidget->addParameter(index);
            } else {
                auto w = AbstractParamWidget::construct(model, index, range, frameSize, this);
                connect(this, &AssetParameterView::initKeyframeView, w, &AbstractParamWidget::slotInitMonitor);
                if (type == ParamType::KeyframeParam || type == ParamType::AnimatedRect ) {
                    m_mainKeyframeWidget = static_cast<KeyframeWidget *>(w);
                }
                connect(w, &AbstractParamWidget::valueChanged, this, &AssetParameterView::commitChanges);
                connect(w, &AbstractParamWidget::seekToPos, this, &AssetParameterView::seekToPos);
                m_lay->addWidget(w);
                m_widgets.push_back(w);
            }
        }
    }
    if (addSpacer) {
        m_lay->addStretch();
    }
}

void AssetParameterView::resetValues()
{
    auto type = m_model->data(m_model->index(0, 0), AssetParameterModel::TypeRole).value<ParamType>();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i, 0);
        QString name = m_model->data(index, AssetParameterModel::NameRole).toString();
        QString defaultValue = m_model->data(index, AssetParameterModel::DefaultRole).toString();
        m_model->setParameter(name, defaultValue);
        if (m_mainKeyframeWidget) {
            // Handles additionnal params like rotation so only refresh initial param at the end
        }
        else if (type == ParamType::ColorWheel) {
            if (i == m_model->rowCount() - 1) {
                // Special case, the ColorWheel widget handles several params, so only refresh once when all parameters were set.
                QModelIndex firstIndex = m_model->index(0, 0);
                refresh(firstIndex, firstIndex, QVector<int>());
            }
        } else {
            refresh(index, index, QVector<int>());
        }
    }
    if (m_mainKeyframeWidget) {
        m_mainKeyframeWidget->slotRefresh();
    }
}

void AssetParameterView::setRange(QPair<int, int> range)
{
    qDebug() << "SETTING RANGE" << range;
    QMutexLocker lock(&m_lock);
    for (uint i = 0; i < m_widgets.size(); ++i) {
        auto w = m_widgets[i];
        w->slotSetRange(range);
    }
}

void AssetParameterView::commitChanges(const QModelIndex &index, const QString &value, bool storeUndo)
{
    // Warning: please note that some widgets (for example keyframes) do NOT send the valueChanged signal and do modifications on their own
    AssetCommand *command = new AssetCommand(m_model, index, value);
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
    if (m_mainKeyframeWidget) {
        m_mainKeyframeWidget->slotRefresh();
    } else {
        Q_ASSERT(bottomRight.row() < (int)m_widgets.size());
        for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
            m_widgets[(uint)i]->slotRefresh();
        }
    }
}

int AssetParameterView::contentHeight() const
{
    return m_lay->sizeHint().height();
}

MonitorSceneType AssetParameterView::needsMonitorEffectScene() const
{
    if (m_mainKeyframeWidget) {
        return MonitorSceneGeometry;
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

