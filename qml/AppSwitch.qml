import QtQuick
import QtQuick.Layouts

// On/off option of the app itself (not of the headset): bound to AppController.
RowLayout {
    id: row

    property string title
    property string subtitle
    property bool checked
    signal toggled(bool on)

    Layout.fillWidth: true
    spacing: 12

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
        checked: row.checked
        onToggled: {
            row.toggled(checked)
            checked = Qt.binding(() => row.checked) // follow the real setting
        }
    }
}
