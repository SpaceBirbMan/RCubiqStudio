import QtQuick
import QtQuick.Controls

Item {
    id: root
    implicitHeight: tf.height
    implicitWidth: tf.width

    // text / textChanged приходят от alias (дублирующий signal textChanged ломает QML в Qt 6)
    property alias text: tf.text
    property alias placeholderText: tf.placeholderText

    TextField {
        id: tf
        placeholderText: qsTr("Enter description")

        placeholderTextColor: "white"

        background: Rectangle {
            implicitWidth: 160
            implicitHeight: 30
            color: "transparent"
            border.color: "#D48912"
            radius: 3
        }
    }
}
