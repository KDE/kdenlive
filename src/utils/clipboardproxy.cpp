/*
    SPDX-FileCopyrightText: 2018 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipboardproxy.hpp"
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

ClipboardProxy::ClipboardProxy(QObject *parent)
    : QObject(parent)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, this, &ClipboardProxy::changed);
    connect(clipboard, &QClipboard::selectionChanged, this, &ClipboardProxy::changed);
}

QStringList ClipboardProxy::mimeTypes() const
{
    return QGuiApplication::clipboard()->mimeData()->formats();
}
