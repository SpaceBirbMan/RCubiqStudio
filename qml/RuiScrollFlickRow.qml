import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: fl
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 32

    contentWidth: inner.implicitWidth
    contentHeight: fl.height
    clip: true

    RowLayout {
        id: inner
        objectName: "rui_scrollInner"
        spacing: 8
        height: fl.height
    }

    ScrollBar.horizontal: ScrollBar {
        policy: fl.contentWidth > fl.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        contentItem: Rectangle {
            implicitHeight: 6
            radius: height / 2
            color: parent.pressed ? "#ffaa33" : "#D48912"
        }
    }
}
