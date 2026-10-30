import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes

// Botón con corte biselado en dos esquinas.
Button {
    id: control

    property bool primary: false
    property bool active: false
    readonly property int cut: 8

    padding: 0
    leftPadding: 18
    rightPadding: 18
    topPadding: 11
    bottomPadding: 11
    hoverEnabled: true
    opacity: enabled ? 1 : 0.5

    contentItem: Text {
        text: control.text.toUpperCase()
        color: control.primary ? Theme.bg : control.active ? Theme.accent : Theme.text
        font.family: Theme.display
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.letterSpacing: 1.4
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Item {
        implicitWidth: 80
        implicitHeight: 36

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 0
                fillColor: control.primary
                    ? (control.hovered ? Theme.accentHi : Theme.accent)
                    : control.active ? Theme.accentDim
                    : control.hovered ? Theme.controlHi : Theme.control
                startX: control.cut; startY: 0
                PathLine { x: control.width; y: 0 }
                PathLine { x: control.width; y: control.height - control.cut }
                PathLine { x: control.width - control.cut; y: control.height }
                PathLine { x: 0; y: control.height }
                PathLine { x: 0; y: control.cut }
                PathLine { x: control.cut; y: 0 }
            }
        }
        Rectangle {
            visible: control.active
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; rightMargin: control.cut }
            height: 2
            color: Theme.accent
        }
        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            anchors.margins: -3
            color: "transparent"
            border.color: Theme.accentHi
            border.width: 2
        }
    }
}
