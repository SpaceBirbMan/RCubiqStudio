import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: root
    clip: true
    property string labelText: "Text"
    property bool extended: false

    Layout.fillWidth: true
    implicitWidth: 200
    implicitHeight: extended ? (headerRect.height + contentColumn.implicitHeight
                                + 6) : headerRect.height

    default property alias content: contentColumn.children

    Rectangle {


        id: contentRect
        anchors.fill: parent
        color: "transparent"
        border.color: "#513405"
        radius: 5

        ColumnLayout {
            clip: true
            id: contentColumn
            objectName: "toolbox_itemContent"
            anchors.top: parent.top
            anchors.topMargin: 22
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 3
            spacing: 0
        }
    }

    Rectangle {
        id: headerRect
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 22
        color: "#191919"
        border.color: "#D48912"
        radius: 3
        z: 1

        Text {
            text: labelText
            color: "white"
            font.pixelSize: 12
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            id: lamp
            anchors.top: parent.top
            anchors.topMargin: 3
            anchors.right: parent.right
            anchors.rightMargin: 5
            color: root.extended ? "#D48912" : "#513405"
            height: 16
            width: 3
            radius: 3
        }

        DropShadow {
            visible: root.extended
            anchors.fill: lamp
            horizontalOffset: 0
            verticalOffset: 0
            radius: 8.0
            samples: 17
            color: "#D48912"
            source: lamp
            transparentBorder: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.extended = !root.extended
                var p = parent
                while (p) {
                    if (typeof p.closeOthers === 'function') {
                        p.closeOthers(root)
                        break
                    }
                    p = p.parent
                }
            }
        }
    }

    DropShadow {
        anchors.fill: headerRect
        horizontalOffset: 0
        verticalOffset: 0
        radius: 8.0
        samples: 17
        color: "#D48912"
        source: headerRect
        transparentBorder: true
        z: 0
    }

    Behavior on implicitHeight {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }

    /// Якорь для Quick-рендера (sink контента страницы аккордеона).
    readonly property alias toolboxContentHost: contentColumn
}
