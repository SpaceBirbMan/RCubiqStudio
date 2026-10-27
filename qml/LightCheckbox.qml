import QtQuick
import QtQuick.Controls.Basic

Item {
    height: 20
    implicitHeight: cb.height
    implicitWidth: cb.width

    property alias text: cb.text
    property alias checked: cb.checked

    CheckBox {
        id: cb
        text: qsTr("CheckBox")
        checked: true

        indicator: Rectangle {
            implicitWidth: 15
            implicitHeight: 15
            x: cb.leftPadding
            y: parent.height / 2 - height / 2
            radius: 3
            color: "transparent"
            border.color: cb.down ? "#D48912" : "#D48912"

            Rectangle {
                width: 10
                height: 10
                x: 2.5
                y: 2.5
                radius: 2
                color: cb.down ? "#D48912" : "#D48912"
                visible: cb.checked
            }
        }

        contentItem: Text {
            text: cb.text
            font: cb.font
            opacity: enabled ? 1.0 : 0.3
            color: cb.down ? "#FFEF0B" : "white"
            verticalAlignment: Text.AlignVCenter
            leftPadding: cb.indicator.width + cb.spacing
        }
    }
}
