/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import org.kde.ki18n

Window {
    id: splash
    property string version
    property var urls: []
    property var fileNames: []
    property var fileDates: []
    property var profileIds: []
    property var profileNames: []
    property var horizontalModel: [{value: "3840x2160x709x1x1x16x9", text: KI18n.i18n("4K UHD 3840x2160")}, {value: "4096x2160x709x1x1x256x135", text: KI18n.i18n("4K DCI 4096x2160")}, {value: "2560x1440x709x1x1x16x9", text: KI18n.i18n("2.5K QHD 2560x1440")}, {value: "1920x1080x709x1x1x16x9", text: KI18n.i18n("Full HD 1920x1080")}, {value: "1280x720x709x1x1x16x9", text: KI18n.i18n("HD 1280x720")}, {value: "720x480x601x32x27x16x9", text: KI18n.i18n("SD NTSC Wide 720x480")}, {value: "720x480x601x8x9x4x3", text: KI18n.i18n("SD NTSC 720x480")}, {value: "720x576x601x64x45x16x9", text: KI18n.i18n("SD PAL Wide 720x576")}, {value: "720x576x601x16x15x4x3", text: KI18n.i18n("SD PAL 720x576")}]
    property var verticalModel: [{value: "1080x1920x709x1x1x9x16", text: KI18n.i18n("Full HD 1080x1920")}, {value: "720x1280x709x1x1x9x16", text: KI18n.i18n("HD 720x1280")}]
    property var horizontalFpsModel: [{value: "24000/1001", text: KI18n.i18n("23.98 fps")},
        {value: "24", text: KI18n.i18n("24 fps")}, {value: "25", text: KI18n.i18n("25 fps")}, {value: "30000/1001", text: KI18n.i18n("29.97 fps")}, {value: "30", text: KI18n.i18n("30 fps")}, {value: "50", text: KI18n.i18n("50 fps")}, {value: "60000/1001", text: KI18n.i18n("59.94 fps")}, {value: "60", text: KI18n.i18n("60 fps")}]
    property var verticalFpsModel: [{value: "25", text: KI18n.i18n("25 fps")}, {value: "30", text: KI18n.i18n("30 fps")}, {value: "60", text: KI18n.i18n("60 fps")}]
    property bool firstRun: true
    property bool crashRecovery: false
    property bool actionsEnabled: false
    property bool wasUpgraded: false
    property bool palFps: true
    property int border: 10

    signal timeout()
    signal openBlank()
    signal openOtherFile()
    signal closeApp()
    signal switchPalette(bool dark)
    signal openFile(string url)
    signal openLink(string url)
    signal firstStart(string resolution, string fps, bool interlaced, int vTracks, int aTracks)
    signal openTemplate(string templateId)
    signal showWelcome(bool show)
    signal resetConfig()
    signal clearHistory()
    signal clearProfiles()
    signal forgetFile(string url)
    signal forgetProfile(string id)

    visible: true
    color: "transparent"
    modality: Qt.WindowModal
    flags: Qt.FramelessWindowHint
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    minimumWidth: splashContent.implicitWidth
    minimumHeight: splashContent.implicitHeight
    width: splashContent.implicitWidth
    height: splashContent.implicitHeight

    function fade()
    {
        fadeAnimation.start()
    }

    function enableActions()
    {
        actionsEnabled = true
    }

    function displayProgress(message)
    {
        loadingLabel.text = message
        loadingBox.visible = message.length > 0
        recentProjects.enabled = false
        donateBar.visible = false
    }

    Component.onCompleted: {
        if (splash.firstRun)
            buttonNext.forceActiveFocus();
        else if (urls.length === 0) {
            tlistView.forceActiveFocus();
        } else {
            listView.forceActiveFocus();
        }
        visible = true;
    }

    SystemPalette {
        id: activePalette
    }

    Rectangle {
        id: splashContent
        anchors.fill: parent
        implicitHeight: splash.crashRecovery || splash.wasUpgraded  ? newProjectButton.height * 20 : newProjectButton.height * 18
        implicitWidth: newProjectButton.height * 17
        radius: 5
        border.width: 2
        border.color: "#d7566e"
        color: activePalette.window
        clip: true
        focus: true
        NumberAnimation on opacity {
            id: fadeAnimation
            running: false
            duration: 1000
            easing.type: Easing.OutCubic
            from: 1
            to: 0
        }
        Keys.enabled: splash.actionsEnabled
        Keys.onDownPressed: {
            if (listView.activeFocus) {
                if (listView.currentIndex < splash.urls.length)
                    listView.currentIndex += 1;

            } else if (tlistView.activeFocus) {
                if (tlistView.currentIndex < splash.urls.length)
                    tlistView.currentIndex += 1;

            }
        }
        Keys.onUpPressed: {
            if (listView.activeFocus) {
                if (listView.currentIndex > 0)
                    listView.currentIndex -= 1;

            } else if (tlistView.activeFocus) {
                if (tlistView.currentIndex > 0)
                    tlistView.currentIndex -= 1;

            }
        }
        Keys.onReturnPressed: {
            if (listView.activeFocus) {
                if (listView.currentIndex >= 0 && listView.currentIndex < splash.urls.length) {
                    splashContent.enabled = false
                    splash.fade()
                    splash.openFile(splash.urls[listView.currentIndex]);
                }

            } else if (tlistView.activeFocus) {
                if (tlistView.currentIndex >= 0 && tlistView.currentIndex < splash.profileIds.length)
                    splash.openTemplate(splash.profileIds[tlistView.currentIndex]);
            }
        }
        Keys.onEnterPressed: {
            if (listView.activeFocus) {
                if (listView.currentIndex >= 0 && listView.currentIndex < splash.urls.length) {
                    splashContent.enabled = false
                    splash.fade()
                    splash.openFile(splash.urls[listView.currentIndex]);
                }

            } else if (tlistView.activeFocus) {
                if (tlistView.currentIndex >= 0 && tlistView.currentIndex < splash.profileIds.length)
                    splash.openTemplate(splash.profileIds[tlistView.currentIndex]);

            }
        }
        Keys.onEscapePressed: {
            if (splash.firstRun) {
                splash.firstStart(profileCombo.currentValue, fpsCombo.currentValue, verticalFrame.checked ? false : interlacedSwitch.enabled ? interlacedSwitch.checked : true, vTracks.value, aTracks.value)
                splash.hide()
            } else {
                splash.openBlank()
            }
        }

        Shortcut {
            sequence: StandardKey.Quit
            context: Qt.ApplicationShortcut
            onActivated: splash.closeApp()
        }

        Shortcut {
            sequence: StandardKey.Open
            context: Qt.ApplicationShortcut
            enabled: !splash.firstRun
            onActivated: splash.openOtherFile()
        }

        Item {
            id: header
            height: splashContent.height / 3
            anchors.left: parent.left
            anchors.right: parent.right

            Image {
                id: background
                anchors.fill: parent
                anchors.margins: 5
                anchors.bottomMargin: 10
                source: "qrc:/pics/splash-background.webp"
                verticalAlignment: Image.AlignTop
                fillMode: Image.PreserveAspectCrop

                Text {
                    id: kdelabel
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 10
                    color: "#FFFFFF"
                    text: KI18n.i18n("Made by KDE")
                }

                // Made By KDE
                Image {
                    id: kdelogo
                    asynchronous: true
                    anchors.verticalCenter: kdelabel.verticalCenter
                    anchors.right: kdelabel.left
                    height: kdelabel.height
                    width: height
                    source: "qrc:/pics/kde-logo.png"
                    fillMode: Image.PreserveAspectFit
                }
            }
            // rounded corners for image

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                color: "transparent"
                border.color: splashContent.color
                border.width: 4
                radius: 5
            }

            Rectangle {
                anchors.bottom: background.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                height: 5
                color: splashContent.color
            }

        }

        Item {
            id: recentProjects
            anchors.top: header.bottom
            anchors.left: splashContent.left
            anchors.leftMargin: splash.border
            anchors.right: splashContent.right
            anchors.rightMargin: splash.border
            anchors.bottom: splashContent.bottom
            visible: !splash.firstRun
            RowLayout {
                id: recentListContainer
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                height: childrenRect.height
                spacing: 20

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: childrenRect.height

                    Button {
                        id: openButton

                        anchors.top: parent.top
                        anchors.left: parent.left
                        icon.name: "document-open"
                        text: KI18n.i18n("Open Project…")
                        onClicked: splash.openOtherFile()
                        KeyNavigation.tab: newProjectButton
                    }

                    Item {
                        //Layout.fillHeight: true
                        anchors.top: openButton.bottom
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: childrenRect.height
                        visible: splash.urls.length > 0

                        Rectangle {
                            id: recentSeparator
                            anchors.top: parent.top
                            anchors.margins: 10
                            height: recentLabel.height + 6
                            width: parent.width
                            radius: height / 4
                            color: Qt.darker(splashContent.color, 1.5)

                            Label {
                                id: recentLabel
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 10
                                text: KI18n.i18n("Open Recent")
                            }
                            ToolButton {
                                id: clearHistoryButton
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                icon.name: "edit-clear-history"
                                hoverEnabled: true
                                ToolTip.text: KI18n.i18n("Clear History of Recent Projects")
                                ToolTip.delay: 1000
                                ToolTip.visible: hovered
                                KeyNavigation.tab: templatesClearButton
                                onClicked: {
                                    splash.clearHistory()
                                    splash.urls = []
                                    splash.fileNames = []
                                    splash.fileDates = []
                                    listView.model = splash.urls
                                }
                            }

                        }

                        ScrollView {
                            id: recentListView
                            anchors.top: recentSeparator.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.topMargin: 10
                            height: newProjectButton.height * (splash.crashRecovery || splash.wasUpgraded ? 4 : 5)
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            clip: true

                            ListView {
                                id: listView
                                property int hoveredIndex: -1
                                property int itemHeight: recentLabel.font.pixelSize * 2.2
                                focus: true
                                model: splash.urls
                                KeyNavigation.right: tlistView
                                KeyNavigation.tab: tlistView

                                delegate: Item {
                                    id: fileItem
                                    width: parent.width
                                    height: listView.itemHeight
                                    required property url modelData
                                    required property int index

                                    ToolTip.text: modelData
                                    ToolTip.visible: index == listView.hoveredIndex
                                    ToolTip.delay: Application.styleHints.mousePressAndHoldInterval
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 4
                                        color: activePalette.highlight
                                        visible: fileItem.index == listView.currentIndex && listView.activeFocus
                                        opacity: 0.5
                                    }

                                    Label {
                                        id: listLabel
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 4
                                        anchors.right: datesLabel.left
                                        leftPadding: 4
                                        text: splash.fileNames[fileItem.index]
                                        elide: Text.ElideRight
                                        color: labelArea.containsMouse || listButton.hovered ? activePalette.highlight : activePalette.text
                                        opacity: recentProjects.enabled ? 1 : 0.5
                                    }
                                    Label {
                                        id: datesLabel
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.right: parent.right
                                        anchors.rightMargin: 4
                                        text: splash.fileDates[fileItem.index]
                                        color: activePalette.text
                                        opacity: recentProjects.enabled ? 0.7 : 0.4
                                    }

                                    MouseArea {
                                        id: labelArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        enabled: splash.actionsEnabled
                                        cursorShape: Qt.PointingHandCursor
                                        onPositionChanged: {
                                            if (fileItem.index != listView.hoveredIndex)
                                                listView.hoveredIndex = fileItem.index;
                                        }
                                        onExited: {
                                            listView.hoveredIndex = -1;
                                        }
                                        onClicked: {
                                            splashContent.enabled = false
                                            splash.fade()
                                            splash.openFile(fileItem.modelData);
                                        }
                                    }
                                    Rectangle {
                                        anchors.fill: listButton
                                        radius: 2
                                        color: splashContent.color
                                        visible: listButton.visible
                                    }

                                    ToolButton {
                                        id: listButton
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: listLabel.height
                                        width: height
                                        icon.name: "list-remove"
                                        enabled: splash.actionsEnabled
                                        hoverEnabled: true
                                        ToolTip.text: KI18n.i18n("Remove file from list")
                                        ToolTip.delay: 1000
                                        ToolTip.visible: hovered
                                        visible: labelArea.containsMouse || hovered
                                        onClicked: {
                                            splash.forgetFile(fileItem.modelData)
                                            splash.fileNames.splice(fileItem.index, 1)
                                            splash.urls.splice(fileItem.index, 1)
                                            listView.model = splash.urls
                                        }
                                    }

                                }

                            }

                        }

                    }

                }

                Item {
                    Layout.preferredHeight: childrenRect.height
                    Layout.preferredWidth: templateSeparator.width

                    Button {
                        id: newProjectButton
                        text: KI18n.i18n("New Project…")
                        enabled: splash.actionsEnabled
                        icon.name: "document-new"
                        onClicked: splash.openTemplate("")
                        anchors.top: parent.top
                        anchors.left: parent.left
                        KeyNavigation.tab: clearHistoryButton
                    }

                    Item {
                        anchors.top: newProjectButton.bottom
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: childrenRect.height

                        Rectangle {
                            id: templateSeparator
                            anchors.top: parent.top
                            anchors.margins: 10
                            height: templatesLabel.height + 6
                            width: templatesLabel.width + 1.5 * templatesClearButton.width
                            radius: height / 4
                            color: Qt.darker(splashContent.color, 1.5)
                            Label {
                                id: templatesLabel
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 10
                                text: KI18n.i18n("New From Profile")
                            }
                            ToolButton {
                                id: templatesClearButton
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                icon.name: "edit-clear-history"
                                enabled: splash.actionsEnabled
                                hoverEnabled: true
                                ToolTip.text: KI18n.i18n("Clear History of Recent Profiles")
                                ToolTip.delay: 1000
                                ToolTip.visible: hovered
                                KeyNavigation.tab: showBox
                                onClicked: {
                                    splash.clearProfiles()
                                    splash.profileIds = []
                                    splash.profileNames = []
                                    tlistView.model = splash.profileIds
                                }
                            }

                        }

                        ScrollView {
                            anchors.top: templateSeparator.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 10
                            height: recentListView.height
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            clip: true

                            ListView {
                                id: tlistView

                                property int hoveredIndex: -1
                                focus: true
                                model: splash.profileIds
                                KeyNavigation.left: listView
                                KeyNavigation.tab: openButton

                                delegate: Item {
                                    id: templateItem
                                    width: parent.width
                                    height: listView.itemHeight
                                    required property string modelData
                                    required property int index
                                    ToolTip.text: modelData
                                    ToolTip.visible: index == tlistView.hoveredIndex
                                    ToolTip.delay: Application.styleHints.mousePressAndHoldInterval
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 4
                                        color: activePalette.highlight
                                        visible: templateItem.index == tlistView.currentIndex && tlistView.activeFocus
                                        opacity: 0.5
                                    }

                                    Label {
                                        id: tlistLabel
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 4
                                        anchors.right: parent.right
                                        anchors.rightMargin: 4
                                        leftPadding: 4
                                        text: splash.profileNames[templateItem.index]
                                        color: templatesArea.containsMouse || tlistButton.hovered ? activePalette.highlight : activePalette.text
                                        elide: Text.ElideMiddle
                                        opacity: recentProjects.enabled ? 1 : 0.5
                                    }

                                    MouseArea {
                                        id: templatesArea

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        enabled: splash.actionsEnabled
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            tlistView.currentIndex = templateItem.index;
                                            tlistView.forceActiveFocus();
                                            splash.openTemplate(templateItem.modelData);

                                        }
                                        onPositionChanged: {
                                            if (templateItem.index != tlistView.hoveredIndex)
                                                tlistView.hoveredIndex = templateItem.index;

                                        }
                                        onExited: {
                                            tlistView.hoveredIndex = -1;
                                        }
                                    }
                                    ToolButton {
                                        id: tlistButton
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: tlistLabel.height
                                        width: height
                                        enabled: splash.actionsEnabled
                                        icon.name: "list-remove"
                                        ToolTip.text: KI18n.i18n("Remove profile from list")
                                        ToolTip.delay: 1000
                                        ToolTip.visible: hovered
                                        hoverEnabled: true
                                        visible: templatesArea.containsMouse || hovered
                                        onClicked: {
                                            splash.forgetProfile(templateItem.modelData)
                                            splash.profileNames.splice(templateItem.index, 1)
                                            splash.profileIds.splice(templateItem.index, 1)
                                            tlistView.model = splash.profileIds
                                        }
                                    }

                                }

                            }

                        }

                    }

                }

            }
            Item {
                id: buttonBar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: showBox.height + 8
                anchors.margins: 10

                CheckBox {
                    id: showBox

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    checked: true
                    text: KI18n.i18n("Show on Startup")
                    onCheckStateChanged: {
                        splash.showWelcome(checked)
                    }
                }

                Label {
                    id: kdenliveid
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: KI18n.i18n("Kdenlive") + " " + splash.version
                }

            }
            Item {
                // Crash recovery
                visible: splash.crashRecovery
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: buttonBar.top
                anchors.margins: 10
                height: Math.max(restartButton.height, restartLabel.height) + 10
                Rectangle {
                    anchors.fill: parent
                    color: "#22FF0000"
                    radius: 5
                }
                Label {
                    id: restartLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: restartButton.left
                    text: KI18n.i18n("Kdenlive crashed on last start. Reset config and restart?")
                    Layout.alignment: Qt.AlignVCenter
                    wrapMode: Text.Wrap
                }
                Button {
                    id: restartButton
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    text: KI18n.i18n("Reset")
                    icon.name: "view-refresh"
                    enabled: splash.actionsEnabled
                    onClicked: splash.resetConfig()
                    KeyNavigation.tab: listView
                }
            }
            Item {
                // Upgrade notice
                id: upgradeBox
                visible: splash.wasUpgraded && !splash.crashRecovery
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: buttonBar.top
                anchors.margins: 10
                height: Math.max(notesButton.height, upgradedLabel.height) + 10
                Rectangle {
                    anchors.fill: parent
                    color: "#2200FF00"
                    radius: 5
                }
                Label {
                    id: upgradedLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: notesButton.left
                    text: KI18n.i18n("Kdenlive was upgraded. If you like it, consider <a href=\"%1\">getting involved</a> or help <a href=\"%2\">funding</a>.",
                               "https://kdenlive.org/get-involved/?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_upgraded_contribute&mtm_content=" + splash.version,
                               "https://kdenlive.org/fund/?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_upgraded_donate&mtm_content=" + splash.version)
                    wrapMode: Text.Wrap
                    onLinkActivated: (link)=> splash.openLink(link)
                    textFormat: Text.RichText
                    HoverHandler {
                        enabled: upgradedLabel.hoveredLink
                        cursorShape: Qt.PointingHandCursor
                    }
                }
                Button {
                    id: notesButton
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    enabled: splash.actionsEnabled
                    text: KI18n.i18n("What's New")
                    icon.name: "help-contents"
                    onClicked: splash.openLink("https://kdenlive.org/news/releases/" + splash.version + "?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_upgraded_notes&mtm_content=" + splash.version)
                    KeyNavigation.tab: listView
                }
            }

            Rectangle {
                id: donateBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: buttonBar.top
                height: donateText.height + 8
                anchors.margins: 10
                visible: splash.urls.length > 2 && !splash.crashRecovery && !upgradeBox.visible
                radius: 5
                color: Qt.darker(splashContent.color, 1.5)

                RowLayout {
                    anchors.right: parent.right
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    Label {
                        id: donateText
                        Layout.fillWidth: true
                        leftPadding: 10
                        text: KI18n.i18n("Help us make Kdenlive even better")
                        wrapMode: Text.WordWrap
                    }
                    ToolButton {
                        icon.name: "user-group-new"
                        text: KI18n.i18n("Contribute…")
                        enabled: splash.actionsEnabled
                        onClicked: splash.openLink("https://kdenlive.org/get-involved?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_donatebar_contribute&mtm_content=" + splash.version)
                    }

                    ToolButton {
                        text: KI18n.i18n("Donate…")
                        icon.name: "donate"
                        enabled: splash.actionsEnabled
                        onClicked: splash.openLink("https://kdenlive.org/fund?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_donatebar_donate&mtm_content=" + splash.version)
                        KeyNavigation.tab: listView
                    }

                }

            }
            // Loading progress info
            Rectangle {
                id: loadingBox
                visible: false
                color: activePalette.highlight
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: buttonBar.top
                radius: 5
                height: Math.max(loadingLabel.height, notesButton.height) + 10
                opacity: 0.5
            }
            Label {
                id: loadingLabel
                anchors.verticalCenter: loadingBox.verticalCenter
                anchors.left: loadingBox.left
                anchors.margins: 5
                wrapMode: Text.Wrap
                color: activePalette.highlightedText
            }

        }

        Item {
            id: welcome
            anchors.fill: parent
            anchors.margins: splash.border
            anchors.topMargin: header.height
            visible: splash.firstRun

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop
                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.StyledText
                        horizontalAlignment: Text.AlignJustify
                        text: KI18n.i18n("<h3>Welcome to Kdenlive Quick Setup</h3>")
                    }

                    Switch {
                        id: themeSwitch
                        enabled: !splash.crashRecovery
                        text: KI18n.i18n("Always Use Dark Color Theme")
                        checked: true
                        onCheckedChanged: {
                            splash.switchPalette(checked);
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.StyledText
                        horizontalAlignment: Text.AlignJustify
                        wrapMode: Text.WordWrap
                        text: KI18n.i18n("<b>Default Project Profile</b>")
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.minimumHeight: childrenRect.height + 2 * 14
                        radius: 5
                        color: Qt.darker(splashContent.color, 1.4)

                        RowLayout {
                            id: profileRow
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 14
                            spacing: 20

                            ColumnLayout {
                                Layout.fillWidth: false
                                // Layout.preferredWidth: horizontalFrame.implicitWidth

                                Rectangle {
                                    id: horizontalFrame
                                    implicitHeight: buttonNext.height * 1.4
                                    implicitWidth: horizontalFrame.implicitHeight * 16 / 9
                                    Layout.alignment: Qt.AlignHCenter
                                    radius: 5
                                    property bool checked: true
                                    property int lastIndex: -1
                                    property int lastFpsIndex: -1
                                    border.color: checked ? activePalette.highlight : horizontalArea.containsMouse ? activePalette.highlight : activePalette.text
                                    border.width: checked ? 3 : horizontalArea.containsMouse ? 2 : 1
                                    color: checked ? splashContent.color : Qt.darker(splashContent.color, 1.2)
                                    opacity: checked ? 1 : 0.5

                                    MouseArea {
                                        id: horizontalArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            verticalFrame.lastIndex = profileCombo.currentIndex
                                            verticalFrame.lastFpsIndex = fpsCombo.currentIndex
                                            horizontalFrame.checked = true
                                            verticalFrame.checked = false
                                            if (horizontalFrame.lastIndex > -1) {
                                                profileCombo.currentIndex = horizontalFrame.lastIndex
                                            }
                                            if (horizontalFrame.lastFpsIndex > -1) {
                                                fpsCombo.currentIndex = horizontalFrame.lastFpsIndex
                                            }
                                        }
                                    }

                                }

                                Label {
                                    id: horizontalLabel
                                    text: KI18n.i18n("Horizontal")
                                    Layout.alignment: Qt.AlignHCenter
                                    font.bold: horizontalFrame.checked
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: false

                                Rectangle {
                                    id: verticalFrame
                                    implicitHeight: horizontalFrame.implicitHeight
                                    implicitWidth: verticalFrame.implicitHeight * 9 / 16
                                    Layout.alignment: Qt.AlignHCenter
                                    radius: 5
                                    property bool checked: false
                                    property int lastIndex: -1
                                    property int lastFpsIndex: -1
                                    border.color: checked ? activePalette.highlight : verticalArea.containsMouse ? activePalette.highlight : activePalette.text
                                    border.width: checked ? 3 : verticalArea.containsMouse ? 2 : 1
                                    color: checked ? splashContent.color : Qt.darker(splashContent.color, 1.2)
                                    opacity: checked ? 1 : 0.5

                                    MouseArea {
                                        id: verticalArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            horizontalFrame.lastIndex = profileCombo.currentIndex
                                            horizontalFrame.lastFpsIndex = fpsCombo.currentIndex
                                            horizontalFrame.checked = false
                                            verticalFrame.checked = true
                                            if (verticalFrame.lastIndex > -1) {
                                                profileCombo.currentIndex = verticalFrame.lastIndex
                                            }
                                            if (verticalFrame.lastFpsIndex > -1) {
                                                fpsCombo.currentIndex = verticalFrame.lastFpsIndex
                                            }
                                        }
                                    }

                                }

                                Label {
                                    id: vLabel
                                    text: KI18n.i18n("Vertical")
                                    Layout.alignment: Qt.AlignHCenter
                                    font.bold: verticalFrame.checked
                                }

                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Row {
                                    spacing: 10
                                    Label {
                                        text: KI18n.i18n("Resolution")
                                        verticalAlignment: Qt.AlignVCenter
                                        Layout.alignment: Qt.AlignVCenter
                                        height: profileCombo.height
                                    }

                                    ComboBox {
                                        id: profileCombo
                                        // profile value is: width x height x colorspace x sarNum x sarDen x darNum x darDen
                                        model: horizontalFrame.checked ? splash.horizontalModel : splash.verticalModel
                                        textRole: "text"
                                        valueRole: "value"
                                        property bool sdProfile: horizontalFrame.checked && (currentValue.startsWith("720x480x") || currentValue.startsWith("720x576x"))
                                        Component.onCompleted: {
                                            let index = indexOfValue("1920x1080x709x1x1x16x9");
                                            if (index >= 0) {
                                                currentIndex = index
                                            }
                                        }
                                    }

                                }

                                Row {
                                    spacing: 10

                                    Label {
                                        text: KI18n.i18n("Frame Rate")
                                        verticalAlignment: Qt.AlignVCenter
                                        Layout.alignment: Qt.AlignVCenter
                                        height: profileCombo.height
                                        enabled: fpsCombo.enabled
                                    }

                                    ComboBox {
                                        id: fpsCombo
                                        model: horizontalFrame.checked ? splash.horizontalFpsModel : splash.verticalFpsModel
                                        textRole: "text"
                                        valueRole: "value"
                                        Component.onCompleted: {
                                            let index = indexOfValue(splash.palFps ? "25" : horizontalFrame.checked ? "24000/1001" : "30");
                                            if (index >= 0) {
                                                currentIndex = index
                                            }
                                        }
                                    }

                                }

                                Row {
                                    spacing: 10
                                    Switch {
                                        id: interlacedSwitch
                                        text: KI18n.i18n("Interlaced")
                                        enabled: !profileCombo.sdProfile && horizontalFrame.checked
                                    }

                                }

                            }

                        }

                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label {
                            text: KI18n.i18n("Video Tracks")
                            Layout.alignment: Qt.AlignVCenter
                        }
                        SpinBox {
                            id: vTracks
                            value: 2

                        }
                        Label {
                            text: KI18n.i18n("Audio Tracks")
                            Layout.alignment: Qt.AlignVCenter
                        }
                        SpinBox {
                            id: aTracks
                            value: 2
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: KI18n.i18n("This can always be changed later in the Settings.")
                        font.italic: true
                    }
                    Rectangle {
                        // Crash recovery
                        visible: splash.crashRecovery
                        Layout.fillWidth: true
                        Layout.minimumHeight: crashRecoveryLayout.implicitHeight + radius * 2

                        color: "#22FF0000"
                        radius: 5

                        RowLayout {
                            id: crashRecoveryLayout
                            anchors.fill: parent
                            anchors.leftMargin: parent.radius * 2
                            anchors.rightMargin: parent.radius * 2
                            Label {
                                id: restartWelcomeLabel
                                text: KI18n.i18n("Kdenlive crashed on last start. Reset config and restart?")
                                Layout.alignment: Qt.AlignVCenter
                                wrapMode: Text.Wrap
                            }
                            Button {
                                id: restartWelcomeButton
                                enabled: splash.actionsEnabled
                                text: KI18n.i18n("Reset")
                                icon.name: "view-refresh"
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                onClicked: splash.resetConfig()
                            }
                        }
                    }

                }

                RowLayout {
                    Button {
                        id: buttonHelp
                        icon.name: "help-browser"
                        text: KI18n.i18n("Check Online Documentation")
                        enabled: splash.actionsEnabled
                        Layout.alignment: Qt.AlignLeft
                        onClicked: splash.openLink("https://docs.kdenlive.org?mtm_campaign=kdenlive_inapp&mtm_kwd=splash_dcumentation")
                    }

                    Button {
                        id: buttonNext
                        icon.name: "go-next"
                        text: KI18n.i18n("Start Editing")
                        enabled: splash.actionsEnabled
                        onClicked: {
                            splash.firstStart(profileCombo.currentValue, fpsCombo.currentValue, verticalFrame.checked ? false : interlacedSwitch.enabled ? interlacedSwitch.checked : true, vTracks.value, aTracks.value)
                            splash.hide()
                        }
                        Layout.alignment: Qt.AlignRight
                    }

                }
            }

        }

        Button {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.rightMargin: 8
            icon.name: "window-close"
            enabled: splash.actionsEnabled
            onClicked: splash.closeApp()
            opacity: 0.6
        }

    }

}
