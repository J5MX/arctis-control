pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

ApplicationWindow {
    id: root

    property bool eqDirty: false
    readonly property list<string> standardFrequencies: ["32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]

    readonly property bool hasGeneral: has("CAP_INACTIVE_TIME") || has("CAP_VOLUME_LIMITER") || has("CAP_LIGHTS")
        || has("CAP_VOICE_PROMPTS") || has("CAP_BT_WHEN_POWERED_ON") || has("CAP_BT_CALL_VOLUME")
        || has("CAP_NOTIFICATION_SOUND")
    readonly property bool hasEq: has("CAP_EQUALIZER") || has("CAP_EQUALIZER_PRESET")
    readonly property bool hasMic: has("CAP_SIDETONE") || has("CAP_MICROPHONE_VOLUME") || has("CAP_ROTATE_TO_MUTE")
        || has("CAP_MICROPHONE_MUTE_LED_BRIGHTNESS") || has("CAP_NOISE_FILTER")

    width: 1000
    height: 860
    minimumWidth: 720
    minimumHeight: 560
    visible: true
    title: HeadsetController.connected ? "Arctis Control · " + HeadsetController.deviceName : "Arctis Control"
    color: Theme.bg
    font.family: "Noto Sans"
    font.pixelSize: 14

    function has(cap: string): bool {
        return HeadsetController.capabilities.indexOf(cap) >= 0
    }

    function presetLabel(name: string): string {
        switch (name) {
        case "flat": return qsTr("Flat")
        case "bass": return qsTr("Bass")
        case "focus": return qsTr("Focus")
        case "smiley": return qsTr("Smiley")
        default: return name.charAt(0).toUpperCase() + name.slice(1)
        }
    }

    function number(value: real): string {
        return String(value).replace(".", Qt.locale().decimalPoint)
    }

    function bandLabel(index: int): string {
        return HeadsetController.eqBands === 10 ? standardFrequencies[index] : "B" + (index + 1)
    }

    function setBands(values: list<real>) {
        const n = Math.min(bands.count, values.length)
        for (let i = 0; i < n; ++i)
            (bands.itemAt(i) as EqBand).value = values[i]
    }

    function bandValues(): list<real> {
        let values = []
        for (let i = 0; i < bands.count; ++i)
            values.push((bands.itemAt(i) as EqBand).value)
        return values
    }

    // The headset cannot report most settings: show the last ones applied.
    function syncControls() {
        inactive.currentIndex = Math.max(0, inactive.indexOfValue(HeadsetController.setting("inactive_time")))
        if (!eqDirty)
            setBands(HeadsetController.activeBands)
    }

    Connections {
        target: HeadsetController
        function onStateChanged() { root.syncControls() }
        function onDeviceChanged() {
            root.eqDirty = false
            Qt.callLater(root.syncControls) // after the new headset's bands exist
        }
        function onNotify(message: string, error: bool) {
            toast.show(message, error)
            if (error)
                root.syncControls()
        }
    }

    Component.onCompleted: syncControls()

    // Do not poll the headset while the window is minimized.
    Binding {
        target: HeadsetController
        property: "polling"
        value: root.visible && root.visibility !== Window.Minimized
    }

    // Orange glow in the top-right corner
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeWidth: 0
            fillGradient: RadialGradient {
                centerX: root.width; centerY: -40
                centerRadius: 520
                focalX: centerX; focalY: centerY
                GradientStop { position: 0; color: "#1aff5200" }
                GradientStop { position: 1; color: "transparent" }
            }
            PathRectangle { width: root.width; height: root.height }
        }
    }

    header: Rectangle {
        implicitHeight: 74
        color: Theme.header

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 14

            Image {
                source: "qrc:/qt/qml/ArctisControl/assets/icon.svg"
                sourceSize: Qt.size(80, 80)
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
            }
            ColumnLayout {
                spacing: 4
                Text {
                    textFormat: Text.PlainText
                    text: "STEELSERIES"
                    color: Theme.accent
                    font.family: Theme.display
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 3.5
                }
                Text {
                    textFormat: Text.PlainText
                    text: HeadsetController.connected ? HeadsetController.deviceName : "Arctis Control"
                    color: Theme.text
                    font.family: Theme.display
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    font.letterSpacing: 0.9
                    font.capitalization: Font.AllUppercase
                }
            }
            Item { Layout.fillWidth: true }

            // Picker when more than one headset is connected
            ComboBox {
                id: devicePicker
                visible: HeadsetController.deviceNames.length > 1
                model: HeadsetController.deviceNames
                currentIndex: HeadsetController.currentDevice
                onActivated: (index) => HeadsetController.currentDevice = index
                implicitWidth: 220
                implicitHeight: 32
                background: Rectangle { color: devicePicker.hovered ? Theme.controlHi : Theme.control }
                contentItem: Text {
                    textFormat: Text.PlainText
                    leftPadding: 12
                    rightPadding: 24
                    text: devicePicker.displayText
                    elide: Text.ElideRight
                    color: Theme.text
                    font.family: Theme.display
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                }
                indicator: Text {
                    textFormat: Text.PlainText
                    x: devicePicker.width - width - 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "▾"
                    color: Theme.accent
                }
            }

            // Connection status
            Item {
                id: pill
                implicitWidth: pillText.implicitWidth + 40
                implicitHeight: 28
                Shape {
                    anchors.fill: parent
                    ShapePath {
                        strokeWidth: 0
                        fillColor: Theme.cardHi
                        startX: 8; startY: 0
                        PathLine { x: pill.width; y: 0 }
                        PathLine { x: pill.width - 8; y: pill.height }
                        PathLine { x: 0; y: pill.height }
                        PathLine { x: 8; y: 0 }
                    }
                }
                Rectangle {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 4
                    color: HeadsetController.connected ? Theme.accent : Theme.bad
                }
                Text {
                    textFormat: Text.PlainText
                    id: pillText
                    x: 28
                    anchors.verticalCenter: parent.verticalCenter
                    text: HeadsetController.connected ? qsTr("CONNECTED") : qsTr("DISCONNECTED")
                    color: HeadsetController.connected ? Theme.accent : Theme.bad
                    font.family: Theme.display
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.5
                }
            }
        }

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Theme.accent }
                GradientStop { position: 0.3; color: Theme.accent }
                GradientStop { position: 1; color: "transparent" }
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        GridLayout {
            id: grid
            width: parent.width - 48
            x: 24
            y: 22
            columns: root.width >= 900 ? 3 : 1
            columnSpacing: 16
            rowSpacing: 16

            // No headset: what to check
            Card {
                visible: !HeadsetController.connected
                title: qsTr("No headset detected")
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: HeadsetController.errorText || qsTr("Looking for headsets…")
                    color: Theme.text
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    lineHeight: 1.3
                    text: qsTr("• Plug in the USB receiver (or the cable) and turn the headset on.\n"
                        + "• Check that HeadsetControl sees it: headsetcontrol -b\n"
                        + "• If it reports missing permissions, replug the receiver after installing headsetcontrol (it installs udev rules).\n"
                        + "• Supported: Arctis 1, 7, 7+, 7X, 7P, 9, Pro (2019 and Wireless), GameBuds and Nova 3, 3P, 5, 5X, 7, 7P and Pro Wireless.")
                }
            }

            // Headset illustration and details
            Card {
                visible: HeadsetController.connected
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                Item { Layout.fillHeight: true }
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(230, parent.width)
                    Layout.preferredHeight: Layout.preferredWidth * 160 / 240
                    source: "qrc:/qt/qml/ArctisControl/assets/headset.svg"
                    sourceSize: Qt.size(460, 307)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.alignment: Qt.AlignHCenter
                    text: HeadsetController.deviceName
                    color: Theme.text
                    font.family: Theme.display
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.alignment: Qt.AlignHCenter
                    text: "USB " + HeadsetController.deviceId
                        // ChatMix only when the mix is off-centre (i.e. actually in use).
                        + (root.has("CAP_CHATMIX_STATUS") && HeadsetController.chatmix >= 0 && HeadsetController.chatmix !== 64
                           ? " · ChatMix " + (HeadsetController.chatmix < 64 ? qsTr("game") : qsTr("chat")) + " "
                             + Math.round(Math.abs(HeadsetController.chatmix - 64) / 64 * 100) + " %" : "")
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                visible: HeadsetController.connected
                title: qsTr("Battery")
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20
                    BatteryRing {
                        level: root.has("CAP_BATTERY_STATUS") ? HeadsetController.batteryLevel : -1
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: !root.has("CAP_BATTERY_STATUS") ? qsTr("Wired")
                                : HeadsetController.batteryStatus === "BATTERY_CHARGING" ? qsTr("⚡ Charging")
                                : HeadsetController.batteryLevel >= 0 ? qsTr("In use")
                                : qsTr("Off or out of range")
                            color: Theme.text
                            font.family: Theme.display
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.9
                            font.capitalization: Font.AllUppercase
                        }
                        Text {
                            textFormat: Text.PlainText
                            visible: HeadsetController.batteryMinutes > 0
                            text: qsTr("≈ %1 h %2 min left").arg(Math.floor(HeadsetController.batteryMinutes / 60))
                                .arg(HeadsetController.batteryMinutes % 60)
                            color: Theme.text
                        }
                        Text {
                            textFormat: Text.PlainText
                            visible: HeadsetController.updatedText !== ""
                            text: qsTr("Updated %1").arg(HeadsetController.updatedText)
                            color: Theme.muted
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                visible: HeadsetController.connected && root.hasGeneral
                title: qsTr("Settings")
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                RowLayout {
                    visible: root.has("CAP_INACTIVE_TIME")
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 2
                        Text { textFormat: Text.PlainText; text: qsTr("Auto power-off"); color: Theme.text; font.weight: Font.Medium }
                        Text { textFormat: Text.PlainText; text: qsTr("After this long without use"); color: Theme.muted; font.pixelSize: 13 }
                    }
                    ComboBox {
                        id: inactive
                        enabled: !HeadsetController.busy
                        textRole: "label"
                        valueRole: "value"
                        model: ListModel {
                            ListElement { label: QT_TR_NOOP("Never"); value: 0 }
                            ListElement { label: "1 min"; value: 1 }
                            ListElement { label: "5 min"; value: 5 }
                            ListElement { label: "10 min"; value: 10 }
                            ListElement { label: "15 min"; value: 15 }
                            ListElement { label: "30 min"; value: 30 }
                            ListElement { label: "60 min"; value: 60 }
                            ListElement { label: "90 min"; value: 90 }
                        }
                        onActivated: HeadsetController.apply("inactive_time", currentValue)
                        implicitWidth: 100
                        implicitHeight: 34

                        background: Rectangle {
                            color: inactive.hovered ? Theme.controlHi : Theme.control
                            Rectangle {
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                                height: 2
                                color: Theme.accent
                            }
                        }
                        contentItem: Text {
                            textFormat: Text.PlainText
                            leftPadding: 12
                            text: HeadsetController.setting("inactive_time") < 0 && HeadsetController.connected ? "—" : qsTr(inactive.displayText)
                            color: Theme.text
                            font.family: Theme.display
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            verticalAlignment: Text.AlignVCenter
                        }
                        indicator: Text {
                            textFormat: Text.PlainText
                            x: inactive.width - width - 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▾"
                            color: Theme.accent
                        }
                        delegate: ItemDelegate {
                            id: entry
                            required property string label
                            required property int index
                            width: inactive.width
                            highlighted: inactive.highlightedIndex === entry.index
                            contentItem: Text {
                                textFormat: Text.PlainText
                                text: qsTr(entry.label)
                                color: entry.highlighted ? Theme.accent : Theme.text
                                font.family: Theme.display
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            background: Rectangle { color: entry.highlighted ? Theme.accentDim : Theme.cardHi }
                        }
                        popup.background: Rectangle { color: Theme.cardHi; border.color: Theme.line }
                    }
                }

                SettingSwitch {
                    visible: root.has("CAP_VOLUME_LIMITER")
                    key: "volume_limiter"
                    title: qsTr("Volume limiter")
                    subtitle: qsTr("Protects your ears from loud peaks")
                }
                SettingSwitch {
                    visible: root.has("CAP_LIGHTS")
                    key: "lights"
                    title: qsTr("Lights")
                    subtitle: qsTr("Headset lighting")
                }
                SettingSwitch {
                    visible: root.has("CAP_VOICE_PROMPTS")
                    key: "voice_prompts"
                    title: qsTr("Voice prompts")
                    subtitle: qsTr("Spoken messages when powering on or switching modes")
                }
                SettingSwitch {
                    visible: root.has("CAP_BT_WHEN_POWERED_ON")
                    key: "bt_powered_on"
                    title: qsTr("Bluetooth on power-up")
                    subtitle: qsTr("Turns Bluetooth on together with the headset")
                }
                SettingSlider {
                    visible: root.has("CAP_BT_CALL_VOLUME")
                    key: "bt_call_volume"
                    title: qsTr("Bluetooth call volume")
                    to: 100
                }
                CutButton {
                    visible: root.has("CAP_NOTIFICATION_SOUND")
                    text: qsTr("Play notification sound")
                    enabled: !HeadsetController.busy
                    onClicked: HeadsetController.playNotification()
                }

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("The headset cannot report these settings: the last value applied from here is shown.")
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                visible: HeadsetController.connected && root.hasEq
                title: qsTr("Equalizer")
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns

                Flow {
                    visible: root.has("CAP_EQUALIZER_PRESET") && HeadsetController.presetNames.length > 0
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: HeadsetController.presetNames
                        CutButton {
                            id: presetButton
                            required property string modelData
                            required property int index
                            text: root.presetLabel(presetButton.modelData)
                            active: !root.eqDirty && HeadsetController.eqPreset === presetButton.index
                            enabled: !HeadsetController.busy
                            onClicked: {
                                root.eqDirty = false
                                HeadsetController.setEqPreset(presetButton.index)
                            }
                        }
                    }
                }

                Rectangle {
                    visible: root.has("CAP_EQUALIZER") && HeadsetController.eqBands > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    implicitHeight: bandRow.implicitHeight + 24
                    color: "#0d0d10"
                    border.color: Theme.line

                    // 0 dB line at the height of the sliders' centre
                    Rectangle {
                        x: 1
                        width: parent.width - 2
                        height: 1
                        y: bandRow.y + (bands.count > 0 ? ((bands.itemAt(0) as EqBand)?.sliderCenter ?? 0) : 0)
                        color: "#40ff5200"
                    }

                    RowLayout {
                        id: bandRow
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        Repeater {
                            id: bands
                            model: HeadsetController.eqBands
                            EqBand {
                                id: band
                                required property int index
                                Layout.fillWidth: true
                                label: root.bandLabel(band.index)
                                from: HeadsetController.eqMin
                                to: HeadsetController.eqMax
                                step: HeadsetController.eqStep
                                onMoved: root.eqDirty = true
                            }
                        }
                    }
                }

                RowLayout {
                    visible: root.has("CAP_EQUALIZER")
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("%1 bands from %2 to +%3 dB in steps of %4.").arg(HeadsetController.eqBands)
                            .arg(root.number(HeadsetController.eqMin)).arg(root.number(HeadsetController.eqMax))
                            .arg(root.number(HeadsetController.eqStep))
                            + (HeadsetController.eqBands === 10 ? " " + qsTr("Frequencies are approximate.") : "")
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                    CutButton {
                        text: qsTr("Reset to 0")
                        onClicked: {
                            root.setBands(new Array(bands.count).fill(0))
                            root.eqDirty = true
                        }
                    }
                    CutButton {
                        text: qsTr("Apply equalizer")
                        primary: true
                        enabled: !HeadsetController.busy
                        onClicked: {
                            root.eqDirty = false
                            HeadsetController.setEqCustom(root.bandValues())
                        }
                    }
                }
            }

            Card {
                visible: HeadsetController.connected && root.hasMic
                title: qsTr("Microphone")
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width >= 900 ? 2 : 1
                    columnSpacing: 32
                    rowSpacing: 18

                    SettingSlider {
                        visible: root.has("CAP_SIDETONE")
                        key: "sidetone"
                        title: qsTr("Sidetone")
                        subtitle: qsTr("How much you hear yourself through the headset")
                        to: 128
                    }
                    SettingSlider {
                        visible: root.has("CAP_MICROPHONE_VOLUME")
                        key: "mic_volume"
                        title: qsTr("Microphone volume")
                        subtitle: qsTr("Gain of the headset's own microphone")
                        to: 128
                    }
                    SettingChoice {
                        visible: root.has("CAP_MICROPHONE_MUTE_LED_BRIGHTNESS")
                        key: "mic_led"
                        title: qsTr("Mute LED brightness")
                        options: [qsTr("Off"), qsTr("Low"), qsTr("Medium"), qsTr("High")]
                    }
                    SettingChoice {
                        visible: root.has("CAP_NOISE_FILTER")
                        key: "noise_filter"
                        title: qsTr("Noise filter")
                        options: [qsTr("Off"), qsTr("Low"), qsTr("High")]
                    }
                    SettingSwitch {
                        visible: root.has("CAP_ROTATE_TO_MUTE")
                        key: "rotate_to_mute"
                        title: qsTr("Rotate to mute")
                        subtitle: qsTr("Mutes when the boom is raised")
                    }
                }
            }

            Item { Layout.columnSpan: grid.columns; implicitHeight: 6 }
        }
    }

    // Temporary notification
    Rectangle {
        id: toast

        property bool error: false

        function show(message: string, isError: bool) {
            toastText.text = message
            error = isError
            opacity = 1
            hideTimer.restart()
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: opacity > 0 ? 20 : 10
        width: toastText.implicitWidth + 40
        height: 42
        color: "#18181c"
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 250 } }
        Behavior on anchors.bottomMargin { NumberAnimation { duration: 250 } }

        Rectangle { width: 3; height: parent.height; color: toast.error ? Theme.bad : Theme.accent }
        Text {
            textFormat: Text.PlainText
            id: toastText
            anchors.centerIn: parent
            font.capitalization: Font.AllUppercase
            color: toast.error ? Theme.bad : Theme.text
            font.family: Theme.display
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
        Timer { id: hideTimer; interval: 2500; onTriggered: toast.opacity = 0 }
    }
}
