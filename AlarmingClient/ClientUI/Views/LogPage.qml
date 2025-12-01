import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "System Logs"

    ListView {
        anchors.fill: parent
        clip: true
        model: ListModel {
            ListElement { time: "10:00:01"; level: "INFO"; message: "System started" }
            ListElement { time: "10:00:02"; level: "INFO"; message: "Connected to CTP Front" }
            ListElement { time: "10:00:05"; level: "WARN"; message: "Market data delay detected" }
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
