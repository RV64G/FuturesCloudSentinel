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

    background: Rectangle {
        color: popup.type === "error" ? "#ffdddd" : (popup.type === "success" ? "#ddffdd" : "#f0f0f0")
        border.color: popup.type === "error" ? "red" : (popup.type === "success" ? "green" : "gray")
        radius: 5
    }
    
    contentItem: RowLayout {
        spacing: 10
        Label {
            id: iconLabel
            text: popup.type === "error" ? "❌" : (popup.type === "success" ? "✅" : "ℹ️")
            font.pixelSize: 20
        }
        Label {
            id: messageLabel
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "black"
        }
    }
}
