import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Settings"
    property var theme: ApplicationWindow.window.theme

    background: Rectangle { color: theme.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label { text: "Appearance"; font.bold: true; font.pixelSize: 16 }
        
        RowLayout {
            Label { text: "Theme Mode" }
            Switch {
                text: checked ? "Dark" : "Light"
                checked: theme.isDark
                onCheckedChanged: theme.isDark = checked
            }
        }

        RowLayout {
            Label { text: "Accent Color" }
            Repeater {
                // Use a fixed list for now to avoid SystemPalette issues in Repeater model
                model: ["#0078d4", "#d83b01", "#107c10", "#6200EE", "#66ccff"]
                delegate: Rectangle {
                    width: 24; height: 24; radius: 12
                    color: modelData
                    border.width: theme.seedColor == modelData ? 2 : 0
                    border.color: theme.colorOnBackground
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: theme.setSeedColor(modelData)
                    }
                }
            }
        }
        
        // SystemPalette { id: sysPal; colorGroup: SystemPalette.Active } // Removed to avoid issues

        MenuSeparator { Layout.fillWidth: true }

        Label { text: "Notification"; font.bold: true; font.pixelSize: 16 }
        
        RowLayout {
            Label { text: "Email Address:" }
            TextField { 
                id: emailField
                placeholderText: "alert@example.com"
                Layout.fillWidth: true 
                Component.onCompleted: text = backend.getSavedEmail()
            }
            Button {
                text: "Set"
                onClicked: backend.setEmail(emailField.text)
            }
        }

        MenuSeparator { Layout.fillWidth: true }

        Label { text: "Network"; font.bold: true; font.pixelSize: 16 }

        CheckBox {
            text: "Auto-connect on startup"
            checked: true
        }

        MenuSeparator { Layout.fillWidth: true }

        Button {
            text: "Logout"
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
