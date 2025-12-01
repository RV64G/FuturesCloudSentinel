import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: loginPage
    title: "Login"

    signal loginSuccess()

    property var tips: ApplicationWindow.window ? ApplicationWindow.window.tips : null
    property bool isBusy: false

    ColumnLayout {
        anchors.centerIn: parent
        width: 300
        spacing: 15
        enabled: !isBusy

        Label {
            text: "Futures Alarming"
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 20
        }

        TextField {
            id: brokerIdField
            placeholderText: "Broker ID"
            Layout.fillWidth: true
            text: "9999"
        }

        TextField {
            id: userIdField
            placeholderText: "User ID"
            Layout.fillWidth: true
        }

        TextField {
            id: passwordField
            placeholderText: "Password"
            echoMode: TextInput.Password
            Layout.fillWidth: true
        }

        // Front address removed: server connection address is no longer user-input.

        Button {
            text: "Login"
            highlighted: true
            Layout.fillWidth: true
            Layout.topMargin: 10
            onClicked: {
                if (userIdField.text === "" || passwordField.text === "") {
                    if (tips) tips.showMessage("Please enter username and password", "error")
                    return
                }
                loginPage.isBusy = true
                backend.login(userIdField.text, passwordField.text, brokerIdField.text, loginPage.defaultFrontAddr)
            }
        }

        Button {
            text: "Register"
            flat: true
            Layout.fillWidth: true
            onClicked: {
                registerPopup.open()
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: loginPage.isBusy
        visible: running
    }

    Popup {
        id: registerPopup
        width: 300
        height: 350
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: ApplicationWindow.window ? ApplicationWindow.window.color : "#ffffff"
            border.color: "#cccccc"
            radius: 5
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 40
            spacing: 15

            Label {
                text: "Create Account"
                font.pixelSize: 20
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            TextField {
                id: regUsernameField
                placeholderText: "Username"
                Layout.fillWidth: true
            }

            TextField {
                id: regPasswordField
                placeholderText: "Password"
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }
            
            TextField {
                id: regConfirmPasswordField
                placeholderText: "Confirm Password"
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }

            Button {
                text: "Register"
                highlighted: true
                Layout.fillWidth: true
                Layout.topMargin: 10
                onClicked: {
                    if (regPasswordField.text !== regConfirmPasswordField.text) {
                        if (tips) tips.showMessage("Passwords do not match", "error")
                        return
                    }
                    if (regUsernameField.text === "" || regPasswordField.text === "") {
                        if (tips) tips.showMessage("Please fill all fields", "error")
                        return
                    }
                    
                    backend.registerUser(regUsernameField.text, regPasswordField.text)
                }
            }
        }
    }

    Connections {
        target: backend
        function onLoginSuccess() {
            loginPage.isBusy = false
            if (tips) tips.showMessage("Login successful!", "success")
            loginPage.loginSuccess()
        }
        function onLoginFailed(message) {
            loginPage.isBusy = false
            if (tips) tips.showMessage(message, "error")
        }
        function onRegisterSuccess() {
            if (tips) tips.showMessage("Registration successful!", "success")
            registerPopup.close()
        }
        function onRegisterFailed(message) {
            if (tips) tips.showMessage(message, "error")
        }
    }
}
