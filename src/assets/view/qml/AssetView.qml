/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.11
import QtQml.Models 2.11

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
