import QtQuick
import QtQuick.Controls

SplitView {
    id: root

    property int reorderMinimumDrag: 28
    property color handleIdleColor: "transparent"
    property color handleHoverColor: "#7a5830"

    orientation: Qt.Horizontal
    clip: true

    handle: Rectangle {
        implicitWidth: root.orientation === Qt.Horizontal ? 3 : root.width
        implicitHeight: root.orientation === Qt.Vertical ? 36 : root.height
        color: hh.hovered ? root.handleHoverColor : root.handleIdleColor

        HoverHandler {
            id: hh
            target: parent
        }
    }

    Component.onCompleted: relinkHosts()

    function relinkHosts() {
        for (var i = 0; i < contentChildren.length; ++i) {
            let c = contentChildren[i]
            if (c.__isDockWidget === true)
                c.dockHost = root
        }
    }

    function indexOfDock(dw) {
        for (var i = 0; i < contentChildren.length; ++i) {
            if (contentChildren[i] === dw)
                return i
        }
        return -1
    }

    function closeDock(dw) {
        if (!dw)
            return
        if (dw.floated) {
            let win = dw._floatWindow
            if (win) {
                win.skipRestoreOnClose = true
                win.close()
            } else {
                dw.destroy()
            }
            return
        }
        let ix = indexOfDock(dw)
        if (ix < 0)
            return
        removeItem(dw)
        dw.destroy()
    }

    function floatDock(dw) {
        if (!dw || dw.floated)
            return
        let ix = indexOfDock(dw)
        if (ix < 0)
            return
        let w = Math.max(320, dw.width)
        let h = Math.max(240, dw.height)
        takeItem(ix)
        dw.dockHost = root
        let win = floatingWindowComponent.createObject(null, {
                                                           "dockWidget": dw,
                                                           "initialWidth": w,
                                                           "initialHeight": h
                                                       })
        dw._floatWindow = win
    }

    function restoreDock(dw) {
        if (!dw || !dw.floated)
            return
        if (indexOfDock(dw) >= 0)
            return
        dw.floated = false
        dw._floatWindow = null
        dw.parent = root
        addItem(dw)
        dw.dockHost = root
    }

    function moveDockByHeaderDrop(dw, xInDockSpace, dragDelta) {
        if (!dw || dw.floated || Math.abs(dragDelta) < reorderMinimumDrag)
            return
        let from = indexOfDock(dw)
        if (from < 0)
            return
        let xInRoot = dw.mapToItem(root, xInDockSpace, 0).x
        let to = contentChildren.length - 1
        for (var i = 0; i < contentChildren.length; ++i) {
            let c = contentChildren[i]
            let mid = c.x + c.width * 0.5
            if (xInRoot < mid) {
                to = i
                break
            }
        }
        if (from !== to)
            moveItem(from, to)
    }

    Component {
        id: floatingWindowComponent
        ApplicationWindow {
            id: floatWin
            property Item dockWidget: null
            property real initialWidth: 400
            property real initialHeight: 300
            property bool skipRestoreOnClose: false

            flags: Qt.Window
            width: initialWidth
            height: initialHeight
            title: dockWidget ? dockWidget.labelText : ""
            visible: false

            Component.onCompleted: {
                if (dockWidget) {
                    dockWidget.parent = floatWin.contentItem
                    dockWidget.floated = true
                }
                visible = true
            }

            onClosing: function (closeEvent) {
                if (!dockWidget) {
                    close.accept()
                    Qt.callLater(function () {
                        floatWin.destroy()
                    })
                    return
                }
                if (skipRestoreOnClose) {
                    dockWidget.destroy()
                    close.accept()
                    Qt.callLater(function () {
                        floatWin.destroy()
                    })
                    return
                }
                if (dockWidget.dockHost)
                    dockWidget.dockHost.restoreDock(dockWidget)
                close.accept()
                Qt.callLater(function () {
                    floatWin.destroy()
                })
            }
        }
    }
}
