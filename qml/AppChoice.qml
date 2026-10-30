pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

// Option of the app itself with a few choices: a row of buttons.
ColumnLayout {
    id: row

    property string title
    property string subtitle
    property list<string> options
    property int current: 0
    signal chosen(int index)

    Layout.fillWidth: true
    spacing: 8

    Text {
        textFormat: Text.PlainText
        text: row.title
        color: row.enabled ? Theme.text : Theme.muted
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
                onClicked: row.chosen(option.index)
            }
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
}
