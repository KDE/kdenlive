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

#include "splash.hpp"

#include <KDeclarative/KDeclarative>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

Splash::Splash(QObject *parent)
    : QObject(parent)
    , m_engine(new QQmlEngine())
    , childItem(nullptr)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(m_engine);
    kdeclarative.setupEngine(m_engine);
    kdeclarative.setupContext();
    component = new QQmlComponent(m_engine);
    QQuickWindow::setDefaultAlphaBuffer(true);
    component->loadUrl(QUrl(QStringLiteral("qrc:/qml/splash.qml")));
    if (component->isLoading())
        QObject::connect(component, SIGNAL(statusChanged(QQmlComponent::Status)),
                         this, SLOT(continueLoading()));
    else {
        continueLoading();
    }
}

void Splash::continueLoading()
{
    if (component->isReady()) {
        childItem = qobject_cast<QQuickWindow*>(component->create());
    } else {
        qWarning() << component->errorString();
    }
}
Splash::~Splash()
{
    delete childItem;
    delete component;
    delete m_engine;
}

void Splash::endSplash()
{
    if (childItem) {
        QMetaObject::invokeMethod(childItem, "endSplash");
    } else {
        qDebug()<<"** ERROR NO SPLASH COMPO";
    }
    emit sigEndSplash();
}
