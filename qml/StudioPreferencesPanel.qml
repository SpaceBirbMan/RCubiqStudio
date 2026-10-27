// Хост под встраивание preferencesTab из mainwindow.ui (QScrollArea + макет из .ui).
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    objectName: "studio_embedPreferencesPanelHost"
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumHeight: 120
}
