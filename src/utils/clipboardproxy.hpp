/*
    SPDX-FileCopyrightText: 2018 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

/** @class ClipboardProxy
    @brief Provides an interface to the clipboard, to use directly from QML
    Inspired by https://stackoverflow.com/questions/40092352/passing-qclipboard-to-qml
 */
class ClipboardProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList mimeTypes READ mimeTypes NOTIFY changed)
public:
    explicit ClipboardProxy(QObject *parent);

    QStringList mimeTypes() const;

signals:
    void changed();
};
