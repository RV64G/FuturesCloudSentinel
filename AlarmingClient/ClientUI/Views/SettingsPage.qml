import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "设置"
    property var theme: ApplicationWindow.window ? ApplicationWindow.window.theme : null

    background: Rectangle { color: theme ? theme.background : "#ffffff" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label { text: "外观"; font.bold: true; font.pixelSize: 16 }
        
        RowLayout {
            Label { text: "主题模式" }
            Switch {
                text: checked ? "深色" : "浅色"
                checked: theme ? theme.isDark : false
                onCheckedChanged: if (theme) theme.isDark = checked
            }
        }

        RowLayout {
            Label { text: "强调色" }
            Repeater {
                // Use a fixed list for now to avoid SystemPalette issues in Repeater model
                model: ["#0078d4", "#d83b01", "#107c10", "#6200EE", "#66ccff"]
                delegate: Rectangle {
                    width: 24; height: 24; radius: 12
                    color: modelData
                    border.width: (theme && theme.seedColor == modelData) ? 2 : 0
                    border.color: theme ? theme.colorOnBackground : "#000000"
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (theme) theme.setSeedColor(modelData)
                    }
                }
            }
        }
        
        // SystemPalette { id: sysPal; colorGroup: SystemPalette.Active } // Removed to avoid issues

        MenuSeparator { Layout.fillWidth: true }

        Label { text: "通知"; font.bold: true; font.pixelSize: 16 }
        
        RowLayout {
            Label { text: "邮箱地址:" }
            TextField { 
                id: emailField
                placeholderText: "alert@example.com"
                Layout.fillWidth: true 
                Component.onCompleted: text = backend.getSavedEmail()
            }
            Button {
                text: "设置"
                onClicked: backend.setEmail(emailField.text)
            }
        }

        MenuSeparator { Layout.fillWidth: true }

        Label { text: "网络"; font.bold: true; font.pixelSize: 16 }

        CheckBox {
            text: "启动时自动连接"
            checked: true
        }

        MenuSeparator { Layout.fillWidth: true }

        Button {
            text: "退出登录"
            Layout.alignment: Qt.AlignRight
            
            contentItem: Text {
                text: parent.text
                font: parent.font
                opacity: enabled ? 1.0 : 0.3
                color: "#F14C4C" // VS Code Red
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            onClicked: {
                backend.clearCredentials()
                if (ApplicationWindow.window && ApplicationWindow.window.logout) {
                    ApplicationWindow.window.logout()
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
