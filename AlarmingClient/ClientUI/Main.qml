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
    title: "期货云哨兵"
    
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
            // 如果是预警触发消息，弹出模态对话框
            if (message.indexOf("预警触发") !== -1) {
                alertDialog.text = message
                alertDialog.open()
            } else {
                // 其他消息用 tips 提示
                if (tips) tips.showMessage(message, "info")
            }
        }
    }

    Dialog {
        id: alertDialog
        title: "⚠️ 预警触发"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        property alias text: msgLabel.text
        
        Label {
            id: msgLabel
            text: ""
            font.pixelSize: 16
            color: "#F14C4C"
            font.bold: true
        }
    }
}
