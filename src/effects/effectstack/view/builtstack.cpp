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

#include <KDeclarative/KDeclarative>
#include <QQmlContext>
#include <QQuickItem>
#include <kdeclarative_version.h>
#if KDECLARATIVE_VERSION >= QT_VERSION_CHECK(5, 98, 0)
#include <kquickiconprovider.h>
#endif

BuiltStack::BuiltStack(AssetPanel *parent)
    : QQuickWidget(parent)
    , m_model(nullptr)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
#if KDECLARATIVE_VERSION < QT_VERSION_CHECK(5, 98, 0)
    kdeclarative.setupEngine(engine());
#else
    engine()->addImageProvider(QStringLiteral("icon"), new KQuickIconProvider);
#endif
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
    if (ownerId.first == ObjectType::TimelineClip) {
        QVariant current_speed(int(100.0 * pCore->getClipSpeed(ownerId.second)));
        qDebug() << " CLIP SPEED OFR: " << ownerId.second << " = " << current_speed;
        QMetaObject::invokeMethod(rootObject(), "setSpeed", Qt::QueuedConnection, Q_ARG(QVariant, current_speed));
    }
    rootContext()->setContextProperty("effectstackmodel", model.get());
    QMetaObject::invokeMethod(rootObject(), "resetStack", Qt::QueuedConnection);
}
