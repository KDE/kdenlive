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
import QtQuick.Layouts 1.11
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import QtQml.Models 2.11
import com.enums 1.0

Rectangle {
    id: listRoot
    SystemPalette { id: activePalette }
    color: activePalette.window

    function expandNodes(indexes)  {
        for(var i = 0; i < indexes.length; i++) {
            if (indexes[i].valid) {
                treeView.expand(indexes[i]);
            }
        }
    }
    function rowPosition(model, index) {
        var pos = 0;
        for(var i = 0; i < index.parent.row; i++) {
            var catIndex = model.getCategory(i);
            if (treeView.isExpanded(catIndex)) {
                pos += model.rowCount(catIndex);
            }
            pos ++;
        }
        pos += index.row + 2;
        return pos;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 4
            ExclusiveGroup { id: filterGroup}
            ToolButton {
                id: showAll
                iconName: "show-all-effects"
                checkable: true
                checked: true
                exclusiveGroup: filterGroup
                tooltip: isEffectList ? i18n("Main effects") : i18n("Main compositions")
                onClicked: {
                    assetlist.setFilterType("")
                }
            }
            ToolButton {
                id: showVideo
                visible: isEffectList
                iconName: "kdenlive-show-video"
                iconSource: 'image://icon/kdenlive-show-video'
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n("Show all video effects")
                onClicked: {
                    assetlist.setFilterType("video")
                }
            }
            ToolButton {
                id: showAudio
                visible: isEffectList
                iconName: "kdenlive-show-audio"
                iconSource: 'image://icon/kdenlive-show-audio'
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n("Show all audio effects")
                onClicked: {
                    assetlist.setFilterType("audio")
                }
            }
            ToolButton {
                id: showCustom
                visible: isEffectList
                iconName: "kdenlive-custom-effect"
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n("Show all custom effects")
                onClicked: {
                    assetlist.setFilterType("custom")
                }
            }
            ToolButton {
                id: showFavorites
                iconName: "favorite"
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n("Show favorite items")
                onClicked: {
                    assetlist.setFilterType("favorites")
                }
            }
            ToolButton {
                id: downloadTransitions
                visible: !isEffectList
                iconName: "edit-download"
                tooltip: i18n("Download New Wipes...")
                onClicked: {
                    assetlist.downloadNewLumas()
                }
            }
            Rectangle {
                //This is a spacer
                Layout.fillHeight: false
                Layout.fillWidth: true
                color: "transparent"
            }
            ToolButton {
                id: showDescription
                iconName: "help-about"
                checkable:true
                tooltip: isEffectList ? i18n("Show/hide description of the effects") : i18n("Show/hide description of the compositions")
                onCheckedChanged:{
                    assetlist.showDescription = checked
                }
                Component.onCompleted: checked = assetlist.showDescription
            }

        }
        TextField {
            id: searchInput
            Layout.fillWidth:true
            Image {
                id: clear
                source: 'image://icon/edit-clear'
                width: parent.height * 0.8
                height: width
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                opacity: 0
                MouseArea {
                    anchors.fill: parent
                    onClicked: { searchInput.text = ''; searchInput.focus = true; }
                }
            }
            states: State {
                name: "hasText"; when: searchInput.text != ''
                PropertyChanges { target: clear; opacity: 1 }
            }

            transitions: [
                Transition {
                    from: ""; to: "hasText"
                    NumberAnimation { properties: "opacity" }
                },
                Transition {
                    from: "hasText"; to: ""
                    NumberAnimation { properties: "opacity" }
                }
            ]
            onTextChanged: {
                var current = sel.currentIndex
                var rowModelIndex = assetListModel.getModelIndex(sel.currentIndex);
                assetlist.setFilterName(text)
                if (text.length > 0) {
                    sel.setCurrentIndex(assetListModel.firstVisibleItem(current), ItemSelectionModel.ClearAndSelect)
                } else {
                    sel.clearCurrentIndex()
                    sel.setCurrentIndex(assetListModel.getProxyIndex(rowModelIndex), ItemSelectionModel.ClearAndSelect)
                }
                treeView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            /*onEditingFinished: {
                if (!assetContextMenu.isDisplayed) {
                    searchList.checked = false
                }
            }*/
            Keys.onDownPressed: {
                sel.setCurrentIndex(assetListModel.getNextChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                treeView.expand(sel.currentIndex.parent)
                treeView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            Keys.onUpPressed: {
                sel.setCurrentIndex(assetListModel.getPreviousChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                treeView.expand(sel.currentIndex.parent)
                treeView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            Keys.onPressed: {
                if (sel.hasSelection && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter )) {
                    assetlist.activate(sel.currentIndex)
                    treeView.focus = true
                    event.accepted = true
                    searchInput.text = '';
                }
            }
        }
        ItemSelectionModel {
            id: sel
            model: assetListModel
            onSelectionChanged: {
                assetDescription.text = assetlist.getDescription(sel.currentIndex)
            }
        }
        SplitView {
            orientation: Qt.Vertical
            Layout.fillHeight: true
            Layout.fillWidth: true
        TreeView {
            id: treeView
            Layout.fillHeight: true
            Layout.fillWidth: true
            alternatingRowColors: false
            headerVisible: false
            selection: sel
            selectionMode: SelectionMode.SingleSelection
            itemDelegate: Rectangle {
                id: assetDelegate
                // These anchors are important to allow "copy" dragging
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                anchors.right: parent ? parent.right : undefined
                property bool isItem : styleData.value !== "root" && styleData.value !== ""
                property string mimeType : isItem ? assetlist.getMimeType(styleData.value) : ""
                height: assetText.implicitHeight
                color: dragArea.containsMouse ? activePalette.highlight : "transparent"

                Drag.active: isItem ? dragArea.drag.active : false
                Drag.dragType: Drag.Automatic
                Drag.supportedActions: Qt.CopyAction
                Drag.mimeData: isItem ? assetlist.getMimeData(styleData.value) : {}
                Drag.keys:[
                    isItem ? assetlist.getMimeType(styleData.value) : ""
                ]

                Row {
                    anchors.fill:parent
                    anchors.leftMargin: 1
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    spacing: 4
                    Image{
                        id: assetThumb
                        anchors.verticalCenter: parent.verticalCenter
                        visible: assetDelegate.isItem
                        property bool isFavorite: model == undefined || model.favorite === undefined ? false : model.favorite
                        property bool isCustom: model == undefined ? false : model.type == AssetType.Custom || model.type == AssetType.CustomAudio
                        height: parent.height * 0.8
                        width: height
                        source: assetText.text == '' ? '' : 'image://asseticon/' + assetText.text + '/' + model.type
                    }
                    Label {
                        id: assetText
                        font.bold : assetThumb.isFavorite
                        text: assetlist.getName(styleData.index)
                    }
                }
                MouseArea {
                    id: dragArea
                    anchors.fill: assetDelegate
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    drag.target: undefined
                    onReleased: {
                        drag.target = undefined
                    }
                    onPressed: {
                        if (assetDelegate.isItem) {
                            //sel.select(styleData.index, ItemSelectionModel.Select)
                            sel.setCurrentIndex(styleData.index, ItemSelectionModel.ClearAndSelect)
                            if (mouse.button === Qt.LeftButton) {
                                drag.target = parent
                                parent.grabToImage(function(result) {
                                    parent.Drag.imageSource = result.url
                                })
                            } else {
                                drag.target = undefined
                                assetContextMenu.isItemFavorite = assetThumb.isFavorite
                                assetContextMenu.isCustom = assetThumb.isCustom
                                assetContextMenu.popup()
                                mouse.accepted = false
                            }
                            console.log(parent.Drag.keys)
                        } else {
                            if (treeView.isExpanded(styleData.index)) {
                                treeView.collapse(styleData.index)
                            } else {
                                treeView.expand(styleData.index)
                            }

                        }
                        treeView.focus = true
                    }
                    onDoubleClicked: {
                        if (isItem) {
                            assetlist.activate(styleData.index)
                        }
                    }
                }
            }
            Menu {
                id: assetContextMenu
                property bool isItemFavorite
                property bool isCustom: false
                MenuItem {
                    text: assetContextMenu.isItemFavorite ? i18n("Remove from favorites") : i18n("Add to favorites")
                    onTriggered: {
                        assetlist.setFavorite(sel.currentIndex, !assetContextMenu.isItemFavorite)
                    }
                }
                MenuItem {
                    id: removeMenu
                    text: i18n("Delete custom effect")
                    visible: isEffectList && assetContextMenu.isCustom
                    onTriggered: {
                        assetlist.deleteCustomEffect(sel.currentIndex)
                    }
                }
            }

            TableViewColumn { role: "identifier"; title: i18n("Name"); }
            model: assetListModel

            Keys.onDownPressed: {
                sel.setCurrentIndex(assetListModel.getNextChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                treeView.expand(sel.currentIndex.parent)
                treeView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            Keys.onUpPressed: {
                sel.setCurrentIndex(assetListModel.getPreviousChild(sel.currentIndex), ItemSelectionModel.ClearAndSelect)
                treeView.expand(sel.currentIndex.parent)
                treeView.__listView.positionViewAtIndex(rowPosition(assetListModel, sel.currentIndex), ListView.Visible)
            }
            Keys.onReturnPressed: {
                if (sel.hasSelection) {
                    assetlist.activate(sel.currentIndex)
                }
            }

        }
        TextArea {
            id: assetDescription
            text: ""
            visible: showDescription.checked
            readOnly: true
            Layout.fillWidth: true
            height: font.pixelSize * 5
            states: State {
                name: "hasDescription"; when: assetDescription.text != '' && showDescription.checked
                PropertyChanges { target: assetDescription; visible: true}
            }
        }

        }
    }
}
