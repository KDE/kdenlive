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

import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.11
import QtQuick.Controls.Styles 1.4


Window {
    id: splash
    objectName: "splash"
    color: "transparent"
    title: "Splash Window"
    SystemPalette { id: activePalette }
    modality: Qt.ApplicationModal
    flags: Qt.SplashScreen
    property int timeoutInterval: 2000
    signal timeout
    x: (Screen.width - splashContent.width) / 2
    y: (Screen.height - splashContent.height) / 2
    width: splashContent.width
    height: splashContent.height

    property int border: 10
    property bool splashing: true

    function endSplash()
    {
        console.log("ending splash")
        splash.splashing = false;
        splash.close();
    }

    Rectangle {
        id:splashContent
        height: Screen.height / 2
        width: Screen.width / 3
        border.width:splash.border
        border.color:"#bfbfbf"
        color: "#31363b"
        Image {
            id:logo
            anchors.left: splashContent.left
            anchors.top: splashContent.top
            anchors.margins: 50
            //  anchors.horizontalCenter: splashContent.horizontalCenter
            source: "qrc:/pics/kdenlive-logo.png"
            fillMode: Image.PreserveAspectFit
            height: splashContent.height / 5 - 100
        }
        RowLayout {
            //anchors.horizontalCenter: splashContent.horizontalCenter
            anchors.bottom: logo.bottom
            anchors.right: splashContent.right
            anchors.rightMargin: logo.x
            spacing: 100
            Text {
                color: "white"
                text: i18n("Website")
                font.bold: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
            }
            Text {
                color: "white"
                text: i18n("Donate")
                font.bold: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
            }
            Text {
                color: "white"
                text: i18n("Forum")
                font.bold: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
            }
        }
        Rectangle {
            id:recentProjects
            y: splashContent.height / 5
            anchors.left: splashContent.left
            anchors.leftMargin: splash.border
            anchors.right: splashContent.right
            anchors.rightMargin: splash.border
            color:"#232629"
            height: 3 * splashContent.height / 5
            width: splashContent.width
            visible: !splashing
            Text {
                id:txtProject
                color: "#f38577"
                text: i18n("Recent Projects")
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 50
                anchors.leftMargin: 100
                font.bold: true
            }
        }
        Image {
            id:splash_img
            y: splashContent.height / 5
            anchors.left: splashContent.left
            anchors.leftMargin: splash.border
            anchors.right: splashContent.right
            anchors.rightMargin: splash.border
            height: 3 * splashContent.height / 5
            width: splashContent.width
            source: "qrc:/pics/splash-background.png"
            fillMode: Image.PreserveAspectFit
            visible: splashing
        }
        /*Text {
            id:txtProject
            color: "#f38577"
            text: "Recent Projects"
            anchors.horizontalCenter: splashContent.horizontalCenter
            anchors.top: logo.bottom
            anchors.topMargin: 50
            font.bold: true
        }
        Rectangle {
            id:recentProjects
            border.width:5
            border.color:"#efefef"
            color: "#fafafa"
            height: splashContent.height / 4
            width: 5*splashContent.width/6
            anchors.horizontalCenter: splashContent.horizontalCenter
            anchors.top: txtProject.bottom
            anchors.topMargin: 5
        }*/
        Item {
            anchors.left: splashContent.left
            anchors.right: splashContent.right
            anchors.bottom: splashContent.bottom
            anchors.top: recentProjects.bottom
            anchors.margins: 50
            visible: !splashing
            CheckBox {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                style: CheckBoxStyle {
                    indicator: Rectangle {
                        implicitWidth: 32
                        implicitHeight: 32
                        radius: 3
                        //border.color: control.activeFocus ? "darkblue" : "gray"
                        border.width: 1
                        border.color:"white"
                        color: "#4d4d4d"
                        Rectangle {
                            visible: control.checked
                            color: "#555"
                            border.color: "#333"
                            radius: 1
                            anchors.margins: 4
                            anchors.fill: parent
                        }
                    }
                    label: Text {
                        text: i18n("Hide on startup")
                        color: "white"
                    }
                }
            }
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 50
                visible: !splashing
                Button {
                    iconSource:"image://icon/document-new"
                    text:i18n("New")
                    //style: CustomButton {
                    //    backColor: splashContent.color
                   // }
                }
                Button {
                    iconSource:"image://icon/document-open"
                    text:i18n("Open")
                    //style: CustomButton {
                    //    backColor: splashContent.color
                   // }
                }
            }
        }

        MouseArea {
            id: clickZone
            anchors.fill: splashContent
            onClicked: {
                console.log("clic");
                  splash.close();
            }
        }
    }
    Component.onCompleted: {
        visible = true
        clickZone.focus = true;
    }

}
