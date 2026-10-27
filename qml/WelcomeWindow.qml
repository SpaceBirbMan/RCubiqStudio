import QtQuick
import QtQuick.Layouts

Window {
    width: 640
    height: 480
    title: qsTr("Test WW")
    color: "#191919"

    FontLoader {
        id: ralewayLight
        source: "qrc:/fonts/Raleway-v4020-Light.ttf"
    }

    Image {
        source: "qrc:/icons/Group 2.svg"
        width: 100
        height: 550
        rotation: 90
        x: 240
        y: -225
    }

    MyLabel {
        caption: "Добро пожаловать в RCubiq Studio"
        x: 25
        y: 100
        fontPixelSize: 20
    }

    LightButton {
        fontFamily: "Noto Sans Symbols"
        label: "\u2699"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 22
        anchors.topMargin: 10
    }

    LightButton {
        fontFamily: "Noto Sans Symbols"
        label: "Github"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 22
        anchors.bottomMargin: 10
    }

    ColumnLayout {
        x: 25
        y: 140
        width: 200
        Text {
            Layout.bottomMargin: -2
            text: qsTr("Откроется базовая компоновка")
            font.pixelSize: 10
            color: "#797979"
        }

        LightButton {
            fillWidth: true
            Layout.fillWidth: true
            label: "Начать работу"
        }
        LightButton {
            fillWidth: true
            Layout.fillWidth: true
            label: "Открыть окно компоновки"
        }
        LightButton {
            fillWidth: true
            Layout.fillWidth: true
            label: "Импортировать настройки"
        }
        LightButton {
            fillWidth: true
            Layout.fillWidth: true
            label: "Помощь"
        }
    }
}
