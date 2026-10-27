import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// Вертикальный скролл контента без полосы прокрутки (полоса вкладки — у внешнего TabWidget при enablePageScrolling).
Flickable {
    id: fl
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 32

    contentWidth: width
    contentHeight: inner.implicitHeight
    clip: true

    ColumnLayout {
        id: inner
        objectName: "rui_scrollInner"
        width: fl.width
        spacing: 8
    }

    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AlwaysOff
        visible: false
    }
}
