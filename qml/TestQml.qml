import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtQuick.Shapes

ApplicationWindow {
    width: 640
    height: 480
    title: qsTr("Test")
    color: "#191919"

    property bool debug: false

    FontLoader {
        id: ralewayLight
        source: "qrc:/fonts/Raleway-v4020-Light.ttf"
    }

    font.family: ralewayLight.name

    header: MenuBar {
        Menu {
            title: qsTr("&Файл")
            Action {
                text: qsTr("&Open...")
                onTriggered: console.log("Open action triggered")
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Exit")
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&Вид")
        }
        Menu {
            title: qsTr("&Настройки")
        }
        Menu {
            title: qsTr("&О программе")
        }
    }
    DockContainer {
        id: dockArea
        anchors.fill: parent

        DockWidget {
            id: enginesDock
            labelText: "Engine"
            dockPreferredWidth: 216

            TabWidget {
                Layout.fillWidth: true
                Layout.fillHeight: true
                tabPosition: 2

                Tab { title: "Settings"
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        Layout.topMargin: 5
                        spacing: 0

                        MyLabel {
                            caption: "Avatar"
                            Layout.leftMargin: 1
                            Layout.bottomMargin: 5
                        }

                        RowLayout {
                            id: rowAvatarButtons
                            spacing: 15
                            Layout.alignment: Qt.AlignLeft

                            LightButton {
                                label: "Open file"
                            }
                            LightButton {
                                label: "Reload model"
                            }
                        }
                    }

                    HorizontalSplitter {}

                    // --- Секция Background ---
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true // Занимает свободное место
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        spacing: 7

                        MyLabel {
                            caption: "Background"
                        }

                        LightButton {
                            label: "Choose..."
                            Layout.alignment: Qt.AlignLeft
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150 // Используем preferredHeight вместо height в Layout
                            radius: 5
                            color: "#1E1E1E"

                            Image {
                                anchors.centerIn: parent
                                source: "qrc:/icons/Logo.svg"
                                width: 70
                                height: 70
                            }
                        }
                    }

                    HorizontalSplitter {}

                    // --- Секция Filter ---
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        spacing: 7

                        MyLabel {
                            caption: "Filter"
                        }

                        ComboBox {
                            id: combo
                            model: ["First", "Second", "Third"]
                            Layout.fillWidth: true
                            Layout.preferredHeight: 20

                            indicator: Canvas {
                                id: canvas
                                x: combo.width - width - combo.rightPadding
                                y: combo.topPadding + (combo.availableHeight - height) / 2
                                width: 12
                                height: 8
                                contextType: "2d"

                                Connections {
                                    target: combo
                                    function onPressedChanged() {
                                        canvas.requestPaint()
                                    }
                                }

                                onPaint: {
                                    context.reset()
                                    context.moveTo(0, 0)
                                    context.lineTo(width, 0)
                                    context.lineTo(width / 2, height)
                                    context.closePath()
                                    context.fillStyle = combo.pressed ? "#D48912" : "#D48912"
                                    context.fill()
                                }
                            }

                            DropShadow {
                                anchors.fill: back
                                horizontalOffset: 0
                                verticalOffset: 0
                                radius: 8.0
                                samples: 17
                                color: "#D48912"
                                source: back
                                transparentBorder: true
                            }

                            DropShadow {
                                anchors.fill: canvas
                                horizontalOffset: 0
                                verticalOffset: 0
                                radius: 8.0
                                samples: 17
                                color: "#D48912"
                                source: canvas
                                transparentBorder: true
                            }

                            contentItem: Text {
                                leftPadding: 0
                                rightPadding: combo.indicator.width + combo.spacing
                                text: combo.displayText
                                font: combo.font
                                color: combo.pressed ? "#FFFFFF" : "#FFFFFF"
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            background: Rectangle {
                                id: back
                                color: "#191919"
                                border.color: combo.pressed ? "#D48912" : "#D48912"
                                border.width: combo.visualFocus ? 2 : 1
                                radius: 2
                            }

                            popup: Popup {
                                y: combo.height - 1
                                width: combo.width
                                height: Math.min(
                                            contentItem.implicitHeight,
                                            combo.Window.height - topMargin - bottomMargin)
                                padding: 1
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: combo.popup.visible ? combo.delegateModel : null
                                    currentIndex: combo.highlightedIndex
                                    ScrollIndicator.vertical: ScrollIndicator {}
                                }
                                background: Rectangle {
                                    color: "#191919"
                                    radius: 2
                                }
                            }
                        }

                        LabeledSlider {
                            Layout.topMargin: 5
                            label: "Opacity"
                        }
                        LabeledSlider {
                            Layout.topMargin: 5
                            label: "Opacity"
                        }
                        LabeledSlider {
                            Layout.topMargin: 5
                            label: "Opacity"
                        }
                        LabeledSlider {
                            Layout.topMargin: 5
                            label: "Opacity"
                        }
                    }
                }
                }
                Tab { title: "Advanced"
                Rectangle {
                    color: "red"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                }
            }
        }
        DockWidget {
            labelText: "Viewport"
            dockFillWidth: true
            dockPreferredWidth: 320
            dockMinimumWidth: 160

            stretchVertical: true
            enableScrolling: false

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true

                color: debug ? "blue" : "transparent"
                Image {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    source: "qrc:/icons/Group 2.svg"
                    width: 61
                    height: 335
                }
            }
            Rectangle {
                // область для рендера
                anchors.fill: parent

                color: "red"
                opacity: debug ? 0.4 : 0
            }
        }
        DockWidget {
            labelText: "Properties"
            dockPreferredWidth: 220
            TabWidget {
                Layout.fillWidth: true
                Layout.fillHeight: true
                tabPosition: 3

                Tab { title: "Settings"
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 7
                    Toolbox {
                        anchors.fill: parent
                        anchors.margins: 5
                        anchors.rightMargin: -1
                        ToolboxItem {
                            LightCheckbox {}
                            LightInputbox {}
                        }
                        ToolboxItem {}
                        ToolboxItem {}
                        ToolboxItem {}
                    }
                }
                }

                Tab { title: "S"
                Item {}
                }
            }
        }
    }
}
