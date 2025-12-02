import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "Views"
import "Components"

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 600
    title: "Futures Alarming Client"
    
    // Global Font Settings
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 14

    property alias tips: tips
    property alias theme: themeManager

    SystemPalette {
        id: activePalette
        colorGroup: SystemPalette.Active
    }

    ThemeManager {
        id: themeManager
        seedColor: activePalette.highlight
        // Detect system dark mode by checking window background brightness
        isDark: (activePalette.window.r * 0.2126 + activePalette.window.g * 0.7152 + activePalette.window.b * 0.0722) < 0.5
    }
    
    color: themeManager.background // Explicitly bind window background color

    // Apply Theme to Global Palette
    palette.accent: themeManager.primary
    palette.window: themeManager.background
    palette.windowText: themeManager.colorOnBackground
    palette.base: themeManager.surface
    palette.text: themeManager.colorOnSurface
    palette.button: themeManager.surfaceVariant
    palette.buttonText: themeManager.colorOnSurfaceVariant
    palette.highlight: themeManager.primary
    palette.highlightedText: themeManager.colorOnPrimary

    function logout() {
        stackView.pop(null) // Pop all
        stackView.push("Views/LoginPage.qml")
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: LoginPage {
            onLoginSuccess: stackView.push("Views/ShellPage.qml")
        }
    }

    TipsPopup {
        id: tips
        theme: themeManager
        // anchors.centerIn: parent // Removed to allow TipsPopup to position itself
    }

    Connections {
        target: backend
        function onShowMessage(message) {
            if (tips) tips.showMessage(message, "info")
            
            // If it's an alert, show a modal dialog or stronger notification
            // Simple heuristic: if message contains "Alert" or "Triggered"
            if (message.indexOf("Alert") !== -1 || message.indexOf("Triggered") !== -1) {
                alertDialog.text = message
                alertDialog.open()
            }
        }
    }

    Dialog {
        id: alertDialog
        title: "⚠️ Warning Triggered"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        property alias text: msgLabel.text
        
        Label {
            id: msgLabel
            text: "Warning!"
            font.pixelSize: 16
            color: "#F14C4C" // VS Code Red
            font.bold: true
        }
    }
}
