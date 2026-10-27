// TabWidget.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic

Item {
    id: root

    enum Position {
        Top,
        Bottom,
        Left,
        Right
    }

    property int tabPosition: TabWidget.Position.Top
    property int currentIndex: contentArea.currentIndex
    property bool enablePageScrolling: true

    /// Декларативный API: TabWidget { Tab { title: "..."; <Item> } ... }
    /// Неявно: всё, что положено внутрь TabWidget, попадает в этот список.
    /// Tab — QtObject, поэтому он не отображается напрямую; за создание кнопок и хостов отвечает буква.
    default property list<Tab> tabs

    implicitWidth: {
        var barW = leftBar.visible ? leftBar.width : (rightBar.visible ? rightBar.width : 0)
        return barW + contentArea.implicitWidth
    }
    implicitHeight: {
        var barH = topBar.visible ? topBar.height : (bottomBar.visible ? bottomBar.height : 0)
        return barH + contentArea.implicitHeight
    }

    // ─── Компоненты ──────────────────────────────────────────────────────────

    Component {
        id: dockTabBtnComp
        TabLightButton {
            objectName: "dock_tabStripBtn"
        }
    }

    Component {
        id: dockTabHostComp
        Rectangle {
            id: hostFrame
            objectName: "dock_tabHostFrame"
            color: "#191919"
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Внутренний Layout на всю высоту хоста: кнопки сверху, fillHeight у скролла — оставшееся место.
            property alias contentParent: contentLayout
            ColumnLayout {
                id: contentLayout
                objectName: "dock_tabHost"
                anchors.fill: parent
                spacing: 4
            }
        }
    }

    // ─── Императивный API (C++/JS) ───────────────────────────────────────────

    /// Все вкладки, созданные через appendTab/декларативно (для clearAllTabs).
    /// Динамические (добавленные C++ после старта) хранятся отдельно — clearDynamicTabs() убирает только их.
    property var __dockAllEntries: []
    property var __dockDynamicEntries: []
    /// Декларативные Tab{} из QML уже materialized.
    property bool __dockDeclarativeTabsBuilt: false

    /// Создаёт пару «кнопка + контент-хост» и возвращает хост.
    /// C++ всегда идёт через appendTab: сначала materialize декларативные Tab{}, затем динамика — порядок: программа, потом плагины.
    function appendTab(title) {
        ensureDeclarativeTabsFirst()
        return appendTabImpl(title, true)
    }

    /// markAsDynamic: true — вкладка от плагина (снимается clearDynamicTabs).
    function appendTabImpl(title, markAsDynamic) {
        var bar = getBarForPosition()
        if (!bar)
            return null

        var btn = dockTabBtnComp.createObject(bar, {
            label: title,
            tabWidgetRef: root,
            rotationAngle: (root.tabPosition === TabWidget.Position.Left) ? -90
                          : (root.tabPosition === TabWidget.Position.Right) ? 90 : 0
        })
        if (!btn)
            return null

        if (bar !== topBar && bar !== bottomBar) {
            btn.Layout.alignment = Qt.AlignHCenter | Qt.AlignTop
            btn.Layout.topMargin = (contentArea.count === 0) ? 5 : 2
        } else {
            btn.Layout.alignment = Qt.AlignVCenter
        }

        var hostFrame = dockTabHostComp.createObject(contentArea)
        if (!hostFrame) {
            btn.destroy()
            return null
        }

        var entry = {btn: btn, host: hostFrame}
        __dockAllEntries.push(entry)
        if (markAsDynamic)
            __dockDynamicEntries.push(entry)

        if (contentArea.count === 1) {
            contentArea.currentIndex = 0
            updateActiveState(0)
        }
        return hostFrame.contentParent
    }

    function ensureDeclarativeTabsFirst() {
        if (!tabs || tabs.length === 0)
            return
        if (__dockDeclarativeTabsBuilt)
            return
        rebuildFromDeclarativeTabs()
    }

    /// Снести все вкладки (включая декларативные).
    function clearAllTabs() {
        var arr = __dockAllEntries
        __dockAllEntries = []
        __dockDynamicEntries = []
        __dockDeclarativeTabsBuilt = false
        destroyEntries(arr)
    }

    /// Снести только вкладки, добавленные после старта через C++ appendTab.
    /// Декларативные Tab{}-вкладки остаются.
    function clearDynamicTabs() {
        var arr = __dockDynamicEntries
        __dockDynamicEntries = []
        // Удалим эти же записи и из __dockAllEntries
        var keep = []
        for (var i = 0; i < __dockAllEntries.length; ++i) {
            var e = __dockAllEntries[i]
            var dynamic = false
            for (var k = 0; k < arr.length; ++k) {
                if (arr[k] === e) { dynamic = true; break }
            }
            if (!dynamic)
                keep.push(e)
        }
        __dockAllEntries = keep
        destroyEntries(arr)
    }

    function destroyEntries(arr) {
        // Сначала отвязываем от parent (count в StackLayout уменьшится сразу),
        // только потом destroy() — иначе count «висит» до следующего event-loop.
        for (var i = 0; i < arr.length; ++i) {
            var e = arr[i]
            if (e.btn) e.btn.parent = null
            if (e.host) e.host.parent = null
        }
        for (var j = 0; j < arr.length; ++j) {
            var e2 = arr[j]
            if (e2.btn) e2.btn.destroy()
            if (e2.host) e2.host.destroy()
        }
    }

    // ─── Декларативный путь: создаём вкладки из tabs[] на старте ─────────────

    // Декларативные Tab{} из Main.qml — полный список известен только после инициализации всех детей.
    // Не подписываемся на onTabsChanged: при росте list<Tab> сигнал шёл бы с tabs.length 1,2,3… —
    // первый вызов уже ставил __dockDeclarativeTabsBuilt и «съедал» бы Plugins / Preferences.
    Component.onCompleted: rebuildFromDeclarativeTabs()

    function rebuildFromDeclarativeTabs() {
        if (!tabs || tabs.length === 0)
            return
        // Не полагаться на __dockAllEntries: динамика трекера может вызвать appendTab до onCompleted.
        if (__dockDeclarativeTabsBuilt)
            return
        for (var i = 0; i < tabs.length; ++i) {
            var t = tabs[i]
            if (!t)
                continue
            var host = appendTabImpl(t.title, false)
            if (!host)
                continue
            var contents = t.content
            if (!contents)
                continue
            for (var j = 0; j < contents.length; ++j) {
                var c = contents[j]
                if (c)
                    c.parent = host
            }
        }
        __dockDeclarativeTabsBuilt = true
    }

    // ─── Бары и контент ───────────────────────────────────────────────────────

    RowLayout {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? 30 : 0
        visible: root.tabPosition === TabWidget.Position.Top
        spacing: 2
    }

    RowLayout {
        id: bottomBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? 30 : 0
        visible: root.tabPosition === TabWidget.Position.Bottom
        spacing: 2
    }

    Flickable {
        id: leftBarFlickable
        z: 2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: visible ? 30 : 0
        visible: root.tabPosition === TabWidget.Position.Left
        contentWidth: width
        contentHeight: leftBar.implicitHeight
        interactive: false
        ScrollBar.vertical: Basic.ScrollBar {
            policy: leftBarFlickable.contentHeight > leftBarFlickable.height
                    ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            contentItem: Rectangle {
                implicitWidth: 4
                radius: width / 2
                color: parent.pressed ? "#ffaa33" : "#D48912"
            }
        }

        WheelHandler {
            onWheel: function(event) {
                leftBarFlickable.contentY = Math.max(0,
                    Math.min(leftBarFlickable.contentHeight - leftBarFlickable.height,
                             leftBarFlickable.contentY - event.angleDelta.y / 3))
                event.accepted = true
            }
        }

        ColumnLayout {
            id: leftBar
            width: parent.width
            spacing: 2
        }
    }

    Flickable {
        id: rightBarFlickable
        z: 2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: visible ? 30 : 0
        visible: root.tabPosition === TabWidget.Position.Right
        contentWidth: width
        contentHeight: rightBar.implicitHeight
        interactive: false
        ScrollBar.vertical: Basic.ScrollBar {
            policy: rightBarFlickable.contentHeight > rightBarFlickable.height
                    ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            contentItem: Rectangle {
                implicitWidth: 4
                radius: width / 2
                color: parent.pressed ? "#ffaa33" : "#D48912"
            }
        }

        WheelHandler {
            onWheel: function(event) {
                rightBarFlickable.contentY = Math.max(0,
                    Math.min(rightBarFlickable.contentHeight - rightBarFlickable.height,
                             rightBarFlickable.contentY - event.angleDelta.y / 3))
                event.accepted = true
            }
        }

        ColumnLayout {
            id: rightBar
            width: parent.width
            spacing: 2
        }
    }

    Flickable {
        id: contentFlickable
        z: -1
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        anchors.left: leftBarFlickable.right
        anchors.right: rightBarFlickable.left

        clip: true
        contentWidth: width
        contentHeight: Math.max(contentArea.implicitHeight, height)
        interactive: enablePageScrolling

        StackLayout {
            id: contentArea
            width: contentFlickable.width
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.rightMargin: 10
            height: Math.max(implicitHeight, contentFlickable.height)
        }

        ScrollBar.vertical: Basic.ScrollBar {
            id: pageVerticalScrollBar
            policy: enablePageScrolling ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            interactive: true
            padding: 1

            background: Rectangle {
                implicitWidth: 7
                color: "#191919"
                radius: 3
            }

            contentItem: Rectangle {
                implicitWidth: 5
                radius: width / 2
                color: pageVerticalScrollBar.pressed ? "#ffaa33" : "#D48912"
            }
        }
    }

    // ─── Утилиты ──────────────────────────────────────────────────────────────

    function dockItemIsTabStripButton(item) {
        if (!item)
            return false
        if (item.objectName === "dock_tabStripBtn")
            return true
        return item.dockTabStripButton === true
    }

    function stripTabButtonsFromBar(bar) {
        if (!bar)
            return
        var dead = []
        for (var i = 0; i < bar.children.length; ++i) {
            var ch = bar.children[i]
            if (dockItemIsTabStripButton(ch))
                dead.push(ch)
        }
        for (var j = 0; j < dead.length; ++j)
            dead[j].destroy()
    }

    function getBarForPosition() {
        switch (root.tabPosition) {
        case TabWidget.Position.Top:    return topBar
        case TabWidget.Position.Bottom: return bottomBar
        case TabWidget.Position.Left:   return leftBar
        case TabWidget.Position.Right:  return rightBar
        default:                        return topBar
        }
    }

    function activateTab(clickedBtn) {
        var bar = getBarForPosition()
        var idx = -1
        var btnRun = 0
        for (var i = 0; i < bar.children.length; ++i) {
            var ch = bar.children[i]
            if (dockItemIsTabStripButton(ch)) {
                if (ch === clickedBtn) {
                    idx = btnRun
                    break
                }
                btnRun++
            }
        }
        if (idx >= 0 && idx < contentArea.count)
            contentArea.currentIndex = idx
    }

    function updateActiveState(activeIndex) {
        var bar = getBarForPosition()
        var btnRun = 0
        for (var i = 0; i < bar.children.length; ++i) {
            var child = bar.children[i]
            if (dockItemIsTabStripButton(child)) {
                child.active = (btnRun === activeIndex)
                btnRun++
            }
        }
    }

    Connections {
        target: contentArea
        function onCurrentIndexChanged() {
            updateActiveState(contentArea.currentIndex)
        }
    }
}
