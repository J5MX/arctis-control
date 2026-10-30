import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Numeric setting. Applied when the slider is released, not while dragging.
ColumnLayout {
    id: row

    required property string key
    property string title
    property string subtitle
    property int from: 0
    property int to: 100
    property string unit
    property bool known: false

    function sync() {
        if (slider.pressed)
            return
        const v = HeadsetController.setting(row.key)
        row.known = v >= 0
        slider.value = row.known ? v : row.from
    }

    Layout.fillWidth: true
    spacing: 6
    Component.onCompleted: sync()

    Connections {
        target: HeadsetController
        function onStateChanged() { row.sync() }
        // Some headsets report the current value (e.g. sidetone).
        function onStatusChanged() { row.sync() }
        function onNotify(message: string, error: bool) { if (error) row.sync() }
    }

    RowLayout {
        Layout.fillWidth: true
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: row.title
            color: Theme.text
            font.weight: Font.Medium
        }
        Text {
            textFormat: Text.PlainText
            text: row.known || slider.pressed ? Math.round(slider.value) + row.unit : "—"
            color: Theme.accent
            font.family: Theme.display
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
    }
    Text {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        visible: row.subtitle !== ""
        wrapMode: Text.WordWrap
        text: row.subtitle
        color: Theme.muted
        font.pixelSize: 13
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: row.from
        to: row.to
        stepSize: 1
        snapMode: Slider.SnapAlways
        enabled: !HeadsetController.busy
        onPressedChanged: {
            if (!pressed && Math.round(value) !== HeadsetController.setting(row.key))
                HeadsetController.apply(row.key, Math.round(value))
        }

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: "#2a2a31"
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: row.known || slider.pressed ? Theme.accent : Theme.muted
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16
            height: 16
            radius: 8
            color: slider.pressed ? Theme.accentHi : row.known ? Theme.accent : Theme.muted
            border.color: Theme.bg
            border.width: 3
        }
    }
}
