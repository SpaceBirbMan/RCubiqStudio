// TabLightButton.qml
import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: root
    /// Маркер для TabWidget: не полагаться на hasOwnProperty("label") — у Item в Qt 6 он часто «ломается».
    property bool dockTabStripButton: true
    property string label: "Button"
    property bool active: false
    property int rotationAngle: 0
    /// Прямая ссылка на TabWidget — устанавливается из setupTabs.
    property var tabWidgetRef: null

    signal clicked

    FontLoader {
        id: ralewayLight
        source: "qrc:/fonts/Raleway-v4020-Light.ttf"
    }

    implicitWidth: (Math.abs(
                        root.rotationAngle) === 90) ? button.height : button.width
    implicitHeight: (Math.abs(
                         root.rotationAngle) === 90) ? button.width : button.height

    Item {
        id: visualContainer
        x: (root.width - width) / 2
        y: (root.height - height) / 2

        width: button.width
        height: button.height
        rotation: root.rotationAngle

        Rectangle {
            id: button
            width: label_obj.width + 22
            height: 21
            radius: 3
            color: "#191919"
            border.color: !mouseArea.containsMouse ? "#D48912" : "#FFEF0B"

            Rectangle {
                id: lamp1
                color: active ? (!mouseArea.containsMouse ? "#D48912" : "#FFEF0B") : "#513405"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                x: 3
                anchors.margins: 3
                width: 3
                radius: 3
            }

            Rectangle {
                id: lamp2
                color: active ? (!mouseArea.containsMouse ? "#D48912" : "#FFEF0B") : "#513405"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                x: parent.width - 6
                anchors.margins: 3
                width: 3
                radius: 3
            }

            DropShadow {
                anchors.fill: lamp1
                horizontalOffset: 0
                verticalOffset: 0
                radius: 6.0
                samples: 17
                color: !mouseArea.containsMouse ? "#D48912" : "#FFEF0B"
                source: lamp1
                transparentBorder: true
                visible: active
                Behavior on radius {
                    NumberAnimation {
                        duration: 50
                    }
                }
            }

            DropShadow {
                anchors.fill: lamp2
                horizontalOffset: 0
                verticalOffset: 0
                radius: 6.0
                samples: 17
                color: !mouseArea.containsMouse ? "#D48912" : "#FFEF0B"
                source: lamp2
                transparentBorder: true
                visible: active
                Behavior on radius {
                    NumberAnimation {
                        duration: 50
                    }
                }
            }

            Text {
                id: label_obj
                font.family: ralewayLight.name
                anchors.centerIn: parent
                font.pixelSize: 12
                color: "white"
                text: root.label
            }
        }

        DropShadow {
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
    }

    MouseArea {
        id: mouseArea
        hoverEnabled: true
        preventStealing: true
        anchors.fill: parent
        onClicked: {
            if (root.tabWidgetRef && typeof root.tabWidgetRef.activateTab === 'function') {
                root.tabWidgetRef.activateTab(root)
            } else {
                // Фоллбэк: обход в случае отсутствия ссылки
                var p = root.parent
                while (p) {
                    if (typeof p.activateTab === 'function') {
                        p.activateTab(root)
                        break
                    }
                    p = p.parent
                }
            }
            root.clicked()
        }
    }
}
