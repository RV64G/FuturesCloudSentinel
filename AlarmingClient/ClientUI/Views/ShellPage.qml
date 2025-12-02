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
                    color: theme.surface // Clean surface color
                    
                    // Decorative strip on the left
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 4
                        color: theme.primary
                    }

                    // Subtle bottom border
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: theme.colorOutline
                        opacity: 0.1
                    }
                    
                    Column {
                        anchors.centerIn: parent
                        spacing: 10
                        Label {
                            text: "User: " + (backend.username ? backend.username : "Guest")
                            color: theme.colorOnSurface 
                            font.bold: true
                            font.pixelSize: 18
                        }
                        Label {
                            text: "Connected"
                            color: theme.primary // Use primary color for status to echo the strip
                            opacity: 0.9
                            font.pixelSize: 12
                            font.bold: true
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
