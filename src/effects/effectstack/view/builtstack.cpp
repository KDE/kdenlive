/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "builtstack.hpp"
#include "assets/assetpanel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
//#include "qml/colorwheelitem.h"

#include <KDeclarative/KDeclarative>
#include <QQmlContext>
#include <QQuickItem>
#include <kdeclarative_version.h>

BuiltStack::BuiltStack(AssetPanel *parent)
    : QQuickWidget(parent)
    , m_model(nullptr)
{
    KDeclarative::KDeclarative kdeclarative;
    QQmlEngine *eng = engine();
    kdeclarative.setDeclarativeEngine(eng);
    kdeclarative.setupEngine(eng);
    kdeclarative.setupContext();
    // qmlRegisterType<ColorWheelItem>("Kdenlive.Controls", 1, 0, "ColorWheelItem");
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setMinimumHeight(300);
    // setClearColor(palette().base().color());
    // setSource(QUrl(QStringLiteral("qrc:/qml/BuiltStack.qml")));
    setFocusPolicy(Qt::StrongFocus);
    QQuickItem *root = rootObject();
    QObject::connect(root, SIGNAL(valueChanged(QString,int)), parent, SLOT(parameterChanged(QString,int)));
    setResizeMode(QQuickWidget::SizeRootObjectToView);
}

BuiltStack::~BuiltStack() = default;

void BuiltStack::setModel(const std::shared_ptr<EffectStackModel> &model, ObjectId ownerId)
{
    m_model = model;
    if (ownerId.first == ObjectType::TimelineClip) {
        QVariant current_speed((int)(100.0 * pCore->getClipSpeed(ownerId.second)));
        qDebug() << " CLIP SPEED OFR: " << ownerId.second << " = " << current_speed;
        QMetaObject::invokeMethod(rootObject(), "setSpeed", Qt::QueuedConnection, Q_ARG(QVariant, current_speed));
    }
    rootContext()->setContextProperty("effectstackmodel", model.get());
    QMetaObject::invokeMethod(rootObject(), "resetStack", Qt::QueuedConnection);
}
