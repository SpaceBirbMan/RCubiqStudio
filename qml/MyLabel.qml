import QtQuick
import QtQuick.Layouts

Item {

    implicitHeight: lab.height
    implicitWidth: lab.width
    property string caption: "Text"
    property int fontPixelSize: 12
    property bool fontBold: false
    property bool fontItalic: false

    FontLoader {
        id: ralewayLight
        source: "qrc:/fonts/Raleway-v4020-Light.ttf"
    }

    Text {
        id: lab
        font.family: ralewayLight.name
        font.bold: fontBold
        font.italic: fontItalic
        text: caption
        color: "white"
        Layout.alignment: Qt.AlignLeft
        Layout.leftMargin: 2
        font.pixelSize: fontPixelSize
    }
}
