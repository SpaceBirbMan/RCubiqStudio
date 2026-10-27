import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: 1

    function closeOthers(exceptItem) {

        if (!exceptItem.extended)
            return

        for (var i = 0; i < root.children.length; ++i) {
            var child = root.children[i]
            if (child !== exceptItem && child.hasOwnProperty("extended")) {
                child.extended = false
            }
        }
    }
}
