import QtQuick 2.11
import QtQuick.Shapes 1.11

Shape {
    anchors.fill: parent
    asynchronous: true
    opacity: 0.4
    ShapePath {
        fillColor: "#000"
        strokeColor: "transparent"
        PathLine {x: 0; y: 0}
        PathLine {x: mixCutPos.x; y: mixBackground.height}
        PathLine {x: 0; y: mixBackground.height}
        PathLine {x: 0; y: 0}
    }
    ShapePath {
        fillColor: "#000"
        strokeColor: "transparent"
        PathLine {x: mixBackground.width; y: 0}
        PathLine {x: mixBackground.width; y: mixBackground.height}
        PathLine {x: mixCutPos.x; y: mixBackground.height}
        PathLine {x: mixBackground.width; y: 0}
    }
}
