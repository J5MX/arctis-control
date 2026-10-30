pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

ApplicationWindow {
    id: root

    readonly property list<string> frequencies: ["32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
    property bool eqDirty: false

    width: 1000
    height: 830
    minimumWidth: 720
    minimumHeight: 560
    visible: true
    title: "Arctis Nova 5"
    color: Theme.bg
    font.family: "Noto Sans"
    font.pixelSize: 14

    function presetLabel(name: string): string {
        switch (name) {
        case "flat": return "Plano"
        case "bass": return "Graves"
        case "focus": return "Enfoque"
        case "smiley": return "Smiley"
        default: return name
        }
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

    // El auricular no deja leer estos ajustes: se muestran los últimos aplicados.
    function syncControls() {
        limiter.checked = HeadsetController.volumeLimiter === 1
        inactive.currentIndex = Math.max(0, inactive.indexOfValue(HeadsetController.inactiveTime))
        if (!eqDirty)
            setBands(HeadsetController.activeBands)
    }

    Connections {
        target: HeadsetController
        function onStateChanged() { root.syncControls() }
        function onPresetsChanged() { root.syncControls() }
        function onNotify(message: string, error: bool) {
            toast.show(message, error)
            if (error)
                root.syncControls()
        }
    }

    Component.onCompleted: syncControls()

    // No se consulta el auricular con la ventana minimizada.
    Binding {
        target: HeadsetController
        property: "polling"
        value: root.visible && root.visibility !== Window.Minimized
    }

    // Resplandor naranja en la esquina superior derecha
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
                source: "qrc:/qt/qml/Nova5Panel/assets/icon.svg"
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
                    text: "ARCTIS NOVA 5"
                    color: Theme.text
                    font.family: Theme.display
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    font.letterSpacing: 0.9
                }
            }
            Item { Layout.fillWidth: true }

            // Estado de conexión
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
                    text: HeadsetController.connected ? "CONECTADO" : "DESCONECTADO"
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

            // Ilustración del auricular
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                Item { Layout.fillHeight: true }
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(230, parent.width)
                    Layout.preferredHeight: Layout.preferredWidth * 160 / 240
                    source: "qrc:/qt/qml/Nova5Panel/assets/headset.svg"
                    sourceSize: Qt.size(460, 307)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.alignment: Qt.AlignHCenter
                    text: "Inalámbrico 2,4 GHz · dongle USB-C"
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                title: "Batería"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20
                    BatteryRing {
                        level: HeadsetController.connected ? HeadsetController.batteryLevel : -1
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: !HeadsetController.connected ? HeadsetController.errorText || "Sin datos"
                                : HeadsetController.batteryStatus === "BATTERY_CHARGING" ? "⚡ CARGANDO"
                                : HeadsetController.batteryLevel >= 0 ? "EN USO"
                                : "Auricular apagado o fuera de alcance"
                            color: Theme.text
                            font.family: Theme.display
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.9
                        }
                        Text {
                            textFormat: Text.PlainText
                            visible: HeadsetController.updatedText !== ""
                            text: "Actualizado " + HeadsetController.updatedText
                            color: Theme.muted
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                title: "Ajustes"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 2
                        Text { textFormat: Text.PlainText; text: "Limitador de volumen"; color: Theme.text; font.weight: Font.Medium }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Protege los oídos de picos fuertes"
                            color: Theme.muted
                            font.pixelSize: 13
                        }
                    }
                    Toggle {
                        id: limiter
                        enabled: !HeadsetController.busy && HeadsetController.connected
                        onToggled: HeadsetController.setVolumeLimiter(checked)
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 2
                        Text { textFormat: Text.PlainText; text: "Apagado automático"; color: Theme.text; font.weight: Font.Medium }
                        Text { textFormat: Text.PlainText; text: "Tras este tiempo sin uso"; color: Theme.muted; font.pixelSize: 13 }
                    }
                    ComboBox {
                        id: inactive
                        enabled: !HeadsetController.busy && HeadsetController.connected
                        textRole: "label"
                        valueRole: "value"
                        model: ListModel {
                            ListElement { label: "Nunca"; value: 0 }
                            ListElement { label: "1 min"; value: 1 }
                            ListElement { label: "5 min"; value: 5 }
                            ListElement { label: "10 min"; value: 10 }
                            ListElement { label: "15 min"; value: 15 }
                            ListElement { label: "30 min"; value: 30 }
                            ListElement { label: "60 min"; value: 60 }
                            ListElement { label: "90 min"; value: 90 }
                        }
                        onActivated: HeadsetController.setInactiveTime(currentValue)
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
                            text: inactive.displayText
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
                                text: entry.label
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

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: HeadsetController.volumeLimiter < 0
                    wrapMode: Text.WordWrap
                    text: "El auricular no permite leer el limitador; se mostrará el valor cuando lo cambies desde aquí."
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                title: "Ecualizador"
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns

                Flow {
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
                            enabled: !HeadsetController.busy && HeadsetController.connected
                            onClicked: {
                                root.eqDirty = false
                                HeadsetController.setEqPreset(presetButton.index)
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    implicitHeight: bandRow.implicitHeight + 24
                    color: "#0d0d10"
                    border.color: Theme.line

                    // Línea de 0 dB a la altura del centro de los deslizadores
                    Rectangle {
                        x: 1
                        width: parent.width - 2
                        height: 1
                        y: bandRow.y + ((bands.itemAt(0) as EqBand)?.sliderCenter ?? 0)
                        color: "#40ff5200"
                    }

                    RowLayout {
                        id: bandRow
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        Repeater {
                            id: bands
                            model: root.frequencies
                            EqBand {
                                id: band
                                required property string modelData
                                Layout.fillWidth: true
                                label: band.modelData
                                onMoved: root.eqDirty = true
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    CutButton {
                        text: "Poner a 0"
                        onClicked: {
                            root.setBands([0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                            root.eqDirty = true
                        }
                    }
                    CutButton {
                        text: "Aplicar ecualizador"
                        primary: true
                        enabled: !HeadsetController.busy && HeadsetController.connected
                        onClicked: {
                            root.eqDirty = false
                            HeadsetController.setEqCustom(root.bandValues())
                        }
                    }
                }

                Text {
                    textFormat: Text.PlainText
                    text: "10 bandas de −10 a +10 dB en pasos de 0,5. Las frecuencias son orientativas."
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }

            Item { Layout.columnSpan: grid.columns; implicitHeight: 6 }
        }
    }

    // Aviso temporal
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
