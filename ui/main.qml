import QtQuick 2.15
import QtQuick.Controls 2.15

// здесь надо прокинуть логику и сделать прототип на html+css
Rectangle {
    width: 400
    height: 300
    color: "#2b2b2b"

    Column {
        anchors.centerIn: parent
        spacing: 16

        Button {
            text: "Нажми меня"
            onClicked: {
                label.text = "Кнопка ныажата"
            }
        }

        Text {
            id: label
            text: "Ожидание..."
            color: "#cccccc"
            font.pixelSize: 16
        }

        Slider {
            id: slider
            width: 200
            from: 0
            to: 100
            onValueChanged: {
                sliderValue.text = value.toFixed(0)
            }
        }

        Text {
            id: sliderValue
            text: slider.value.toFixed(0)
            color: "#aaaaaa"
        }
    }
}
