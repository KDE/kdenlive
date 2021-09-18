/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Window 2.2
import QtQml.Models 2.11

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
