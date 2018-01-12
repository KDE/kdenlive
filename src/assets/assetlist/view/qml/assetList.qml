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

import QtQuick 2.7
import QtQuick.Layouts 1.3
import QtQuick.Controls 1.5
import QtQuick.Controls.Styles 1.4
import QtQuick.Window 2.2
import QtQml.Models 2.2

Rectangle {
    id: listRoot
    SystemPalette { id: activePalette }
    color: activePalette.window

    function assetType(){
        return isEffectList ? i18n("effects") : i18n("transitions");
    }

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
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 6
            ExclusiveGroup { id: filterGroup}
            ToolButton {
                id: searchList
                implicitWidth: 40
                implicitHeight: 40
                iconName: "edit-find"
                checkable: true
                tooltip: isEffectList ? i18n('Find effect') : i18n('Find composition')
                onCheckedChanged: {
                    searchInput.visible = searchList.checked
                    searchInput.focus = searchList.checked
                    if (!searchList.checked) {
                        searchInput.text = ''
                        treeView.focus = true
                    }
                }
            }
            ToolButton {
                id: showAll
                implicitWidth: 40
                implicitHeight: 40
                iconName: "show-all-effects"
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all ')+assetType()
                onClicked: {
                    assetlist.setFilterType("")
                }
            }
            ToolButton {
                id: showVideo
                visible: isEffectList
                implicitWidth: 40
                implicitHeight: 40
                iconName: "kdenlive-show-video"
                iconSource: 'qrc:///pics/kdenlive-show-video.svgz'
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all video effects')
                onClicked: {
                    assetlist.setFilterType("video")
                }
            }
            ToolButton {
                id: showAudio
                visible: isEffectList
                implicitWidth: 40
                implicitHeight: 40
                iconName: "kdenlive-show-audio"
                iconSource: 'qrc:///pics/kdenlive-show-audio.svgz'
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all audio effects')
                onClicked: {
                    assetlist.setFilterType("audio")
                }
            }
            ToolButton {
                id: showCustom
                visible: isEffectList
                implicitWidth: 40
                implicitHeight: 40
                iconName: "kdenlive-custom-effect"
                checkable:true
                exclusiveGroup: filterGroup
                tooltip: i18n('Show all custom effects')
                onClicked: {
                    assetlist.setFilterType("custom")
                }
            }
            Rectangle {
                //This is a spacer
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "transparent"
            }
            ToolButton {
                id: showDescription
                implicitWidth: 40
                implicitHeight: 40
                iconName: "help-about"
                checkable:true
                tooltip: i18n('Show/hide description of the ') + assetType()
                onCheckedChanged:{
                    assetlist.showDescription = checked
                    if (!checked) {
                        assetDescription.visible = false
                    }
                }
                Component.onCompleted: checked = assetlist.showDescription
            }

        }
        TextField {
            id: searchInput
            Layout.fillWidth:true
            visible: false
            Image {
                id: clear
                source: 'image://icon/edit-clear'
                width: parent.height * 0.8
                height: width
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                opacity: 0
                MouseArea {
                    anchors.fill: parent
                    onClicked: { searchInput.text = ''; searchInput.focus = true; searchList.checked = false; }
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
            onEditingFinished: {
                searchList.checked = false
            }
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
                    searchList.checked = false
                }
            }
        }
        ItemSelectionModel {
            id: sel
            model: assetListModel
        }

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
                property bool isItem : styleData.value != "root" && styleData.value != ""
                property string mimeType : isItem ? assetlist.getMimeType(styleData.value) : ""
                height: text.implicitHeight + 8
                color: "transparent"

                Drag.active: isItem ? dragArea.drag.active : false
                Drag.dragType: Drag.Automatic
                Drag.supportedActions: Qt.CopyAction
                Drag.mimeData: isItem ? assetlist.getMimeData(styleData.value) : {}
                Drag.keys:[
                    isItem ? assetlist.getMimeType(styleData.value) : ""
                ]

                Row {
                    anchors.fill:parent
                    anchors.leftMargin: 2
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    spacing: 2
                    Image{
                        visible: assetDelegate.isItem
                        height: parent.height
                        width: height
                        source: 'image://asseticon/' + styleData.value
                    }
                    Label {
                        id: text
                        text: assetlist.getName(styleData.index)
                    }
                }

                MouseArea {
                    id: dragArea
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: parent
                    onPressed: {
                        if (isItem) {
                            assetDescription.text = assetlist.getDescription(styleData.index)
                            parent.grabToImage(function(result) {
                                parent.Drag.imageSource = result.url
                            })
                            sel.setCurrentIndex(styleData.index, ItemSelectionModel.ClearAndSelect)
                            console.log(parent.Drag.keys)
                        } else {
                            if (treeView.isExpanded(styleData.index)) {
                                treeView.collapse(styleData.index)
                            } else {
                                treeView.expand(styleData.index)
                            }

                        }
                    }
                    onDoubleClicked: {
                        if (isItem) {
                            assetlist.activate(styleData.index)
                        }
                    }
                }
            }

            TableViewColumn { role: "identifier"; title: "Name"; }
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
            visible: false
            readOnly: true
            Layout.fillWidth: true
            states: State {
                name: "hasDescription"; when: assetDescription.text != '' && showDescription.checked
                PropertyChanges { target: assetDescription; visible: true}
            }

        }
    }
}
