import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: appRoot
    width: 1074
    height: 614
    visible: true
    title: qsTr("RCubiQ Studio")
    color: "#191919"

    property bool debug: false

    FontLoader {
        id: ralewayLight
        source: "qrc:/fonts/Raleway-v4020-Light.ttf"
    }

    font.family: ralewayLight.name

    onClosing: function (closeEvent) {
        Qt.quit()
    }

    // Component.onCompleted: RenderHost.attachTo(renderZone)
    // Attach is now done in MainWindow::attachEmbeddedPanels() via QTimer::singleShot(0)
    // to avoid a second WidgetSurfaceEmbed::attach call on the same ViewportWidget,
    // which recreates the native HWND and hangs the main thread on the first bgfx frame.

    header: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&New setup…")
                onTriggered: AppBridge.newSetup()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Save setup as…")
                onTriggered: AppBridge.saveSetup()
            }
            MenuSeparator {}
            Action {
                text: qsTr("E&xit")
                onTriggered: AppBridge.quit()
            }
        }
        Menu {
            title: qsTr("&View")
        }
        Menu {
            title: qsTr("&Settings")
            Action {
                text: qsTr("&Preferences…")
                onTriggered: {
                    if (typeof AppBridge !== "undefined" && AppBridge.openPreferences)
                        AppBridge.openPreferences()
                    else
                        console.warn("Settings: AppBridge.openPreferences is not defined (widget build uses the menu bar)")
                }
            }
            MenuSeparator {}
            Action {
                text: qsTr("Add render &API…")
                onTriggered: AppBridge.chooseRenderApi()
            }
        }
        Menu {
            title: qsTr("&About")
        }
    }
    DockContainer {
        id: dockArea
        anchors.fill: parent

        DockWidget {
            id: enginesDock
            labelText: qsTr("Engine")
            dockPreferredWidth: 216

            TabWidget {
                objectName: "studio_engineTabWidget"
                Layout.fillWidth: true
                Layout.fillHeight: true
                tabPosition: TabWidget.Position.Left
            }
        }
        DockWidget {
            labelText: qsTr("Viewport")
            dockFillWidth: true
            dockPreferredWidth: 320
            dockMinimumWidth: 160

            stretchVertical: true
            enableScrolling: false

            Item {
                id: viewportInner
                Layout.fillWidth: true
                Layout.fillHeight: true

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    opacity: 0.35
                    source: "qrc:/icons/Group 2.svg"
                    z: 0
                }

                Item {
                    id: renderZone
                    objectName: "renderZone"
                    anchors.centerIn: parent
                    z: 1

                    function syncSize() {
                        var w = viewportInner.width - 72
                        var h = viewportInner.height - 72
                        width = Math.max(0, w)
                        height = Math.max(0, h)
                    }
                    Component.onCompleted: syncSize()

                    Rectangle {
                        anchors.fill: parent
                        visible: debug
                        color: "red"
                        opacity: 0.35
                    }
                }

                Connections {
                    target: viewportInner
                    ignoreUnknownSignals: true
                    function onWidthChanged() { renderZone.syncSize() }
                    function onHeightChanged() { renderZone.syncSize() }
                }
            }
        }
        DockWidget {
            labelText: qsTr("Plugins & devices")
            dockPreferredWidth: 280

            TabWidget {
                objectName: "studio_propertiesTabWidget"
                Layout.fillWidth: true
                Layout.fillHeight: true
                tabPosition: TabWidget.Position.Right

                Tab {
                    title: qsTr("Tracking")
                    StudioTrackingPanel {}
                }
                Tab {
                    title: qsTr("Plugins")
                    StudioPluginsPanel {}
                }
                Tab {
                    title: qsTr("Preferences")
                    StudioPreferencesPanel {}
                }
            }
        }
    }
}
