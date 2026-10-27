import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

ColumnLayout {
    id: root

    signal sliderValueChanged(int v)

    property string label: "Label"
    property alias from: control.from
    property alias to: control.to
    property alias value: control.value

    RowLayout {
        Layout.fillWidth: true
        MyLabel {
            caption: label
        }
        Item {
            Layout.fillWidth: true
        }
        MyLabel {
            id: valueLabel
            caption: String(Math.round(control.value))
        }
    }

    Slider {
        id: control
        from: 0
        to: 100
        stepSize: 1
        value: 50
        Layout.fillWidth: true
        Layout.topMargin: -6
        background: Rectangle {
            x: control.leftPadding
            y: control.topPadding + control.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4

            height: implicitHeight
            radius: 2
            color: "#513405"
        }

        handle: Rectangle {
            id: handle
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + control.availableHeight / 2 - height / 2
            implicitWidth: 13
            implicitHeight: 13
            radius: 13
            color: control.pressed ? "#FFEF0B" : "#D48912"
            border.color: "#D48912"
        }

        DropShadow {
            anchors.fill: handle
            horizontalOffset: 0
            verticalOffset: 0
            radius: 8.0
            samples: 17
            color: control.pressed ? "#FFEF0B" : "#D48912"
            source: handle
            transparentBorder: true
        }

        onValueChanged: {
            valueLabel.caption = String(Math.round(value))
            root.sliderValueChanged(Math.round(value))
        }
    }
}
