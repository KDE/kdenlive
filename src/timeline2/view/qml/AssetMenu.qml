/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQml.Models 2.15

Menu {
    id: menuRoot
    property alias menuModel: itemRepeater.model
    property bool isTransition: false

    signal assetSelected(string assetId)

    Instantiator {
        id: itemRepeater
        onObjectAdded: menuRoot.insertItem( index, object )
        onObjectRemoved: menuRoot.removeItem( object )
        delegate: MenuItem {
            text: timeline.getAssetName(modelData, isTransition) //name
            property string assetId: modelData //identifier
            onTriggered: {
                console.log(assetId)
                menuRoot.assetSelected(assetId)
            }
        }
    }
}
