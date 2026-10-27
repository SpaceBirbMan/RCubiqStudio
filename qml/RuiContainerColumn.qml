import QtQuick
import QtQuick.Layouts

// Внешний Item — стабильный хост для ColumnLayout; без растягивания col на весь док (контент по натуральной высоте).
Item {
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        id: col
        objectName: "rui_containerColumn"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        width: parent.width
        spacing: 8
    }
}
