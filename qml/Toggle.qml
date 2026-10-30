import QtQuick
import QtQuick.Controls.Basic

// Interruptor cuadrado que se enciende en naranja.
Switch {
    id: control

    padding: 0
    spacing: 0
    // Sin texto ni relleno, Basic calcularía un ancho de 0.
    implicitWidth: indicator.implicitWidth
    implicitHeight: indicator.implicitHeight

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 24
        color: control.checked ? Theme.accentDim : "#2a2a31"
        border.color: control.checked ? Theme.accent : "transparent"
        border.width: 1
        Behavior on color { ColorAnimation { duration: 150 } }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 18
            height: 18
            color: control.checked ? Theme.accent : Theme.muted
            Behavior on x { NumberAnimation { duration: 150 } }
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
    contentItem: Item {}
}
