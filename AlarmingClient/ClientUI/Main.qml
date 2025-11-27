import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "Views"
import "Components"

ApplicationWindow {
    id: window
    visible: true
    width: 1024
    height: 768
    title: "Futures Alarming Client"

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
        anchors.centerIn: parent
    }
}
