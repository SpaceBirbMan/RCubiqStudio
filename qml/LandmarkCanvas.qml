import QtQuick

Item {
    id: root
    width: parent ? parent.width : 0
    height: 240
    property var pointsData: []

    function setPoints(points) {
        pointsData = points
        plot.requestPaint()
    }

    Rectangle {
        anchors.fill: parent
        color: "#111111"
        border.color: "#513405"
        radius: 4
    }

    Canvas {
        id: plot
        anchors.fill: parent
        anchors.margins: 6
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#111111"
            ctx.fillRect(0, 0, width, height)

            if (!root.pointsData || root.pointsData.length === 0)
                return

            var minX = root.pointsData[0].x
            var maxX = root.pointsData[0].x
            var minY = root.pointsData[0].y
            var maxY = root.pointsData[0].y

            for (var i = 1; i < root.pointsData.length; ++i) {
                var p = root.pointsData[i]
                if (p.x < minX) minX = p.x
                if (p.x > maxX) maxX = p.x
                if (p.y < minY) minY = p.y
                if (p.y > maxY) maxY = p.y
            }

            var rangeX = Math.max(1.0, maxX - minX)
            var rangeY = Math.max(1.0, maxY - minY)
            var margin = 12
            var drawW = Math.max(1, width - margin * 2)
            var drawH = Math.max(1, height - margin * 2)

            for (var j = 0; j < root.pointsData.length; ++j) {
                var pt = root.pointsData[j]
                var nx = (pt.x - minX) / rangeX
                var ny = (pt.y - minY) / rangeY
                var px = margin + nx * drawW
                var py = margin + ny * drawH
                var rr = Math.max(1.0, pt.size || 2.0)
                ctx.beginPath()
                ctx.fillStyle = pt.color || "#ff0000"
                ctx.arc(px, py, rr, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }
}
