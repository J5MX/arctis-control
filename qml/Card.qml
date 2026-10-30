import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

// Tarjeta con esquinas cortadas en diagonal y título con barrita naranja.
Item {
    id: card

    property string title
    property int cut: 10
    default property alias content: column.data

    implicitWidth: column.implicitWidth + 40
    implicitHeight: column.implicitHeight + 40

    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: Theme.line
            strokeWidth: 1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: card.height
                GradientStop { position: 0; color: Theme.cardHi }
                GradientStop { position: 1; color: Theme.card }
            }
            startX: 0.5; startY: 0.5
            PathLine { x: card.width - card.cut; y: 0.5 }
            PathLine { x: card.width - 0.5; y: card.cut }
            PathLine { x: card.width - 0.5; y: card.height - 0.5 }
            PathLine { x: card.cut; y: card.height - 0.5 }
            PathLine { x: 0.5; y: card.height - card.cut }
            PathLine { x: 0.5; y: 0.5 }
        }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            visible: card.title !== ""
            spacing: 10

            Shape {
                implicitWidth: 8
                implicitHeight: 14
                ShapePath {
                    strokeWidth: 0
                    fillColor: Theme.accent
                    startX: 4; startY: 0
                    PathLine { x: 8; y: 0 }
                    PathLine { x: 4; y: 14 }
                    PathLine { x: 0; y: 14 }
                    PathLine { x: 4; y: 0 }
                }
            }
            Text {
                textFormat: Text.PlainText
                text: card.title
                font.capitalization: Font.AllUppercase
                color: Theme.text
                font.family: Theme.display
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.letterSpacing: 1.8
            }
        }
    }
}
