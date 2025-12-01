import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Alarm Monitor"

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left: Alarm List
        ListView {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 200
            clip: true
            
            model: ListModel {
                ListElement { instrument: "au2306"; price: "450.5"; status: "Active" }
                ListElement { instrument: "ag2306"; price: "5600"; status: "Triggered" }
                ListElement { instrument: "rb2310"; price: "3800"; status: "Inactive" }
            }

            delegate: ItemDelegate {
                width: parent.width
                contentItem: ColumnLayout {
                    RowLayout {
                        Label { text: model.instrument; font.bold: true; Layout.fillWidth: true }
                        Label { text: model.price; color: "red" }
                    }
                    Label { text: model.status; font.pixelSize: 12; color: "gray" }
                }
                highlighted: ListView.isCurrentItem
                onClicked: ListView.view.currentIndex = index
            }
            
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        // Right: Detail/Edit Panel
        Rectangle {
            SplitView.fillWidth: true
            color: palette.window // Use system palette for adaptive background
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                Label { 
                    text: "Edit Alarm" 
                    font.pixelSize: 20 
                    font.bold: true 
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 10

                    Label { text: "Instrument:" }
                    ComboBox { 
                        model: backend.contractCodes 
                        editable: true
                        Layout.fillWidth: true 
                    }

                    Label { text: "Condition:" }
                    ComboBox {
                        model: ["Price >=", "Price <=", "Volume >"]
                        Layout.fillWidth: true
                    }

                    Label { text: "Threshold:" }
                    TextField { placeholderText: "Price"; Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "Save"; highlighted: true }
                    Button { text: "Delete"; flat: true; palette.buttonText: "red" }
                    Item { Layout.fillWidth: true }
                }
                
                Item { Layout.fillHeight: true } // Spacer
            }
        }
    }
}
