import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: shellPage
    property var theme: ApplicationWindow.window ? ApplicationWindow.window.theme : null
    
    background: Rectangle { color: theme ? theme.background : "#ffffff" }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "☰"
                onClicked: drawer.open()
            }
            Label {
                text: viewStack.currentItem ? viewStack.currentItem.title : "期货云哨兵"
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
                    MenuItem { text: "关于" }
                }
            }
        }
    }

    Drawer {
        id: drawer
        width: Math.min(shellPage.width * 0.66, 250)
        height: shellPage.height
        
        background: Rectangle {
            color: theme ? theme.surface : "#f5f5f5"
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: theme ? theme.colorOutline : "#cccccc"
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
                    color: theme ? theme.surface : "#f5f5f5"
                    
                    // Decorative strip on the left
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 4
                        color: theme ? theme.primary : "#0078d4"
                    }

                    // Subtle bottom border
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: theme ? theme.colorOutline : "#cccccc"
                        opacity: 0.1
                    }
                    
                    Column {
                        anchors.centerIn: parent
                        spacing: 10
                        Label {
                            text: "用户: " + (backend && backend.username ? backend.username : "游客")
                            color: theme ? theme.colorOnSurface : "#000000"
                            font.bold: true
                            font.pixelSize: 18
                        }
                        Label {
                            text: (backend && backend.ctpConnected) ? "行情已连接" : "行情未连接"
                            color: (backend && backend.ctpConnected) ? "#89D185" : "#F14C4C"
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
                    ListElement { title: "预警监控"; source: "AlarmPage.qml" }
                    ListElement { title: "日志"; source: "LogPage.qml" }
                    ListElement { title: "设置"; source: "SettingsPage.qml" }
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
