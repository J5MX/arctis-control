import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

ApplicationWindow {
    id: root

    readonly property var frequencies: ["32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
    readonly property var presetNames: ({ flat: "Plano", bass: "Graves", focus: "Enfoque", smiley: "Smiley" })
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

    function setBands(values) {
        for (let i = 0; i < bands.count && i < values.length; ++i)
            bands.itemAt(i).value = values[i]
    }

    function bandValues() {
        const values = []
        for (let i = 0; i < bands.count; ++i)
            values.push(bands.itemAt(i).value)
        return values
    }

    // El auricular no deja leer estos ajustes: se muestran los últimos aplicados.
    function syncControls() {
        limiter.checked = HeadsetController.volumeLimiter === 1
        inactive.currentIndex = Math.max(0, inactive.indexOfValue(HeadsetController.inactiveTime))
        if (eqDirty)
            return
        const preset = HeadsetController.eqPreset
        const presets = HeadsetController.presets
        setBands(preset >= 0 && preset < presets.length ? presets[preset].bands : HeadsetController.eqCustom)
    }

    Connections {
        target: HeadsetController
        function onStateChanged() { root.syncControls() }
        function onPresetsChanged() { root.syncControls() }
        function onNotify(message, error) {
            toast.show(message, error)
            if (error)
                root.syncControls()
        }
    }

    Component.onCompleted: syncControls()

    // Resplandor naranja en la esquina superior derecha
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeWidth: 0
            fillGradient: RadialGradient {
                centerX: root.width; centerY: -40
                centerRadius: 520
                focalX: centerX; focalY: centerY
                GradientStop { position: 0; color: Qt.rgba(1, 0.32, 0, 0.10) }
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
                    text: "STEELSERIES"
                    color: Theme.accent
                    font.family: Theme.display
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 3.5
                }
                Text {
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
                    preferredRendererType: Shape.CurveRenderer
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
                        Text { text: "Limitador de volumen"; color: Theme.text; font.weight: Font.Medium }
                        Text {
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
                        Text { text: "Apagado automático"; color: Theme.text; font.weight: Font.Medium }
                        Text { text: "Tras este tiempo sin uso"; color: Theme.muted; font.pixelSize: 13 }
                    }
                    ComboBox {
                        id: inactive
                        enabled: !HeadsetController.busy && HeadsetController.connected
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: "Nunca", value: 0 },
                            { text: "1 min", value: 1 },
                            { text: "5 min", value: 5 },
                            { text: "10 min", value: 10 },
                            { text: "15 min", value: 15 },
                            { text: "30 min", value: 30 },
                            { text: "60 min", value: 60 },
                            { text: "90 min", value: 90 }
                        ]
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
                            leftPadding: 12
                            text: inactive.displayText
                            color: Theme.text
                            font.family: Theme.display
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            verticalAlignment: Text.AlignVCenter
                        }
                        indicator: Text {
                            x: inactive.width - width - 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▾"
                            color: Theme.accent
                        }
                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            width: inactive.width
                            highlighted: inactive.highlightedIndex === index
                            contentItem: Text {
                                text: parent.modelData.text
                                color: parent.highlighted ? Theme.accent : Theme.text
                                font.family: Theme.display
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            background: Rectangle { color: parent.highlighted ? Theme.accentDim : Theme.cardHi }
                        }
                        popup.background: Rectangle { color: Theme.cardHi; border.color: Theme.line }
                    }
                }

                Text {
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
                Layout.columnSpan: parent.columns

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: HeadsetController.presets
                        CutButton {
                            required property var modelData
                            required property int index
                            text: root.presetNames[modelData.name] || modelData.name
                            active: !root.eqDirty && HeadsetController.eqPreset === index
                            enabled: !HeadsetController.busy && HeadsetController.connected
                            onClicked: {
                                root.eqDirty = false
                                HeadsetController.setEqPreset(index)
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
                        y: bandRow.y + (bands.count > 0 && bands.itemAt(0) ? bands.itemAt(0).sliderCenter : 0)
                        color: Qt.rgba(1, 0.32, 0, 0.25)
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
                                required property string modelData
                                Layout.fillWidth: true
                                label: modelData
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
                    text: "10 bandas de −10 a +10 dB en pasos de 0,5. Las frecuencias son orientativas."
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }

            Item { Layout.columnSpan: parent.columns; implicitHeight: 6 }
        }
    }

    // Aviso temporal
    Rectangle {
        id: toast

        property bool error: false

        function show(message, isError) {
            toastText.text = message.toUpperCase()
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
            id: toastText
            anchors.centerIn: parent
            color: toast.error ? Theme.bad : Theme.text
            font.family: Theme.display
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
        Timer { id: hideTimer; interval: 2500; onTriggered: toast.opacity = 0 }
    }
}
