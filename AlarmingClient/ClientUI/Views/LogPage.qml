import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "系统日志"
    property var theme: ApplicationWindow.window ? ApplicationWindow.window.theme : null

    background: Rectangle { color: theme ? theme.background : "#ffffff" }

    ListView {
        id: logListView
        anchors.fill: parent
        clip: true
        model: ListModel {
            id: logModel
            // ListElement { time: "10:00:01"; level: "INFO"; message: "System started" }
        }

        Connections {
            target: backend
            function onLogReceived(time, level, message) {
                logModel.append({ "time": time, "level": level, "message": message })
                logListView.positionViewAtEnd()
            }
        }

        delegate: ItemDelegate {
            width: ListView.view.width
            contentItem: RowLayout {
                Label { text: model.time; font.family: "Consolas"; color: "gray" }
                Label { 
                    text: "[" + model.level + "]"; 
                    font.family: "Consolas"; 
                    font.bold: true
                    color: model.level === "ERROR" ? "red" : (model.level === "WARN" ? "orange" : "green")
                }
                Label { text: model.message; Layout.fillWidth: true; elide: Text.ElideRight }
            }
        }
        
        ScrollIndicator.vertical: ScrollIndicator { }
    }
}
