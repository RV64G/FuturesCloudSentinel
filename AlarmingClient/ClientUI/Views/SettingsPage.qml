import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Settings"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Label { text: "Appearance"; font.bold: true; font.pixelSize: 16 }
        
        RowLayout {
            Label { text: "Theme Color" }
            ComboBox {
                model: ["System", "Light", "Dark"]
            }
        }

        RowLayout {
            Label { text: "Accent Color" }
            Rectangle { width: 20; height: 20; color: "#0078d4"; radius: 10 }
            Rectangle { width: 20; height: 20; color: "#d83b01"; radius: 10 }
            Rectangle { width: 20; height: 20; color: "#107c10"; radius: 10 }
        }

        MenuSeparator { Layout.fillWidth: true }

        Label { text: "Notification"; font.bold: true; font.pixelSize: 16 }
        
        RowLayout {
            Label { text: "Email Address:" }
            TextField { 
                id: emailField
                placeholderText: "alert@example.com"
                Layout.fillWidth: true 
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

        Item { Layout.fillHeight: true }
    }
}
