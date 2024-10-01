/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KLocalizedContext>

#include "assets/assetpanel.hpp"
#include "builtstack.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
//#include "qml/colorwheelitem.h"

#include <KQuickIconProvider>
#include <QQmlContext>
#include <QQuickItem>

BuiltStack::BuiltStack(AssetPanel *parent)
    : QQuickWidget(parent)
    , m_model(nullptr)
{
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));

    // qmlRegisterType<ColorWheelItem>("Kdenlive.Controls", 1, 0, "ColorWheelItem");
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setMinimumHeight(300);
    // setClearColor(palette().base().color());
    // setSource(QUrl(QStringLiteral("qrc:/qml/BuiltStack.qml")));
    setFocusPolicy(Qt::StrongFocus);
    QQuickItem *root = rootObject();
    QObject::connect(root, SIGNAL(valueChanged(QString, int)), parent, SLOT(parameterChanged(QString, int)));
    setResizeMode(QQuickWidget::SizeRootObjectToView);
}

BuiltStack::~BuiltStack() = default;

void BuiltStack::setModel(const std::shared_ptr<EffectStackModel> &model, ObjectId ownerId)
{
    m_model = model;
    if (ownerId.type == KdenliveObjectType::TimelineClip) {
        QVariant current_speed(int(100.0 * pCore->getClipSpeed(ownerId)));
        qDebug() << " CLIP SPEED OFR: " << ownerId.itemId << " = " << current_speed;
        QMetaObject::invokeMethod(rootObject(), "setSpeed", Qt::QueuedConnection, Q_ARG(QVariant, current_speed));
    }
    rootContext()->setContextProperty("effectstackmodel", model.get());
    QMetaObject::invokeMethod(rootObject(), "resetStack", Qt::QueuedConnection);
}
