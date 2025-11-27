import QtQuick
import QtQuick.Controls

Popup {
    id: popup
    width: 300
    height: 100
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property alias text: messageLabel.text
    property string type: "info" // info, error, success

    background: Rectangle {
        color: popup.type === "error" ? "#ffcccc" : (popup.type === "success" ? "#ccffcc" : "#ffffff")
        border.color: "#cccccc"
        radius: 5
    }

    Label {
        id: messageLabel
        anchors.centerIn: parent
        wrapMode: Text.WordWrap
        width: parent.width - 20
        horizontalAlignment: Text.AlignHCenter
        color: "#000000" // Ensure text is visible on light background
    }
    
    function showMessage(msg, msgType) {
        text = msg
        type = msgType || "info"
        open()
    }
}
