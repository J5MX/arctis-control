import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Banda del ecualizador: deslizador vertical de -10 a +10 dB en pasos de 0,5.
ColumnLayout {
    id: band

    property string label
    property alias value: slider.value
    readonly property real sliderCenter: slider.y + slider.height / 2
    signal moved()

    spacing: 8

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: (slider.value > 0 ? "+" : "") + slider.value.toLocaleString(Qt.locale("es_ES"), "f", slider.value % 1 === 0 ? 0 : 1)
        color: Theme.accent
        font.family: Theme.display
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    Slider {
        id: slider
        Layout.alignment: Qt.AlignHCenter
        orientation: Qt.Vertical
        from: -10
        to: 10
        stepSize: 0.5
        snapMode: Slider.SnapAlways
        implicitWidth: 28
        implicitHeight: 170
        onMoved: band.moved()

        background: Item {
            x: slider.leftPadding + slider.availableWidth / 2 - 2
            y: slider.topPadding
            width: 4
            height: slider.availableHeight

            Rectangle { anchors.fill: parent; radius: 2; color: "#2a2a31" }
            // Relleno naranja desde 0 dB hasta el tirador
            Rectangle {
                readonly property real center: parent.height / 2
                readonly property real handleY: slider.visualPosition * parent.height
                width: parent.width
                radius: 2
                color: Theme.accent
                y: Math.min(center, handleY)
                height: Math.abs(center - handleY)
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.availableWidth / 2 - width / 2
            y: slider.topPadding + slider.visualPosition * (slider.availableHeight - height)
            width: 16
            height: 16
            radius: 8
            color: slider.pressed ? Theme.accentHi : Theme.accent
            border.color: Theme.bg
            border.width: 3
        }
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: band.label
        color: Theme.muted
        font.family: Theme.display
        font.pixelSize: 11
        font.weight: Font.Medium
    }
}
