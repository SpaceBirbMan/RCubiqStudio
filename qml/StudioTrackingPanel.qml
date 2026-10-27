// Хост под встраивание bindsTab из mainwindow.ui (скролл + QToolBox + tableTrackerWidget).
// Те же QWidget, что в .ui; динамические вкладки трекеров добавляются к studio_propertiesTabWidget отдельно.
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    objectName: "studio_embedTrackingBindsHost"
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 160
}
