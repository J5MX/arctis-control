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
        case "flat": return "Plano"
        case "bass": return "Graves"
        case "focus": return "Enfoque"
        case "smiley": return "Smiley"
        default: return name.charAt(0).toUpperCase() + name.slice(1)
        }
    }

    function number(value: real): string {
        return String(value).replace(".", ",")
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

    // El auricular no deja leer la mayoría de ajustes: se muestran los últimos aplicados.
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
            Qt.callLater(root.syncControls) // tras crear las bandas del nuevo auricular
        }
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

            // Selector si hay más de un auricular conectado
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

            // Sin auricular: qué hacer
            Card {
                visible: !HeadsetController.connected
                title: "Ningún auricular detectado"
                Layout.fillWidth: true
                Layout.columnSpan: grid.columns

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: HeadsetController.errorText || "Buscando auriculares…"
                    color: Theme.text
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    lineHeight: 1.3
                    text: "• Conecta el receptor USB (o el cable) y enciende el auricular.\n"
                        + "• Comprueba que HeadsetControl lo ve: headsetcontrol -b\n"
                        + "• Si dice que no tiene permisos, reconecta el receptor tras instalar headsetcontrol (instala reglas udev).\n"
                        + "• Compatibles: Arctis 1, 7, 7+, 7X, 7P, 9, Pro (2019 y Wireless), GameBuds y Nova 3, 3P, 5, 5X, 7, 7P y Pro Wireless."
                }
            }

            // Ilustración y datos del auricular
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
                        // ChatMix solo si la mezcla no está centrada (si se está usando).
                        + (root.has("CAP_CHATMIX_STATUS") && HeadsetController.chatmix >= 0 && HeadsetController.chatmix !== 64
                           ? " · ChatMix " + (HeadsetController.chatmix < 64 ? "juego " : "chat ")
                             + Math.round(Math.abs(HeadsetController.chatmix - 64) / 64 * 100) + " %" : "")
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                visible: HeadsetController.connected
                title: "Batería"
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
                            text: !root.has("CAP_BATTERY_STATUS") ? "Con cable"
                                : HeadsetController.batteryStatus === "BATTERY_CHARGING" ? "⚡ Cargando"
                                : HeadsetController.batteryLevel >= 0 ? "En uso"
                                : "Apagado o fuera de alcance"
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
                            text: "≈ " + Math.floor(HeadsetController.batteryMinutes / 60) + " h "
                                + HeadsetController.batteryMinutes % 60 + " min restantes"
                            color: Theme.text
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
                visible: HeadsetController.connected && root.hasGeneral
                title: "Ajustes"
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
                        Text { textFormat: Text.PlainText; text: "Apagado automático"; color: Theme.text; font.weight: Font.Medium }
                        Text { textFormat: Text.PlainText; text: "Tras este tiempo sin uso"; color: Theme.muted; font.pixelSize: 13 }
                    }
                    ComboBox {
                        id: inactive
                        enabled: !HeadsetController.busy
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
                            text: HeadsetController.setting("inactive_time") < 0 && HeadsetController.connected ? "—" : inactive.displayText
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

                SettingSwitch {
                    visible: root.has("CAP_VOLUME_LIMITER")
                    key: "volume_limiter"
                    title: "Limitador de volumen"
                    subtitle: "Protege los oídos de picos fuertes"
                }
                SettingSwitch {
                    visible: root.has("CAP_LIGHTS")
                    key: "lights"
                    title: "Luces"
                    subtitle: "Iluminación del auricular"
                }
                SettingSwitch {
                    visible: root.has("CAP_VOICE_PROMPTS")
                    key: "voice_prompts"
                    title: "Avisos de voz"
                    subtitle: "Mensajes hablados al encender o cambiar de modo"
                }
                SettingSwitch {
                    visible: root.has("CAP_BT_WHEN_POWERED_ON")
                    key: "bt_powered_on"
                    title: "Bluetooth al encender"
                    subtitle: "Activa el Bluetooth junto con el auricular"
                }
                SettingSlider {
                    visible: root.has("CAP_BT_CALL_VOLUME")
                    key: "bt_call_volume"
                    title: "Volumen de llamadas Bluetooth"
                    to: 100
                }
                CutButton {
                    visible: root.has("CAP_NOTIFICATION_SOUND")
                    text: "Probar sonido de aviso"
                    enabled: !HeadsetController.busy
                    onClicked: HeadsetController.playNotification()
                }

                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "El auricular no deja leer estos ajustes: se muestra el último aplicado desde aquí."
                    color: Theme.muted
                    font.pixelSize: 12
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                visible: HeadsetController.connected && root.hasEq
                title: "Ecualizador"
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

                    // Línea de 0 dB a la altura del centro de los deslizadores
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
                        text: HeadsetController.eqBands + " bandas de " + root.number(HeadsetController.eqMin) + " a +"
                            + root.number(HeadsetController.eqMax) + " dB en pasos de " + root.number(HeadsetController.eqStep)
                            + (HeadsetController.eqBands === 10 ? ". Frecuencias orientativas." : ".")
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                    CutButton {
                        text: "Poner a 0"
                        onClicked: {
                            root.setBands(new Array(bands.count).fill(0))
                            root.eqDirty = true
                        }
                    }
                    CutButton {
                        text: "Aplicar ecualizador"
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
                title: "Micrófono"
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
                        title: "Sidetone"
                        subtitle: "Cuánto te oyes a ti mismo por los auriculares"
                        to: 128
                    }
                    SettingSlider {
                        visible: root.has("CAP_MICROPHONE_VOLUME")
                        key: "mic_volume"
                        title: "Volumen del micrófono"
                        subtitle: "Ganancia del micro del propio auricular"
                        to: 128
                    }
                    SettingChoice {
                        visible: root.has("CAP_MICROPHONE_MUTE_LED_BRIGHTNESS")
                        key: "mic_led"
                        title: "Brillo del LED de silencio"
                        options: ["Apagado", "Bajo", "Medio", "Alto"]
                    }
                    SettingChoice {
                        visible: root.has("CAP_NOISE_FILTER")
                        key: "noise_filter"
                        title: "Filtro de ruido"
                        options: ["No", "Bajo", "Alto"]
                    }
                    SettingSwitch {
                        visible: root.has("CAP_ROTATE_TO_MUTE")
                        key: "rotate_to_mute"
                        title: "Silenciar al subir el micro"
                        subtitle: "Se silencia al levantar la varilla"
                    }
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
