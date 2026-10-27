import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts

Item {
    id: root

    signal clicked

    property string label: "Button"
    property bool fillWidth: false
    property string fontFamily: ralewayLight.name

    // Минимально гарантированная ширина: текст + поля. Так fillWidth не схлопывает кнопку до 0,
    // даже если parent.width ещё не известен (родитель — динамический Layout без anchors).
    readonly property real minButtonWidth: label_obj.implicitWidth + 12

    implicitWidth: minButtonWidth
    implicitHeight: 21

    Layout.fillWidth: fillWidth
    Layout.minimumWidth: minButtonWidth
    Layout.preferredHeight: 21

    FontLoader {
        id: ralewayLight
        source: "qrc:/fonts/Raleway-v4020-Light.ttf"
    }

    Rectangle {
        id: button
        anchors.fill: parent
        radius: 3
        color: "#191919"
        border.color: !mouseArea.containsMouse ? "#D48912" : "#FFEF0B"

        Text {
            id: label_obj
            font.family: fontFamily
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: root.fillWidth ? parent.left : undefined
            anchors.leftMargin: 5
            anchors.horizontalCenter: root.fillWidth ? undefined : parent.horizontalCenter
            font.pixelSize: 12
            color: "white"
            text: label
        }
    }

    DropShadow {
        id: backlight
        anchors.fill: button
        horizontalOffset: 0
        verticalOffset: 0
        radius: mouseArea.containsMouse ? 8.0 : 6.0
        samples: 17
        color: !mouseArea.containsMouse ? "#D48912" : "#FFEF0B"
        source: button
        transparentBorder: true

        Behavior on radius {
            NumberAnimation {
                duration: 50
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked: root.clicked()

        onPressed: {
            backlight.radius = 0.0
        }

        onReleased: {
            backlight.radius = mouseArea.containsMouse ? 8.0 : 6.0
        }
    }
}
