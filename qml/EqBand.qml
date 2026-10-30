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

    function format(v: real): string {
        const sign = v > 0 ? "+" : v < 0 ? "-" : ""
        const a = Math.abs(v)
        const whole = Math.floor(a)
        const tenth = Math.round((a - whole) * 10)
        return sign + whole + (tenth > 0 ? "," + tenth : "")
    }

    spacing: 8

    Text {
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        text: band.format(slider.value)
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
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        text: band.label
        color: Theme.muted
        font.family: Theme.display
        font.pixelSize: 11
        font.weight: Font.Medium
    }
}
