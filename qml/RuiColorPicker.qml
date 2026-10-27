// RUI UiColorPicker: превью + LightButton; открытие системного выбора цвета — из C++ (QColorDialog).
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    Layout.fillWidth: true

    /// Запрос открыть диалог (подключается в uirenderer_quick.cpp).
    signal pickRequested()

    property string buttonLabel: "Choose color..."
    property color previewColor: "#ff000000"

    spacing: 8
    implicitHeight: Math.max(swatch.implicitHeight, pickBtn.implicitHeight)

    Rectangle {
        id: swatch
        Layout.preferredWidth: 32
        Layout.preferredHeight: 22
        Layout.alignment: Qt.AlignVCenter
        radius: 3
        color: root.previewColor
        border.width: 1
        border.color: mousePreview.containsMouse ? "#FFEF0B" : "#D48912"

        MouseArea {
            id: mousePreview
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.pickRequested()
        }
    }

    LightButton {
        id: pickBtn
        Layout.fillWidth: true
        label: root.buttonLabel
        onClicked: root.pickRequested()
    }
}
