import QtQuick
import QtQuick.Layouts

// On/off setting: title, description and switch.
RowLayout {
    id: row

    required property string key
    property string title
    property string subtitle

    function sync() {
        toggle.checked = HeadsetController.setting(row.key) === 1
    }

    Layout.fillWidth: true
    spacing: 12
    Component.onCompleted: sync()

    Connections {
        target: HeadsetController
        function onStateChanged() { row.sync() }
        function onNotify(message: string, error: bool) { if (error) row.sync() }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: 2
        Text {
            textFormat: Text.PlainText
            text: row.title
            color: Theme.text
            font.weight: Font.Medium
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
    }
    Toggle {
        id: toggle
        enabled: !HeadsetController.busy
        onToggled: HeadsetController.apply(row.key, checked ? 1 : 0)
    }
}
