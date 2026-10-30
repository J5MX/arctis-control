pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// Ajuste con pocas opciones (0, 1, 2…): una fila de botones.
ColumnLayout {
    id: row

    required property string key
    property string title
    property list<string> options
    property int current: -1

    function sync() {
        row.current = HeadsetController.setting(row.key)
    }

    Layout.fillWidth: true
    spacing: 8
    Component.onCompleted: sync()

    Connections {
        target: HeadsetController
        function onStateChanged() { row.sync() }
    }

    Text {
        textFormat: Text.PlainText
        text: row.title
        color: Theme.text
        font.weight: Font.Medium
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: row.options
            CutButton {
                id: option
                required property string modelData
                required property int index
                text: option.modelData
                active: row.current === option.index
                enabled: !HeadsetController.busy
                onClicked: HeadsetController.apply(row.key, option.index)
            }
        }
    }
}
