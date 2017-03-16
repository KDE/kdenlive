/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.3

Rectangle {
    id: listRoot

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 6
            ExclusiveGroup { id: filterGroup}
            ToolButton {
                id: showAll
                implicitWidth: 40
                implicitHeight: 40
                iconName: "show-all-effects"
                onClicked: console.log("show dshow-all-effects")
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all effects')
            }
            ToolButton {
                id: showVideo
                implicitWidth: 40
                implicitHeight: 40
                iconName: "kdenlive-show-video"
                iconSource: 'qrc:///pics/kdenlive-show-video.svgz'
                onClicked: console.log("show dshow video")
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all video effects')
            }
            ToolButton {
                id: showAudio
                implicitWidth: 40
                implicitHeight: 40
                iconName: "kdenlive-show-audio"
                iconSource: 'qrc:///pics/kdenlive-show-audio.svgz'
                onClicked: console.log("show dshow audio")
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all audio effects')
            }
            Rectangle {
                //This is a spacer
                Layout.fillHeight: true
                Layout.fillWidth: true
            }
            ToolButton {
                id: showDescription
                implicitWidth: 40
                implicitHeight: 40
                iconName: "help-about"
                onClicked: console.log("show description")
                checkable:true
                tooltip: i18n('Show/hide description of the effects')
            }

        }
        /*FocusScope {
            id: focusScope
            width: 250; height: 28

            BorderImage {
                source: "images/lineedit-bg.png"
                width: parent.width; height: parent.height
                border { left: 4; top: 4; right: 4; bottom: 4 }
            }

            BorderImage {
                source: "images/lineedit-bg-focus.png"
                width: parent.width; height: parent.height
                border { left: 4; top: 4; right: 4; bottom: 4 }
                visible: parent.activeFocus ? true : false
            }

            Text {
                id: typeSomething
                anchors.fill: parent; anchors.leftMargin: 8
                verticalAlignment: Text.AlignVCenter
                text: "Type something..."
                color: "gray"
                font.italic: true
            }

            MouseArea {
                anchors.fill: parent
                onClicked: { focusScope.focus = true; textInput.openSoftwareInputPanel(); }
            }

            TextInput {
                id: textInput
                anchors { left: parent.left; leftMargin: 8; right: clear.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                focus: true
                selectByMouse: true
            }

            Image {
                id: clear
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                source: "images/clear.png"
                opacity: 0

                MouseArea {
                    anchors.fill: parent
                    onClicked: { textInput.text = ''; focusScope.focus = true; textInput.openSoftwareInputPanel(); }
                }
            }

            states: State {
                name: "hasText"; when: textInput.text != ''
                PropertyChanges { target: typeSomething; opacity: 0 }
                PropertyChanges { target: clear; opacity: 1 }
            }

            transitions: [
                Transition {
                    from: ""; to: "hasText"
                    NumberAnimation { exclude: typeSomething; properties: "opacity" }
                },
                Transition {
                    from: "hasText"; to: ""
                    NumberAnimation { properties: "opacity" }
                }
            ]
        } */
        TreeView {
            Layout.fillHeight: true
            Layout.fillWidth: true
            alternatingRowColors: false
            headerVisible: false
            itemDelegate: RowLayout {
                Image{
                    visible: styleData.value != "root"
                    source: 'image://effecticon/' + styleData.value
                }
                Text{
                    text: effectlist.getName(styleData.index)
                }
            }
            TableViewColumn { role: "id"; title: "Name"; width: 200 }
            model: effectListModel
        }
    }
}
