/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/
import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Window 2.2
import QtQml.Models 2.11

Menu {
    id: menuRoot
    property alias menuModel: itemRepeater.model
    signal guideSelected(int assetFrame)

    Instantiator {
        id: itemRepeater
        onObjectAdded: menuRoot.insertItem( index, object )
        onObjectRemoved: menuRoot.removeItem( object )
        delegate: MenuItem {
            text: model.comment
            property int assetFrame: model.frame
            onTriggered: {
                menuRoot.guideSelected(assetFrame)
            }
        }
    }
}
