/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: splash
    visible: true
    color: "transparent"
    title: "Splash Screen"
    SystemPalette { id: activePalette }
    modality: Qt.WindowModal
    flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    property int timeoutInterval: 5000
    property string version
    property bool crashRecovery: false
    property bool wasUpgraded: false
    signal timeout
    x: (Screen.width - splashContent.width) / 2
    y: (Screen.height - splashContent.height) / 2
    width: splashContent.width
    height: splashContent.height
    signal openLink(string url)
    signal openBlank()
    signal resetConfig()

    property int border: 10

    function fade()
    {
        fadeAnimation.start()
    }

    function activate()
    {
        // Hack to ensure the qml Window stays on top of our QWidget MainWindow while loading
        splash.requestActivate()
        splashContent.forceActiveFocus()
    }

    function displayProgress(message)
    {
        loadingLabel.text = message
        loadingBox.visible = message.length > 0
    }

    Rectangle {
        id: splashContent
        height: splash.crashRecovery || splash.wasUpgraded ? kdenliveid.height * 17 : kdenliveid.height * 15
        width: height * 1.78
        radius: 8
        border.width: 2
        border.color:"#f38577"
        color: activePalette.window
        clip: true
        focus: true
        NumberAnimation on opacity {
            id: fadeAnimation
            running: false
            duration: 600
            easing.type: Easing.InCubic
            from: 1
            to: 0
        }
        onActiveFocusChanged: {
            if (!activeFocus) {
                activate()
            }
        }
        Keys.onEscapePressed: {
            console.log('ESC PRESSED!!!')
            if (splash.wasUpgraded || splash.crashRecovery) {
                openBlank()
            }
        }

        Item {
            id: header
            anchors.fill: parent
            Image {
                id: background
                anchors.fill: parent
                anchors.margins: 5
                source: "qrc:/pics/splash-background.png"
                fillMode: Image.TileVertically
                // Make By KDE
                Image {
                    id: kdelogo
                    asynchronous: true
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 10
                    height: kdelabel.height
                    width: height
                    source: "qrc:/pics/kde-logo.png"
                    fillMode: Image.PreserveAspectFit
                }
                Label {
                    id: kdelabel
                    anchors.verticalCenter: kdelogo.verticalCenter
                    anchors.left: kdelogo.right
                    text: i18n("Make by KDE")
                    color: "#ffffff"
                }
            }
            Rectangle { // rounded corners for image
                anchors.fill: parent
                anchors.margins: 2
                color: "transparent"
                border.color: splashContent.color
                border.width: 4
                radius: 8
            }
        }
        Rectangle {
            id: buttonBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 2
            height: 1.7 * kdenliveid.height
            color: splashContent.color
            opacity: 0.5
        }
        Label {
            id: kdenliveid
            anchors.right: buttonBar.right
            anchors.rightMargin: 10
            anchors.verticalCenter: buttonBar.verticalCenter
            text: i18n("Kdenlive") + " " + splash.version
        }
        MouseArea {
            anchors.fill: parent
            enabled: !splash.crashRecovery && !splash.wasUpgraded
            acceptedButtons: Qt.NoButton
            onClicked: {
                splashContent.visible = false
            }
        }
        Rectangle {
            // Crash recovery
            id: resetBox
            visible: splash.crashRecovery
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: buttonBar.top
            anchors.leftMargin: splashContent.border.width + 5
            anchors.rightMargin: splashContent.border.width + 5
            height: restartButton.height + 10
            color: activePalette.window
            Rectangle {
                anchors.fill: parent
                color: "#22FF0000"
            }
            Label {
                id: restartLabel
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: restartButton.left
                text: i18n("Kdenlive crashed on last start.")
                Layout.alignment: Qt.AlignVCenter
                wrapMode: Text.Wrap
            }
            Button {
                id: restartButton
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: normalStartButton.left
                anchors.rightMargin: 10
                text: i18n("Reset Configuration")
                icon.name: "view-refresh"
                onClicked: resetConfig()
            }
            Button {
                id: normalStartButton
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 10
                text: i18n("Start Normally")
                icon.name: "go-next"
                onClicked: {
                    normalStartButton.text = i18n("Starting…")
                    openBlank()
                }
            }
        }
        Rectangle {
            // Upgrade notice
            id: upgradeBox
            visible: splash.wasUpgraded && !splash.crashRecovery
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: splashContent.border.width + 5
            anchors.rightMargin: splashContent.border.width + 5
            anchors.bottom: buttonBar.top
            height: Math.max(upgradedLabel.height, notesButton.height) + 10
            color: activePalette.window
            Rectangle {
                anchors.fill: parent
                color: "#2200FF00"
            }
            Label {
                id: upgradedLabel
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: notesButton.left
                textFormat: Text.RichText
                text: i18n("Kdenlive was upgraded. If you like it, consider <a href=\"https://kdenlive.org/get-involved/\">getting involved</a> or help <a href=\"https://kdenlive.org/fund/\">funding</a>.")
                wrapMode: Text.Wrap
                HoverHandler {
                    enabled: upgradedLabel.hoveredLink
                    cursorShape: Qt.PointingHandCursor
                }
                onLinkActivated: (link)=> openLink(link)
            }
            Button {
                id: notesButton
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: notesStartButton.left
                anchors.rightMargin: 10
                text: i18n("What's New")
                icon.name: "help-contents"
                onClicked: openLink("https://kdenlive.org/news/releases/" + splash.version + "?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_upgradedone")
            }
            Button {
                id: notesStartButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 10
                text: i18n("Continue")
                icon.name: "go-next"
                onClicked: openBlank()
            }
        }
        Rectangle {
            id: loadingBox
            visible: false
            color: activePalette.window
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: splashContent.border.width + 5
            anchors.rightMargin: splashContent.border.width + 5
            anchors.bottom: buttonBar.top
            height: Math.max(loadingLabel.height, notesButton.height) + 10
            Label {
                id: loadingLabel
                anchors.fill: parent
                anchors.margins: 10
                wrapMode: Text.Wrap
            }
        }
    }
    Component.onCompleted: {
        if (splash.crashRecovery)
            normalStartButton.forceActiveFocus();
        else if (splash.wasUpgraded) {
            notesStartButton.forceActiveFocus();
        } else {
            splashContent.forceActiveFocus()
        }
        visible = true;
    }
}
