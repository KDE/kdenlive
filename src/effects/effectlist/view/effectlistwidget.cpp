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

#include "effectlistwidget.hpp"
#include "../model/effecttreemodel.hpp"
#include "../model/effectfilter.hpp"
#include "effects/effectsrepository.hpp"

#include <KDeclarative/KDeclarative>
#include <QStandardPaths>
#include <QQmlContext>

EffectListWidget::EffectListWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    QString effectCategory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("kdenliveeffectscategory.rc"));
    m_model.reset(new EffectTreeModel(effectCategory, this));


    m_proxyModel = new EffectFilter(this);
    m_proxyModel->setSourceModel(m_model.get());

    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.initialize();
    kdeclarative.setupBindings();

    setResizeMode(QQuickWidget::SizeRootObjectToView);
    rootContext()->setContextProperty("effectlist", this);
    rootContext()->setContextProperty("effectListModel", m_proxyModel);

    m_effectIconProvider.reset(new EffectIconProvider);
    engine()->addImageProvider(QStringLiteral("effecticon"), m_effectIconProvider.get());
    setSource(QUrl(QStringLiteral("qrc:/qml/effectList.qml")));
    setFocusPolicy(Qt::StrongFocus);
}


QString EffectListWidget::getName(const QModelIndex& index) const
{
    return m_model->getName(m_proxyModel->mapToSource(index));
}
