/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.11
import QtQml.Models 2.11

Rectangle {
    id: transitionRoot
    SystemPalette { id: activePalette }
    color: "red"//activePalette.window

    ColumnLayout {
        anchors.fill: parent

        Label {
            Layout.fillWidth:true
            text: i18n("Properties of transition %1", transitionName)
        }
        AssetView {
            Layout.fillWidth:true
            Layout.fillHeight:true
            id: assetView
            assetModel: paramModel
        }

    }


}
