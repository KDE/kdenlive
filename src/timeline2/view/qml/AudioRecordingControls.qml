/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import org.kde.kdenlive as K

Item {
    id: recContainer
    property int trackId: -1
    property int recState: audiorec.recordState
    width: parent.width
    implicitHeight: K.UiUtils.baseSizeMedium * 1.5 + 4  // +4 for tick marks below levels
    
    RowLayout {
        spacing: 12
        anchors.fill: parent
        
        AudioRecordButton {
            recState: recContainer.recState
            trackId: recContainer.trackId
            isLocked: trackHeadRoot.isLocked
        }
        
        AudioLevels {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.rightMargin: 5
            Layout.bottomMargin: 1
            audioLevels: audiorec.levels !== undefined ? audiorec.levels : []
        }
    }
}

