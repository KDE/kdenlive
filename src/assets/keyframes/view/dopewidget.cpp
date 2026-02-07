/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopewidget.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include <QQmlContext>
#include <QQuickItem>

DopeWidget::DopeWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    setClearColor(palette().base().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/kdenlive/DopeSheetView.qml")));
}

void DopeWidget::deleteItem()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "deleteSelection");
    }
}

void DopeWidget::doKeyPressEvent(QKeyEvent *ev)
{
    keyPressEvent(ev);
}
