import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: shellPage
    property var theme: ApplicationWindow.window.theme
    
    background: Rectangle { color: theme.background }

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
        
        background: Rectangle {
            color: theme.surface
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: theme.colorOutline
                opacity: 0.2
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                Rectangle {
                    anchors.fill: parent
                    color: theme.primary
                    Column {
                        anchors.centerIn: parent
                        spacing: 10
                        Label {
                            text: "User: " + (backend.username ? backend.username : "Guest")
                            color: theme.colorOnPrimary
                            font.bold: true
                        }
                        Label {
                            text: "Connected"
                            color: theme.colorOnPrimary
                            opacity: 0.8
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
                        // Update the ListView's current index to match the click
                        navList.currentIndex = index
                        // StackLayout is bound to navList.currentIndex, so it updates automatically
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
