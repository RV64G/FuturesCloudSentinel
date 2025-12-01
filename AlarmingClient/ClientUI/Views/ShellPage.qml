import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: shellPage
    
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "☰"
                onClicked: drawer.open()
            }
            Label {
                text: viewStack.currentItem ? viewStack.currentItem.title : "Futures Alarming"
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            ToolButton {
                text: "⋮"
                onClicked: menu.open()
                Menu {
                    id: menu
                    MenuItem { text: "About" }
                }
            }
        }
    }

    Drawer {
        id: drawer
        width: Math.min(shellPage.width * 0.66, 250)
        height: shellPage.height

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                Rectangle {
                    anchors.fill: parent
                    color: "#0078d4"
                    Column {
                        anchors.centerIn: parent
                        spacing: 10
                        Label {
                            text: "User: " + (backend.username ? backend.username : "Guest")
                            color: "white"
                            font.bold: true
                        }
                        Label {
                            text: "Connected"
                            color: "#e0e0e0"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            ListView {
                id: navList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ListModel {
                    ListElement { title: "Alarm Monitor"; source: "AlarmPage.qml" }
                    ListElement { title: "Logs"; source: "LogPage.qml" }
                    ListElement { title: "Settings"; source: "SettingsPage.qml" }
                }
                delegate: ItemDelegate {
                    text: model.title
                    width: parent.width
                    highlighted: ListView.isCurrentItem
                    onClicked: {
                        viewStack.currentIndex = index
                        drawer.close()
                    }
                }
            }
        }
    }

    StackLayout {
        id: viewStack
        anchors.fill: parent
        currentIndex: navList.currentIndex

        AlarmPage {
            id: alarmPage
        }

        LogPage {
            id: logPage
        }

        SettingsPage {
            id: settingsPage
        }
    }
}
