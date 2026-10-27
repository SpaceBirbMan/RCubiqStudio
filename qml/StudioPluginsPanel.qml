// Хост под встраивание вкладки pluginsTab из mainwindow.ui (тот же QWidget, кнопки + QToolBox).
// Декларативная копия убрана — источник правды один, без расхождения с C++.
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    objectName: "studio_embedPluginsPanelHost"
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 160
}
