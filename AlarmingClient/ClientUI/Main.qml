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

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: LoginPage {
            onLoginSuccess: stackView.push("Views/ShellPage.qml")
        }
    }

    TipsPopup {
        id: tips
        // anchors.centerIn: parent // Removed to allow TipsPopup to position itself
    }

    Connections {
        target: backend
        function onShowMessage(message) {
            if (tips) tips.showMessage(message, "info")
        }
    }
}
