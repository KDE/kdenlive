/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.15

Rectangle {
    id: assetRoot
    property var assetModel
    SystemPalette { id: activePalette }
    color: "blue"//activePalette.window


    ListView {
        anchors.fill: parent
        id: listView
        delegate: Label{
            text: display
        }

        model: assetModel
    }

}
