import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string labelText: "Text"
    property bool stretchVertical: false // для тех, кто скроллиться не должен
    property bool enableScrolling: false // скролл на уровне DockWidget — выключен по умолчанию

    // Ограничения в док-зоне (SplitView). Когда панель во всплывающем окне, эти пропы ни на что не давят.
    property real dockMinimumWidth: 140
    property real dockPreferredWidth: 220
    property real dockMaximumWidth: 100000
    property bool dockFillWidth: false

    property var dockHost: null
    property bool floated: false
    /** @internal */
    property var _floatWindow: null

    readonly property bool __isDockWidget: true

    Layout.fillWidth: true
    Layout.fillHeight: true

    implicitWidth: dockPreferredWidth
    implicitHeight: 150

    default property alias content: container.children

    SplitView.minimumWidth: dockMinimumWidth
    SplitView.preferredWidth: dockPreferredWidth
    SplitView.maximumWidth: dockMaximumWidth
    SplitView.fillWidth: dockFillWidth

    Binding on width {
        when: floated && parent !== null
        value: parent.width
        restoreMode: Binding.RestoreBindingOrValue
    }
    Binding on height {
        when: floated && parent !== null
        value: parent.height
        restoreMode: Binding.RestoreBindingOrValue
    }

    Rectangle {
        id: contentRect
        anchors.fill: parent
        color: "transparent"
        border.color: "#513405"
        radius: 5

        Flickable {
            id: flickable
            anchors.top: parent.top
            anchors.topMargin: 25 // 22 (header) + 3 (margin)
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.right: parent.right
            anchors.rightMargin: 3
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3

            clip: true

            // contentWidth: width — нет binding loop (self-reference валиден)
            contentWidth: width
            // stretchVertical или скролл выкл → заполняем flickable; скролл вкл → натуральная высота
            contentHeight: (enableScrolling
                            && !stretchVertical) ? container.implicitHeight : height

            // Флик (свайп/колёсико) только когда скролл разрешён
            interactive: enableScrolling && !stretchVertical

            ColumnLayout {
                // контент
                id: container
                width: flickable.width
                // заполняем flickable когда скролл выключен, иначе натуральная высота
                height: (enableScrolling
                         && !stretchVertical) ? implicitHeight : flickable.height
                spacing: 0
            }

            ScrollBar.vertical: ScrollBar {
                policy: (enableScrolling
                         && !stretchVertical) ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                contentItem: Rectangle {
                    implicitWidth: 6
                    color: parent.pressed ? "#000" : "#ccc"
                    radius: width / 2
                }
            }
        }
    }

    // Заголовок
    Rectangle {
        id: headerRect
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 22
        color: "#513405"
        radius: 3

        MouseArea {
            id: headerReorderArea
            anchors.fill: parent
            anchors.rightMargin: 50
            hoverEnabled: false
            enabled: dockHost !== null && !floated
            z: 0
            property real pressX

            onPressed: function (mouse) {
                pressX = mouse.x
            }
            onReleased: function (mouse) {
                if (!dockHost)
                    return
                let xInDock = mapToItem(root, mouse.x, mouse.y).x
                dockHost.moveDockByHeaderDrop(root, xInDock, mouse.x - pressX)
            }
        }

        Text {
            id: label
            text: labelText
            color: "white"
            font.pixelSize: 12

            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            z: 0
        }

        Item {
            anchors.right: parent.right
            anchors.rightMargin: 21
            anchors.verticalCenter: parent.verticalCenter
            width: 15
            height: 15
            z: 1

            Image {
                source: "qrc:/icons/Floating_Icon.svg"
                width: 11
                height: 11
                anchors.centerIn: parent
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (dockHost)
                        dockHost.floatDock(root)
                    else
                        console.log("Float: нет dockHost")
                }
            }
        }

        // Кнопка Close
        Item {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 12
            height: 12
            z: 1

            Image {
                source: "qrc:/icons/Close_Icon.svg"
                width: 12
                height: 12
                anchors.centerIn: parent
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (dockHost)
                        dockHost.closeDock(root)
                    else
                        console.log("Close: нет dockHost")
                }
            }
        }
    }
}
