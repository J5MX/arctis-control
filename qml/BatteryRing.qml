import QtQuick
import QtQuick.Shapes

// Anillo de batería: naranja normal, amarillo ≤ 35 %, rojo ≤ 15 %.
Item {
    id: ring

    property int level: -1
    readonly property color tone: level < 0 ? Theme.muted
        : level <= 15 ? Theme.bad
        : level <= 35 ? Theme.warn
        : Theme.accent

    implicitWidth: 104
    implicitHeight: 104

    // Halo suave alrededor del anillo
    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 14
        height: width
        radius: width / 2
        color: "transparent"
        border.color: ring.tone
        border.width: 7
        opacity: ring.level < 0 ? 0 : 0.12
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "#222228"
            strokeWidth: 10
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            PathAngleArc {
                centerX: ring.width / 2; centerY: ring.height / 2
                radiusX: ring.width / 2 - 5; radiusY: ring.height / 2 - 5
                startAngle: 0; sweepAngle: 360
            }
        }
        ShapePath {
            strokeColor: ring.tone
            strokeWidth: 10
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            PathAngleArc {
                centerX: ring.width / 2; centerY: ring.height / 2
                radiusX: ring.width / 2 - 5; radiusY: ring.height / 2 - 5
                startAngle: -90
                sweepAngle: Math.max(0, ring.level) * 3.6
                Behavior on sweepAngle { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
            }
        }
    }

    Text {
        textFormat: Text.PlainText
        anchors.centerIn: parent
        text: ring.level < 0 ? "–" : ring.level + "%"
        color: Theme.text
        font.family: Theme.display
        font.pixelSize: 26
        font.weight: Font.Bold
    }
}
