import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    width: 300
    height: 60
    modal: false 
    focus: false 
    dim: false // Disable background dimming
    
    // Position at bottom-right corner
    parent: Overlay.overlay
    x: parent.width - width - 20
    y: parent.height - height - 20
    
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property alias text: messageLabel.text
    property string type: "info" // info, error, success
    property var theme: null

    function showMessage(msg, msgType) {
        text = msg
        type = msgType || "info"
        open()
    }

    // Auto-close timer
    Timer {
        id: closeTimer
        interval: 3000 // 3 seconds
        repeat: false
        onTriggered: popup.close()
    }

    onOpened: {
        closeTimer.restart()
    }

    // Helper to get background color based on type
    function getBackgroundColor() {
        if (!theme) return popup.type === "error" ? "#ffdddd" : (popup.type === "success" ? "#ddffdd" : "#f0f0f0")
        switch (type) {
            case "error": return theme.colorErrorContainer
            case "success": return theme.colorSuccessContainer
            default: return theme.surfaceVariant // or primaryContainer
        }
    }

    function getContentColor() {
        if (!theme) return "#000000"
        switch (type) {
            case "error": return theme.colorOnErrorContainer
            case "success": return theme.colorOnSuccessContainer
            default: return theme.colorOnSurfaceVariant
        }
    }

    background: Rectangle {
        color: getBackgroundColor()
        border.color: "transparent" // Monet usually doesn't use borders for toasts, just color
        radius: 5
    }
    
    contentItem: RowLayout {
        spacing: 10
        Label {
            id: iconLabel
            text: popup.type === "error" ? "❌" : (popup.type === "success" ? "✅" : "ℹ️")
            color: getContentColor()
            font.pixelSize: 20
        }
        Label {
            id: messageLabel
            text: "Message"
            color: getContentColor()
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
    }
}
